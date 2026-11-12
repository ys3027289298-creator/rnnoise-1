/* Copyright (c) 2018 Gregor Richards
 * Copyright (c) 2017 Mozilla */
/*
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef RNNOISE_H
#define RNNOISE_H 1

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef RNNOISE_EXPORT
# if defined(WIN32)
#  if defined(RNNOISE_BUILD) && defined(DLL_EXPORT)
#   define RNNOISE_EXPORT __declspec(dllexport)
#  else
#   define RNNOISE_EXPORT
#  endif
# elif defined(__GNUC__) && defined(RNNOISE_BUILD)
#  define RNNOISE_EXPORT __attribute__ ((visibility ("default")))
# else
#  define RNNOISE_EXPORT
# endif
#endif

typedef struct DenoiseState DenoiseState;
typedef struct RNNModel RNNModel;

/**
 * Return the size of DenoiseState
 */
RNNOISE_EXPORT int rnnoise_get_size(void);

/**
 * Return the number of samples processed by rnnoise_process_frame at a time
 */
RNNOISE_EXPORT int rnnoise_get_frame_size(void);

/**
 * Initializes a pre-allocated DenoiseState
 *
 * If model is NULL the default model is used.
 *
 * See: rnnoise_create() and rnnoise_model_from_file()
 */
RNNOISE_EXPORT int rnnoise_init(DenoiseState *st, RNNModel *model);

/**
 * Allocate and initialize a DenoiseState
 *
 * If model is NULL the default model is used.
 *
 * The returned pointer MUST be freed with rnnoise_destroy().
 */
RNNOISE_EXPORT DenoiseState *rnnoise_create(RNNModel *model);

/**
 * Free a DenoiseState produced by rnnoise_create.
 *
 * The optional custom model must be freed by rnnoise_model_free() after.
 */
RNNOISE_EXPORT void rnnoise_destroy(DenoiseState *st);

/**
 * Denoise a frame of samples
 *
 * in and out must be at least rnnoise_get_frame_size() large.
 */
RNNOISE_EXPORT float rnnoise_process_frame(DenoiseState *st, float *out, const float *in);

/**
 * Load a model from a memory buffer
 *
 * It must be deallocated with rnnoise_model_free() and the buffer must remain
 * valid until after the returned object is destroyed.
 */
RNNOISE_EXPORT RNNModel *rnnoise_model_from_buffer(const void *ptr, int len);


/**
 * Load a model from a file
 *
 * It must be deallocated with rnnoise_model_free() and the file must not be
 * closed until the returned object is destroyed.
 */
RNNOISE_EXPORT RNNModel *rnnoise_model_from_file(FILE *f);

/**
 * Load a model from a file name
 *
 * It must be deallocated with rnnoise_model_free()
 */
RNNOISE_EXPORT RNNModel *rnnoise_model_from_filename(const char *filename);

/**
 * Free a custom model
 *
 * It must be called after all the DenoiseStates referring to it are freed.
 */
RNNOISE_EXPORT void rnnoise_model_free(RNNModel *model);

/**
 * Return the number of samples processed per frame at sample_rate Hz.
 *
 * The frame length is derived from the native 480-sample (10 ms) frame:
 * sample_rate/100 samples. Supported sample rates are 48000, 44100 and 32000;
 * other rates return -1.
 */
RNNOISE_EXPORT int rnnoise_sr_get_frame_size(int sample_rate);

/**
 * Allocate and initialize a denoiser for the given input sample rate.
 *
 * 44100 Hz and 32000 Hz inputs are anti-alias filtered and resampled to
 * 48000 Hz internally, denoised, and resampled back to sample_rate. At
 * 48000 Hz the native processing path is used unchanged.
 *
 * The returned pointer MUST be freed with rnnoise_sr_destroy().
 */
RNNOISE_EXPORT DenoiseState *rnnoise_sr_create(RNNModel *model, int sample_rate);

/**
 * Denoise one frame at the sample rate given to rnnoise_sr_create().
 *
 * in and out must each hold rnnoise_sr_get_frame_size(sample_rate) samples,
 * i.e. the same number of samples is consumed and produced on every call.
 */
RNNOISE_EXPORT float rnnoise_sr_process_frame(DenoiseState *st, float *out, const float *in);

/**
 * Free a DenoiseState produced by rnnoise_sr_create().
 */
RNNOISE_EXPORT void rnnoise_sr_destroy(DenoiseState *st);

#ifdef __cplusplus
}
#endif

#endif
