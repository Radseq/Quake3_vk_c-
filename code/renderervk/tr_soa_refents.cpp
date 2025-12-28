#include "tr_soa_refents.hpp"

namespace trsoa
{
	static inline std::uint8_t to_u8(int v) noexcept { return static_cast<std::uint8_t>(v); }

	void BuildFrameGather(
		FrameGather& out,
		const trRefdef_t& refdef,
		const viewParms_t& view,
		model_t* (*GetModelByHandle)(qhandle_t)
	) noexcept
	{
		out.reset();

		const int n = refdef.num_entities;
		for (int i = 0; i < n; ++i)
		{
			const trRefEntity_t& ent = refdef.entities[i];
			const refEntity_t&  e    = ent.e;

			// Semantyka jak u Ciebie: first-person weapon nie pojawia się w portal/mirror view.
			if ((e.renderfx & RF_FIRST_PERSON) && (view.portalView != portalView_t::PV_NONE))
				continue;

			// --- All entities SoA (packed) ---
			const std::uint16_t dst = out.all.count;
			if (dst >= MAX_REFENTITIES)
				break;

			out.all.entIndex[dst]     = static_cast<std::uint16_t>(i);
			out.all.reType[dst]       = to_u8(static_cast<int>(e.reType));
			out.all.nnAxes[dst]       = e.nonNormalizedAxes ? 1u : 0u;
			out.all.frame[dst]        = static_cast<std::uint16_t>(e.frame);
			out.all.oldframe[dst]     = static_cast<std::uint16_t>(e.oldframe);
			out.all.renderfx[dst]     = static_cast<std::uint32_t>(e.renderfx);
			out.all.hModel[dst]       = e.hModel;
			out.all.customShader[dst] = e.customShader;
			out.all.customSkin[dst]   = e.customSkin;
			out.all.skinNum[dst]      = static_cast<std::uint16_t>(e.skinNum);

			out.all.ox[dst] = e.origin[0];
			out.all.oy[dst] = e.origin[1];
			out.all.oz[dst] = e.origin[2];

			out.all.count = static_cast<std::uint16_t>(dst + 1);

			// --- Buckets (entity indices) ---
			switch (e.reType)
			{
				case RT_MODEL:
				{
					out.buckets.models.push(static_cast<std::uint16_t>(i));

					// Model-only SoA (packed separately) + resolve model pointer once
					const std::uint16_t mdst = out.models.count;
					if (mdst < MAX_REFENTITIES)
					{
						out.models.entIndex[mdst] = static_cast<std::uint16_t>(i);
						out.models.frame[mdst]    = static_cast<std::uint16_t>(e.frame);
						out.models.oldframe[mdst] = static_cast<std::uint16_t>(e.oldframe);
						out.models.renderfx[mdst] = static_cast<std::uint32_t>(e.renderfx);
						out.models.nnAxes[mdst]   = e.nonNormalizedAxes ? 1u : 0u;

						out.models.ox[mdst] = e.origin[0];
						out.models.oy[mdst] = e.origin[1];
						out.models.oz[mdst] = e.origin[2];

						out.models.modelPtr[mdst] = GetModelByHandle ? GetModelByHandle(e.hModel) : nullptr;

						out.models.count = static_cast<std::uint16_t>(mdst + 1);
					}
					break;
				}

				case RT_PORTALSURFACE:
					out.buckets.portalSurfaces.push(static_cast<std::uint16_t>(i));
					break;

				case RT_POLY:
					out.buckets.polys.push(static_cast<std::uint16_t>(i));
					break;

				case RT_SPRITE:
				case RT_BEAM:
				case RT_LIGHTNING:
				case RT_RAIL_CORE:
				case RT_RAIL_RINGS:
					out.buckets.generated.push(static_cast<std::uint16_t>(i));
					break;

				default:
					out.buckets.unknown.push(static_cast<std::uint16_t>(i));
					break;
			}
		}
	}

	void BucketModelsByType(
		ModelTypeBuckets& out,
		const ModelsSoA& models
	) noexcept
	{
		out.reset();

		const std::uint16_t n = models.count;
		for (std::uint16_t i = 0; i < n; ++i)
		{
			const std::uint16_t ei = models.entIndex[i];
			model_t* m = models.modelPtr[i];

			if (!m)
			{
				out.bad.push(ei);
				continue;
			}

			switch (m->type)
			{
				case modtype_t::MOD_MESH:  out.md3.push(ei); break;
				case modtype_t::MOD_MDR:   out.mdr.push(ei); break;
				case modtype_t::MOD_IQM:   out.iqm.push(ei); break;
				case modtype_t::MOD_BRUSH: out.brush.push(ei); break;
				case modtype_t::MOD_BAD:   out.bad.push(ei); break;
				default:                   out.unknown.push(ei); break;
			}
		}
	}

	void R_AddEntitySurfaces_SoA(
		const AddEntitySurfacesApi& api,
		surfaceType_t& entitySurface
	)
	{
		trGlobals_t& tr = *api.tr;

		if (!r_drawentities->integer)
			return;

		// 1) Gather once (AoS->SoA + buckets)
		FrameGather g;
		BuildFrameGather(g, tr.refdef, tr.viewParms, api.GetModelByHandle);

		// 2) Generated entities in a tight loop
		{
			const auto n = g.buckets.generated.size();
			for (std::uint16_t k = 0; k < n; ++k)
			{
				const std::uint16_t ei = g.buckets.generated[k];

				tr.currentEntityNum = static_cast<int>(ei);
				tr.currentEntity    = &tr.refdef.entities[ei];
				tr.shiftedEntityNum = (static_cast<int>(ei) << QSORT_REFENTITYNUM_SHIFT);

				trRefEntity_t& ent = *tr.currentEntity;

				// Semantyka jak w Q3: third-person-only pomijamy w primary view
				if ((ent.e.renderfx & RF_THIRD_PERSON) && (tr.viewParms.portalView == portalView_t::PV_NONE))
					continue;

				shader_t* sh = api.GetShaderByHandle(ent.e.customShader);
				const int fog = api.SpriteFogNum(ent);
				api.AddDrawSurf(entitySurface, *sh, fog, 0);
			}
		}

		// 3) Models grouped by model type
		{
			ModelTypeBuckets typed;
			BucketModelsByType(typed, g.models);

			auto process = [&](const IndexBucket<MAX_REFENTITIES>& lst, auto&& fn)
			{
				const auto n = lst.size();
				for (std::uint16_t k = 0; k < n; ++k)
				{
					const std::uint16_t ei = lst[k];

					tr.currentEntityNum = static_cast<int>(ei);
					tr.currentEntity    = &tr.refdef.entities[ei];
					tr.shiftedEntityNum = (static_cast<int>(ei) << QSORT_REFENTITYNUM_SHIFT);

					trRefEntity_t& ent = *tr.currentEntity;

					api.RotateForEntity(ent, tr.viewParms, tr.ort);

					tr.currentModel = api.GetModelByHandle(ent.e.hModel);
					if (!tr.currentModel)
					{
						api.AddDrawSurf(entitySurface, *tr.defaultShader, 0, 0);
						continue;
					}

					fn(ent);
				}
			};

			process(typed.md3,   [&](trRefEntity_t& e) { api.AddMD3(e); });
			process(typed.mdr,   [&](trRefEntity_t& e) { api.AddMDR(e); });
			process(typed.iqm,   [&](trRefEntity_t& e) { api.AddIQM(e); });
			process(typed.brush, [&](trRefEntity_t& e) { api.AddBrush(e); });

			// MOD_BAD – semantyka jak u Ciebie
			process(typed.bad, [&](trRefEntity_t& e)
			{
				if ((e.e.renderfx & RF_THIRD_PERSON) && (tr.viewParms.portalView == portalView_t::PV_NONE))
					return;
				api.AddDrawSurf(entitySurface, *tr.defaultShader, 0, 0);
			});

			// Unknown model types – zachowaj “drop”
			if (typed.unknown.size())
			{
				ri.Error(ERR_DROP, "R_AddEntitySurfaces_SoA: Bad modeltype");
			}
		}

		// 4) Portal surfaces – nie renderujesz tu (zwykle obsługiwane w osobnym przebiegu)
		// g.buckets.portalSurfaces jest dostępne, jeśli chcesz mieć to w jednym miejscu.

		// 5) Polys – jeśli w Twojej ścieżce tego nie ma, możesz zostawić jako error/debug
		// (tu nic nie robimy, bo Twoje wcześniejsze kody sugerowały, że RT_POLY nie jest normalnie używany)
	}

} // namespace trsoa
