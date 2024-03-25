#include "tr_light.hpp"
/*
===============
R_TransformDlights

Transforms the origins of an array of dlights.
Used by both the front end (for DlightBmodel) and
the back end (before doing the lighting calculation)
===============
*/
void R_TransformDlights_plus(int count, dlight_t *dl, orientationr_t *ort)
{
    int i;
    vec3_t temp, temp2;

    for (i = 0; i < count; i++, dl++)
    {
        VectorSubtract(dl->origin, ort->origin, temp);
        dl->transformed[0] = DotProduct(temp, ort->axis[0]);
        dl->transformed[1] = DotProduct(temp, ort->axis[1]);
        dl->transformed[2] = DotProduct(temp, ort->axis[2]);
        if (dl->linear)
        {
            VectorSubtract(dl->origin2, ort->origin, temp2);
            dl->transformed2[0] = DotProduct(temp2, ort->axis[0]);
            dl->transformed2[1] = DotProduct(temp2, ort->axis[1]);
            dl->transformed2[2] = DotProduct(temp2, ort->axis[2]);
        }
    }
}