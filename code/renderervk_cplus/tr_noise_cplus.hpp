#ifndef TR_NOISE_CPLUS_HPP
#define TR_NOISE_CPLUS_HPP

#ifdef __cplusplus
extern "C"
{
#endif

    void NoiseInit_plus();
    float NoiseGet4f_plus(float x, float y, float z, double t);

#ifdef __cplusplus
}
#endif

#endif // TR_NOISE_CPLUS_HPP
