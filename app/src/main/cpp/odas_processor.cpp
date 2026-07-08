#include <jni.h>
#include <android/log.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include <module/mod_stft.h>
#include <module/mod_ssl.h>
#include <message/msg_hops.h>
#include <message/msg_spectra.h>
#include <message/msg_pots.h>
#include <general/mic.h>
#include <general/samplerate.h>
#include <general/soundspeed.h>
#include <general/spatialfilter.h>
}

#define LOG_TAG "ODAS_SSL"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static int g_initialized = 0;

static mod_stft_obj * g_stft = NULL;
static mod_ssl_obj * g_ssl = NULL;
static msg_hops_obj * g_hop_msg = NULL;
static msg_spectra_obj * g_spec_msg = NULL;
static msg_pots_obj * g_pot_msg = NULL;

static unsigned int g_hopSize = 128;
static unsigned int g_nChannels = 2;
static unsigned int g_nPots = 3;
static float g_micSpacing = 0.05f;

static unsigned long long g_timestamp = 0;

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_example_myapplication_OdasEngine_initSSL(JNIEnv *env, jobject thiz,
    jfloat micSpacing) {

    if (g_initialized) return JNI_TRUE;

    g_micSpacing = micSpacing;
    float halfSpacing = micSpacing / 2.0f;

    msg_hops_cfg * hop_cfg = msg_hops_cfg_construct();
    hop_cfg->hopSize = g_hopSize;
    hop_cfg->nChannels = g_nChannels;
    hop_cfg->fS = 16000;

    msg_spectra_cfg * spec_cfg = msg_spectra_cfg_construct();
    spec_cfg->halfFrameSize = 129;
    spec_cfg->nChannels = g_nChannels;
    spec_cfg->fS = 16000;

    msg_pots_cfg * pot_cfg = msg_pots_cfg_construct();
    pot_cfg->nPots = g_nPots;
    pot_cfg->fS = 16000;

    mics_obj * mics = mics_construct_zero(2);
    mics->mu[0] = -halfSpacing; mics->mu[1] = 0.0f;  mics->mu[2] = 0.0f;
    mics->mu[3] = halfSpacing;  mics->mu[4] = 0.0f;  mics->mu[5] = 0.0f;
    for (int i = 0; i < 2; i++) {
        mics->direction[i * 3 + 0] = 0.0f;
        mics->direction[i * 3 + 1] = 1.0f;
        mics->direction[i * 3 + 2] = 0.0f;
        mics->thetaAllPass[i] = 90.0f;
        mics->thetaNoPass[i] = 90.0f;
        for (int j = 0; j < 9; j++) {
            mics->sigma2[i * 9 + j] = (j % 4 == 0) ? 1.0f : 0.0f;
        }
    }

    samplerate_obj * samplerate = samplerate_construct_zero();
    samplerate->mu = 16000;
    samplerate->sigma2 = 0.0f;

    soundspeed_obj * soundspeed = soundspeed_construct_zero();
    soundspeed->mu = 343.0f;
    soundspeed->sigma2 = 0.0f;

    spatialfilters_obj * spatialfilters = spatialfilters_construct_zero(1);
    spatialfilters->direction[0] = 0.0f;
    spatialfilters->direction[1] = 1.0f;
    spatialfilters->direction[2] = 0.0f;
    spatialfilters->thetaAllPass[0] = 90.0f;
    spatialfilters->thetaNoPass[0] = 90.0f;

    mod_ssl_cfg * ssl_cfg = mod_ssl_cfg_construct();
    ssl_cfg->mics = mics;
    ssl_cfg->samplerate = samplerate;
    ssl_cfg->soundspeed = soundspeed;
    ssl_cfg->spatialfilters = spatialfilters;
    ssl_cfg->interpRate = 4;
    ssl_cfg->epsilon = 0.0000000001f;
    ssl_cfg->nLevels = 2;
    ssl_cfg->levels = (unsigned int *)malloc(2 * sizeof(unsigned int));
    ssl_cfg->levels[0] = 2;
    ssl_cfg->levels[1] = 3;
    ssl_cfg->deltas = (signed int *)malloc(2 * sizeof(signed int));
    ssl_cfg->deltas[0] = -1;
    ssl_cfg->deltas[1] = -1;
    ssl_cfg->nMatches = 3;
    ssl_cfg->probMin = 0.5f;
    ssl_cfg->nRefinedLevels = 1;
    ssl_cfg->nThetas = 181;
    ssl_cfg->gainMin = 0.1f;

    mod_stft_cfg * stft_cfg = mod_stft_cfg_construct();

    g_stft = mod_stft_construct(stft_cfg, hop_cfg, spec_cfg);
    g_ssl = mod_ssl_construct(ssl_cfg, spec_cfg, pot_cfg);

    g_hop_msg = msg_hops_construct(hop_cfg);
    g_spec_msg = msg_spectra_construct(spec_cfg);
    g_pot_msg = msg_pots_construct(pot_cfg);

    mod_stft_connect(g_stft, g_hop_msg, g_spec_msg);
    mod_ssl_connect(g_ssl, g_spec_msg, g_pot_msg);

    mod_stft_enable(g_stft);
    mod_ssl_enable(g_ssl);

    mod_stft_cfg_destroy(stft_cfg);
    mod_ssl_cfg_destroy(ssl_cfg);
    msg_hops_cfg_destroy(hop_cfg);
    msg_spectra_cfg_destroy(spec_cfg);
    msg_pots_cfg_destroy(pot_cfg);

    g_timestamp = 0;
    g_initialized = 1;

    LOGI("SSL initialized (2 mics, 16kHz, hop=%d, spacing=%.1fcm)", g_hopSize, g_micSpacing * 100.0f);
    return JNI_TRUE;
}

JNIEXPORT jfloatArray JNICALL
Java_com_example_myapplication_OdasEngine_processAudio(JNIEnv *env, jobject thiz,
    jfloatArray audioData, jint nChannels) {

    if (!g_initialized) {
        return env->NewFloatArray(0);
    }

    jsize len = env->GetArrayLength(audioData);
    jfloat *data = env->GetFloatArrayElements(audioData, NULL);

    unsigned int hopSize = (unsigned int)(len / nChannels);

    for (unsigned int ch = 0; ch < (unsigned int)nChannels && ch < g_nChannels; ch++) {
        for (unsigned int s = 0; s < hopSize && s < g_hopSize; s++) {
            g_hop_msg->hops->array[ch][s] = data[s * nChannels + ch];
        }
    }

    env->ReleaseFloatArrayElements(audioData, data, JNI_ABORT);

    g_hop_msg->timeStamp = ++g_timestamp;
    g_hop_msg->fS = 16000;

    mod_stft_process(g_stft);
    mod_ssl_process(g_ssl);

    // Direct TDOA estimate via cross-correlation with parabolic peak
    // interpolation for sub-sample precision.
    float *ch0 = g_hop_msg->hops->array[0];
    float *ch1 = g_hop_msg->hops->array[1];
    // Max physically-possible lag: spacing * fS / c
    int maxPossibleLag = (int)(g_micSpacing * 16000.0f / 343.0f) + 1;
    if (maxPossibleLag < 1) maxPossibleLag = 1;
    if (maxPossibleLag > 4) maxPossibleLag = 4;
    int kMaxLag = maxPossibleLag;
    float corrVals[2 * kMaxLag + 1];
    int bestLag = 0;
    float bestCorr = -1e10f;
    float sumCorr = 0.0f;
    for (int lag = -kMaxLag; lag <= kMaxLag; lag++) {
        float corr = 0.0f;
        int cnt = 0;
        for (unsigned int s = 0; s < g_hopSize; s++) {
            int s1 = (int)s + lag;
            if (s1 >= 0 && s1 < (int)g_hopSize) {
                corr += ch0[s] * ch1[s1];
                cnt++;
            }
        }
        if (cnt > 0) corr /= cnt;
        corrVals[lag + kMaxLag] = corr;
        sumCorr += corr;
        if (corr > bestCorr) { bestCorr = corr; bestLag = lag; }
    }
    float avgCorr = sumCorr / (float)(2 * kMaxLag + 1);
    float peakQuality = (avgCorr > 0.0001f) ? (bestCorr - avgCorr) / avgCorr : 0.0f;

    // Parabolic interpolation around the peak
    float peakLag = (float)bestLag;
    int idx = bestLag + kMaxLag;
    if (bestLag > -kMaxLag && bestLag < kMaxLag) {
        float cm1 = corrVals[idx - 1];
        float c0 = corrVals[idx];
        float cp1 = corrVals[idx + 1];
        float denom = 2.0f * (2.0f * c0 - cm1 - cp1);
        if (fabsf(denom) > 1e-12f) {
            peakLag += (cp1 - cm1) / denom;
        }
    }

    float directAz = 0.0f;
    float directE = 0.0f;
    if (peakQuality > 0.3f) {
        float tdoa = peakLag / 16000.0f;
        float xDirect = tdoa * 343.0f / g_micSpacing;
        if (xDirect > 1.0f) xDirect = 1.0f;
        if (xDirect < -1.0f) xDirect = -1.0f;
        directAz = asinf(xDirect) * 180.0f / M_PI;
        directE = peakQuality;
    } else {
        directAz = 0.0f;
        directE = 0.0f;
    }

    // Periodic debug log (very sparse)
    static int logCounter = 0;
    if (++logCounter % 500 == 1) {
        LOGI("Xcorr: bestLag=%d floatLag=%.2f az=%.1f qual=%.2f (spacing=%.1fcm)",
             bestLag, peakLag, directAz, peakQuality, g_micSpacing * 100.0f);
    }

    jfloat result[6];
    for (unsigned int i = 0; i < g_nPots; i++) {
        float x = g_pot_msg->pots->array[i * 4 + 0];
        float energy = g_pot_msg->pots->array[i * 4 + 3];
        if (x > 1.0f) x = 1.0f;
        if (x < -1.0f) x = -1.0f;
        float azimuth = asinf(x) * 180.0f / M_PI;
        result[i * 2 + 0] = azimuth;
        result[i * 2 + 1] = energy;
    }

    // Replace pot[0] (the tracked source) with the direct per-frame estimate
    // so the UI shows instantaneous direction. Energy = peak quality.
    result[0] = directAz;
    result[1] = directE;

    jfloatArray resultArray = env->NewFloatArray(g_nPots * 2);
    env->SetFloatArrayRegion(resultArray, 0, g_nPots * 2, result);
    return resultArray;
}

JNIEXPORT void JNICALL
Java_com_example_myapplication_OdasEngine_destroySSL(JNIEnv *env, jobject thiz) {

    if (!g_initialized) return;

    mod_stft_disconnect(g_stft);
    mod_ssl_disconnect(g_ssl);

    mod_stft_destroy(g_stft);
    mod_ssl_destroy(g_ssl);

    msg_hops_destroy(g_hop_msg);
    msg_spectra_destroy(g_spec_msg);
    msg_pots_destroy(g_pot_msg);

    g_stft = NULL;
    g_ssl = NULL;
    g_hop_msg = NULL;
    g_spec_msg = NULL;
    g_pot_msg = NULL;

    g_initialized = 0;
    LOGI("SSL destroyed");
}

} // extern "C"
