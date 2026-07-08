#include <utils/fft.h>

fft_obj * fft_construct(const unsigned int frameSize) {

    fft_obj * obj;

    obj = (fft_obj *) malloc(sizeof(fft_obj));

    obj->frameSize = frameSize;
    obj->halfFrameSize = frameSize/2+1;

    obj->pR2C = pffft_new_setup(frameSize, PFFFT_REAL);
    obj->pC2R = pffft_new_setup(frameSize, PFFFT_REAL);

    obj->inputBuffer = (float *) pffft_aligned_malloc(sizeof(float) * frameSize);
    obj->outputBuffer = (float *) pffft_aligned_malloc(sizeof(float) * frameSize);
    obj->work = (float *) pffft_aligned_malloc(sizeof(float) * frameSize);

    return obj;

}

void fft_destroy(fft_obj * obj) {

    pffft_destroy_setup(obj->pR2C);
    pffft_destroy_setup(obj->pC2R);

    pffft_aligned_free(obj->inputBuffer);
    pffft_aligned_free(obj->outputBuffer);
    pffft_aligned_free(obj->work);

    free((void *) obj);

}

void fft_r2c(fft_obj * obj, const float * in, float * out) {

    unsigned int iSample;

    for (iSample = 0; iSample < obj->frameSize; iSample++) {
        obj->inputBuffer[iSample] = in[iSample];
    }

    pffft_transform_ordered(obj->pR2C, obj->inputBuffer, obj->outputBuffer, obj->work, PFFFT_FORWARD);

    for (iSample = 0; iSample < obj->halfFrameSize; iSample++) {
        out[iSample*2+0] = obj->outputBuffer[iSample*2+0];
        out[iSample*2+1] = obj->outputBuffer[iSample*2+1];
    }

}

void fft_c2r(fft_obj * obj, const float * in, float * out) {

    unsigned int iSample;

    for (iSample = 0; iSample < obj->halfFrameSize; iSample++) {
        obj->inputBuffer[iSample*2+0] = in[iSample*2+0];
        obj->inputBuffer[iSample*2+1] = in[iSample*2+1];
    }

    pffft_transform_ordered(obj->pC2R, obj->inputBuffer, obj->outputBuffer, obj->work, PFFFT_BACKWARD);

    for (iSample = 0; iSample < obj->frameSize; iSample++) {
        out[iSample] = obj->outputBuffer[iSample] / ((float) obj->frameSize);
    }

}
