/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
#include "tr_cmds.hpp"
#include "tr_image.hpp"
#include "tr_backend.hpp"
#include "tr_local.hpp"
#include "tr_shader.hpp"
#include "tr_scene.hpp"
#include "vk.hpp"
#include "vk_pipeline.hpp"
#include "utils.hpp"

#include <condition_variable>
#include <mutex>
#include <thread>

constexpr int MODE_RED_CYAN = 1;
constexpr int MODE_RED_BLUE = 2;
constexpr int MODE_RED_GREEN = 3;
constexpr int MODE_GREEN_MAGENTA = 4;
constexpr int MODE_MAX = MODE_GREEN_MAGENTA;
/*
============================================================
Asynchronous render back end
============================================================

The front end owns backEndData while building a frame.  Once submitted, that
entire buffer becomes immutable and is owned by the render thread until it
signals completion.  Only one frame is allowed in flight, so latency is capped
at one frame and two backEndData buffers are sufficient.
*/
static std::thread s_renderThread;
static std::mutex s_renderThreadMutex;
static std::condition_variable s_renderThreadCv;
static bool s_renderThreadActive;
static bool s_renderThreadExit;
static bool s_renderThreadBusy;
static backEndData_t *s_renderThreadData;
static backEndData_t *s_completedRenderData;
static backEndCounters_t s_completedBackEndPc;

static void R_PrepareBackEndFrame(const backEndData_t &data)
{
	Com_Memset(&backEnd.pc, 0, sizeof(backEnd.pc));

	backEnd.frameCount = data.frame.frameCount;
	backEnd.doneBloom = false;
	backEnd.color2D.u32 = ~0U;

	backEnd.entity2D.local = &backEnd.entity2DLocal;
	backEnd.entity2D.flags = 0;
	Com_Memset(&backEnd.entity2DLocal, 0, sizeof(backEnd.entity2DLocal));

	backEnd.worldEntity.local = &backEnd.worldEntityLocal;
	backEnd.worldEntity.flags = 0;
	Com_Memset(&backEnd.worldEntityLocal, 0, sizeof(backEnd.worldEntityLocal));

	backEnd.screenshotMask = data.frame.screenshotMask;
	Q_strncpyz(backEnd.screenshotTGA, data.frame.screenshotTGA, sizeof(backEnd.screenshotTGA));
	Q_strncpyz(backEnd.screenshotJPG, data.frame.screenshotJPG, sizeof(backEnd.screenshotJPG));
	Q_strncpyz(backEnd.screenshotBMP, data.frame.screenshotBMP, sizeof(backEnd.screenshotBMP));
	backEnd.screenShotTGAsilent = data.frame.screenShotTGAsilent;
	backEnd.screenShotJPGsilent = data.frame.screenShotJPGsilent;
	backEnd.screenShotBMPsilent = data.frame.screenShotBMPsilent;
	backEnd.vcmd = data.frame.vcmd;
}

static void R_ExecuteBackEndFrame(backEndData_t &data)
{
	R_PrepareBackEndFrame(data);
	RB_ExecuteRenderCommands(data.commands.cmds);
}

static void R_RenderThreadMain()
{
	for (;;)
	{
		backEndData_t *data = nullptr;
		{
			std::unique_lock<std::mutex> lock(s_renderThreadMutex);
			s_renderThreadCv.wait(lock, [] { return s_renderThreadBusy || s_renderThreadExit; });

			if (s_renderThreadExit && !s_renderThreadBusy)
				break;

			data = s_renderThreadData;
		}

		R_ExecuteBackEndFrame(*data);

		{
			std::lock_guard<std::mutex> lock(s_renderThreadMutex);
			s_completedRenderData = data;
			s_completedBackEndPc = backEnd.pc;
			s_renderThreadData = nullptr;
			s_renderThreadBusy = false;
		}
		s_renderThreadCv.notify_all();
	}
}

void R_InitRenderThread()
{
	if (s_renderThreadActive)
	{
		// R_Init can run again during a registration/map transition while the
		// worker itself stays alive.  The old completed buffer belongs to the
		// previous hunk generation and must never be consumed by the new frame.
		std::lock_guard<std::mutex> lock(s_renderThreadMutex);
		assert(!s_renderThreadBusy);
		s_completedRenderData = nullptr;
		Com_Memset(&s_completedBackEndPc, 0, sizeof(s_completedBackEndPc));
		return;
	}

	if (!r_smp || !r_smp->integer || backEndDataBuffers[0] == backEndDataBuffers[1])
		return;

	s_renderThreadExit = false;
	s_renderThreadBusy = false;
	s_renderThreadData = nullptr;
	s_completedRenderData = nullptr;
	Com_Memset(&s_completedBackEndPc, 0, sizeof(s_completedBackEndPc));

	s_renderThread = std::thread(R_RenderThreadMain);
	s_renderThreadActive = true;
	ri.Printf(PRINT_ALL, "Render thread: enabled (one frame in flight)\n");
}

bool R_RenderThreadActive()
{
	return s_renderThreadActive;
}

void R_SyncRenderThread()
{
	if (!s_renderThreadActive)
		return;

	std::unique_lock<std::mutex> lock(s_renderThreadMutex);
	s_renderThreadCv.wait(lock, [] { return !s_renderThreadBusy; });
}

void R_ShutdownRenderThread()
{
	if (!s_renderThreadActive)
		return;

	R_SyncRenderThread();
	{
		std::lock_guard<std::mutex> lock(s_renderThreadMutex);
		s_renderThreadExit = true;
	}
	s_renderThreadCv.notify_all();
	s_renderThread.join();

	s_renderThreadActive = false;
	s_renderThreadExit = false;
	s_renderThreadData = nullptr;
	s_completedRenderData = nullptr;
	ri.Printf(PRINT_ALL, "Render thread: disabled\n");
}

static void R_SubmitRenderFrame(backEndData_t &data)
{
	std::lock_guard<std::mutex> lock(s_renderThreadMutex);
	assert(!s_renderThreadBusy);
	s_renderThreadData = &data;
	s_renderThreadBusy = true;
	s_renderThreadCv.notify_one();
}

static bool R_ConsumeCompletedRenderFrame(backEndData_t *&data, backEndCounters_t &pc)
{
	data = nullptr;
	Com_Memset(&pc, 0, sizeof(pc));

	if (!s_renderThreadActive)
		return false;

	R_SyncRenderThread();

	std::lock_guard<std::mutex> lock(s_renderThreadMutex);
	if (!s_completedRenderData)
		return false;

	data = s_completedRenderData;
	pc = s_completedBackEndPc;
	s_completedRenderData = nullptr;
	return true;
}

static void R_RotateFrontEndDataBuffer()
{
	assert(s_renderThreadActive);
	assert(backEndDataBuffers[0] != backEndDataBuffers[1]);
	backEndData = (backEndData == backEndDataBuffers[0]) ? backEndDataBuffers[1] : backEndDataBuffers[0];
}

/*
=====================
R_PerformanceCounters
=====================
*/
static void R_PerformanceCounters(const renderFrameState_t &frame, const backEndCounters_t &backendPc)
{
	if (!r_speeds->integer)
		return;

	const frontEndCounters_t &frontendPc = frame.frontEndPc;

	if (r_speeds->integer == 1) {
		ri.Printf (PRINT_ALL, "%i/%i shaders/surfs %i leafs %i verts %i/%i tris %.2f mtex\n",
			backendPc.c_shaders, backendPc.c_surfaces, frontendPc.c_leafs, backendPc.c_vertexes,
			backendPc.c_indexes/3, backendPc.c_totalIndexes/3, R_SumOfUsedImages(frame.frameCount)/1000000.0);
	}
	else if (r_speeds->integer == 2) {
		ri.Printf(PRINT_ALL, "(patch) %i sin %i sclip  %i sout %i bin %i bclip %i bout\n",
				  frontendPc.c_sphere_cull_patch_in, frontendPc.c_sphere_cull_patch_clip, frontendPc.c_sphere_cull_patch_out,
				  frontendPc.c_box_cull_patch_in, frontendPc.c_box_cull_patch_clip, frontendPc.c_box_cull_patch_out);
		ri.Printf(PRINT_ALL, "(md3) %i sin %i sclip  %i sout %i bin %i bclip %i bout\n",
				  frontendPc.c_sphere_cull_md3_in, frontendPc.c_sphere_cull_md3_clip, frontendPc.c_sphere_cull_md3_out,
				  frontendPc.c_box_cull_md3_in, frontendPc.c_box_cull_md3_clip, frontendPc.c_box_cull_md3_out);
	}
	else if (r_speeds->integer == 3)
	{
		ri.Printf(PRINT_ALL, "viewcluster: %i\n", frame.viewCluster);
	}
	else if (r_speeds->integer == 4)
	{
		if (backendPc.c_dlightVertexes)
		{
			ri.Printf(PRINT_ALL, "dlight srf:%i  culled:%i  verts:%i  tris:%i\n",
					  frontendPc.c_dlightSurfaces, frontendPc.c_dlightSurfacesCulled,
					  backendPc.c_dlightVertexes, backendPc.c_dlightIndexes / 3);
		}
	}
	else if (r_speeds->integer == 5)
	{
		ri.Printf(PRINT_ALL, "zFar: %.0f\n", frame.zFar);
	}
	else if (r_speeds->integer == 6)
	{
		ri.Printf(PRINT_ALL, "flare adds:%i tests:%i renders:%i\n",
				  backendPc.c_flareAdds, backendPc.c_flareTests, backendPc.c_flareRenders);
	}
}

enum class renderIssueResult_t
{
	SKIPPED,
	SYNCHRONOUS,
	ASYNCHRONOUS
};

/*
====================
R_IssueRenderCommands
====================
*/
static renderIssueResult_t R_IssueRenderCommands(void)
{
	renderCommandList_t &cmdList = backEndData->commands;
	renderFrameState_t &frame = backEndData->frame;

	// add an end-of-list command
	*(int *)(cmdList.cmds + cmdList.used) = static_cast<int>(renderCommand_t::RC_END_OF_LIST);

	if (frame.screenshotMask == 0)
	{
		if (ri.CL_IsMinimized())
			return renderIssueResult_t::SKIPPED; // skip backend when minimized
		if (frame.throttle)
			return renderIssueResult_t::SKIPPED; // or throttled on demand
	}
	else
	{
		if (ri.CL_IsMinimized() && !RE_CanMinimize())
		{
			frame.screenshotMask = 0;
			return renderIssueResult_t::SKIPPED;
		}
	}

	if (r_skipBackEnd->integer)
		return renderIssueResult_t::SKIPPED;

	// Screenshot/video paths call back into filesystem/video code.  Keep them on
	// the main thread until those engine callbacks are explicitly made thread-safe.
	if (s_renderThreadActive && frame.screenshotMask == 0)
	{
		R_SubmitRenderFrame(*backEndData);
		return renderIssueResult_t::ASYNCHRONOUS;
	}

	R_ExecuteBackEndFrame(*backEndData);
	return renderIssueResult_t::SYNCHRONOUS;
}

/*
============
R_GetCommandBufferReserved

make sure there is enough command space
============
*/
static void *R_GetCommandBufferReserved(int bytes, const int reservedBytes)
{
	renderCommandList_t &cmdList = backEndData->commands;
	bytes = pad_up_ct<int, alignof(void*)>(bytes);

	// always leave room for the end of list command
	if (cmdList.used + bytes + sizeof(int) + reservedBytes > MAX_RENDER_COMMANDS)
	{
		if (static_cast<std::size_t>(bytes) > MAX_RENDER_COMMANDS - sizeof(int))
		{
			ri.Error(ERR_FATAL, "R_GetCommandBuffer: bad size %i", bytes);
		}
		// if we run out of room, just start dropping commands
		return NULL;
	}

	cmdList.used += bytes;

	return cmdList.cmds + cmdList.used - bytes;
}

/*
=============
R_GetCommandBuffer
returns NULL if there is not enough space for important commands
=============
*/
void *R_GetCommandBuffer(int bytes)
{
	tr.lastRenderCommand = renderCommand_t::RC_END_OF_LIST;

	constexpr size_t aligned = pad_up_ct<size_t, alignof(void*)>(sizeof(swapBuffersCommand_t));

#ifndef NDEBUG
	// Ensure the value fits into int before narrowing
	assert(aligned <= static_cast<size_t>(std::numeric_limits<int>::max()));
#endif

	return R_GetCommandBufferReserved(bytes, static_cast<int>(aligned));
}

/*
=============
R_AddDrawSurfCmd
=============
*/
void R_AddDrawSurfCmd(drawSurf_t &drawSurfs, int numDrawSurfs)
{
	drawSurfsCommand_t *cmd;

	cmd = static_cast<drawSurfsCommand_t *>(R_GetCommandBuffer(sizeof(*cmd)));
	if (!cmd)
	{
		return;
	}
	cmd->commandId = renderCommand_t::RC_DRAW_SURFS;

	cmd->drawSurfs = &drawSurfs;
	cmd->numDrawSurfs = numDrawSurfs;

	cmd->refdef = tr.refdef;
	cmd->viewParms = tr.viewParms;

	tr.numDrawSurfCmds++;
	if (tr.drawSurfCmd == NULL)
	{
		tr.drawSurfCmd = cmd;
	}
}

constexpr vec4_t colorWhite_cpp = {1, 1, 1, 1};

/*
=============
RE_SetColor

Passing NULL will set the color to white
=============
*/
void RE_SetColor(const float *rgba)
{
	setColorCommand_t *cmd;

	if (!tr.registered)
	{
		return;
	}
	cmd = static_cast<setColorCommand_t *>(R_GetCommandBuffer(sizeof(*cmd)));
	if (!cmd)
	{
		return;
	}
	cmd->commandId = renderCommand_t::RC_SET_COLOR;
	if (!rgba)
	{
		rgba = colorWhite_cpp;
	}

	cmd->color[0] = rgba[0];
	cmd->color[1] = rgba[1];
	cmd->color[2] = rgba[2];
	cmd->color[3] = rgba[3];
}

/*
=============
RE_StretchPic
=============
*/
void RE_StretchPic(float x, float y, float w, float h,
				   float s1, float t1, float s2, float t2, qhandle_t hShader)
{
	stretchPicCommand_t *cmd;

	if (!tr.registered)
	{
		return;
	}
	cmd = reinterpret_cast<stretchPicCommand_t *>(R_GetCommandBuffer(sizeof(*cmd)));
	if (!cmd)
	{
		return;
	}
	cmd->commandId = renderCommand_t::RC_STRETCH_PIC;
	cmd->shader = R_GetShaderByHandle(hShader);
	cmd->x = x;
	cmd->y = y;
	cmd->w = w;
	cmd->h = h;
	cmd->s1 = s1;
	cmd->t1 = t1;
	cmd->s2 = s2;
	cmd->t2 = t2;
}

/*
====================
RE_BeginFrame

If running in stereo, RE_BeginFrame will be called twice
for each RE_EndFrame
====================
*/
void RE_BeginFrame(stereoFrame_t stereoFrame)
{
	drawBufferCommand_t *cmd;

	if (!tr.registered)
	{
		return;
	}

	tr.frameCount++;
	tr.frameSceneNum = 0;
	backEndData->frame.frameCount = tr.frameCount;

	if ((cmd = static_cast<drawBufferCommand_t *>(R_GetCommandBuffer(sizeof(*cmd)))) == NULL)
		return;

	cmd->commandId = renderCommand_t::RC_DRAW_BUFFER;

	tr.lastRenderCommand = renderCommand_t::RC_DRAW_BUFFER;

	if (glConfig.stereoEnabled)
	{
		if (stereoFrame == STEREO_LEFT)
		{
			cmd->buffer = std::to_underlying(glCompat::GL_BACK_LEFT);
		}
		else if (stereoFrame == STEREO_RIGHT)
		{
			cmd->buffer = std::to_underlying(glCompat::GL_BACK_RIGHT);
		}
		else
		{
			ri.Error(ERR_FATAL, "RE_BeginFrame: Stereo is enabled, but stereoFrame was %i", stereoFrame);
		}
	}
	else
	{
		if (stereoFrame != STEREO_CENTER)
		{
			ri.Error(ERR_FATAL, "RE_BeginFrame: Stereo is disabled, but stereoFrame was %i", stereoFrame);
		}

		cmd->buffer = 0;
	}

#ifndef USE_BUFFER_CLEAR
	if ( r_fastsky->integer && vk_inst.clearAttachment ) {
		if ( stereoFrame != STEREO_RIGHT ) {
			clearColorCommand_t *clrcmd; 
			if ( ( clrcmd = R_GetCommandBuffer( sizeof( *clrcmd ) ) ) == NULL )
				return;
			clrcmd->commandId = renderCommand_t::RC_CLEARCOLOR;
		}
	}
#endif // USE_BUFFER_CLEAR

	tr.refdef.stereoFrame = stereoFrame;
}

/*
=============
RE_TakeVideoFrame
=============
*/
void RE_TakeVideoFrame(int width, int height,
					   byte *captureBuffer, byte *encodeBuffer, bool motionJpeg)
{
	if (!tr.registered)
	{
		return;
	}

	backEndData->frame.screenshotMask |= SCREENSHOT_AVI;

	videoFrameCommand_t &cmd = backEndData->frame.vcmd;

	// cmd->commandId = RC_VIDEOFRAME;

	cmd.width = width;
	cmd.height = height;
	cmd.captureBuffer = captureBuffer;
	cmd.encodeBuffer = encodeBuffer;
	cmd.motionJpeg = motionJpeg;
}

void RE_ThrottleBackend()
{
	backEndData->frame.throttle = true;
}

void RE_FinishBloom()
{
	finishBloomCommand_t *cmd;

	if (!tr.registered)
	{
		return;
	}

	cmd = static_cast<finishBloomCommand_t *>(R_GetCommandBuffer(sizeof(*cmd)));
	if (!cmd)
	{
		return;
	}

	cmd->commandId = renderCommand_t::RC_FINISHBLOOM;
}

bool RE_CanMinimize()
{
	if (vk_inst.fboActive || vk_inst.offscreenRender)
		return true;
	return false;
}

const glconfig_t *RE_GetConfig()
{
	return &glConfig;
}

void RE_VertexLighting(bool allowed)
{
	tr.vertexLightingAllowed = allowed;
}

/*
=============
RE_EndFrame

Returns the number of msec spent in the back end
=============
*/
void RE_EndFrame(int *frontEndMsec, int *backEndMsec)
{
	swapBuffersCommand_t *cmd;

	if (!tr.registered)
	{
		return;
	}

	cmd = static_cast<swapBuffersCommand_t *>(R_GetCommandBufferReserved(sizeof(*cmd), 0));
	if (!cmd)
	{
		return;
	}
	cmd->commandId = renderCommand_t::RC_SWAP_BUFFERS;

	// The previous frame may still be executing.  We wait only here, after the
	// complete front end for this frame has already run, so the overlap is:
	// backend(N-1) || frontend(N).
	backEndData_t *completedData = nullptr;
	backEndCounters_t completedPc{};
	const bool completedPrevious = R_ConsumeCompletedRenderFrame(completedData, completedPc);

	if (completedPrevious)
	{
		R_PerformanceCounters(completedData->frame, completedPc);
	}

	// Any Vulkan/resource mutation caused by renderer cvars must happen after
	// the previous backend completed and before the next one is submitted.
	if (ri.Cvar_CheckGroup(CVG_RENDERER))
	{
		if (r_textureMode->modified)
		{
			TextureMode(r_textureMode->string);
		}

		if (r_gamma->modified)
		{
			R_SetColorMappings();
		}

		vk_update_post_process_pipelines();
		ri.Cvar_ResetGroup(CVG_RENDERER, true /* reset modified flags */);
	}

	// Snapshot all front-end diagnostics into the same immutable frame buffer
	// that owns the draw surfaces/entities referenced by the command list.
	backEndData->frame.frontEndPc = tr.pc;
	backEndData->frame.frontEndMsec = tr.frontEndMsec;
	backEndData->frame.viewCluster = tr.viewCluster;
	backEndData->frame.zFar = tr.viewParms.zFar;
	if (!backEndData->frame.frameCount)
		backEndData->frame.frameCount = tr.frameCount;

	if (frontEndMsec)
	{
		*frontEndMsec = tr.frontEndMsec;
	}
	tr.frontEndMsec = 0;
	Com_Memset(&tr.pc, 0, sizeof(tr.pc));

	if (backEndMsec)
	{
		*backEndMsec = completedPrevious ? completedPc.msec : 0;
	}

	const renderIssueResult_t issueResult = R_IssueRenderCommands();

	if (issueResult == renderIssueResult_t::SYNCHRONOUS)
	{
		R_PerformanceCounters(backEndData->frame, backEnd.pc);
		if (backEndMsec)
			*backEndMsec = backEnd.pc.msec;
	}
	else if (issueResult == renderIssueResult_t::ASYNCHRONOUS)
	{
		R_RotateFrontEndDataBuffer();
	}

	R_InitNextFrame();
}
