#ifndef TR_NOISE_CPLUS_HPP
#define TR_NOISE_CPLUS_HPP

#ifdef __cplusplus
extern "C"
{
#endif

    void NoiseInit();
    float NoiseGet4f(float x, float y, float z, double t);

#ifdef __cplusplus
}
#endif

#endif // TR_NOISE_CPLUS_HPP
