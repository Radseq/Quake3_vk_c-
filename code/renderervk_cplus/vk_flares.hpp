#ifndef VK_FLARES_HPP
#define VK_FLARES_HPP

#ifdef __cplusplus
extern "C"
{
#endif

#include "q_math.hpp"
#include "tr_main.hpp"
#include "tr_light.hpp"
#include "q_shared.hpp"

      // flare states maintain visibility over multiple frames for fading
      // layers: view, mirror, menu
      typedef struct flare_s
      {
            struct flare_s *next; // for active chain

            int addedFrame;
            uint32_t testCount;

            portalView_t portalView;
            int frameSceneNum;
            void *surface;
            int fogNum;

            int fadeTime;

            bool visible;        // state of last test
            float drawIntensity; // may be non 0 even if !visible due to fading

            int windowX, windowY;
            float eyeZ;
            float drawZ;

            vec3_t origin;
            vec3_t color;
      } flare_t;

      static flare_t r_flareStructs[MAX_FLARES];
      static flare_t *r_activeFlares, *r_inactiveFlares;

      void R_ClearFlares_plus(void);
      void RB_AddFlare_plus(void *surface, int fogNum, vec3_t point, vec3_t color, vec3_t normal);
      void RB_AddDlightFlares_plus(void);
      void RB_RenderFlares_plus(void);

#ifdef __cplusplus
}
#endif

#endif // VK_FLARES_HPP
