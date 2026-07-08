#ifndef __ODAS_UTILS_FFT
#define __ODAS_UTILS_FFT

#include <stdlib.h>
#include <pffft.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fft_obj {

    unsigned int frameSize;
    unsigned int halfFrameSize;
    PFFFT_Setup * pR2C;
    PFFFT_Setup * pC2R;
    float * inputBuffer;
    float * outputBuffer;
    float * work;

} fft_obj;

fft_obj * fft_construct(const unsigned int size);
void fft_destroy(fft_obj * obj);
void fft_r2c(fft_obj * obj, const float * in, float * out);
void fft_c2r(fft_obj * obj, const float * in, float * out);

#ifdef __cplusplus
}
#endif

#endif
