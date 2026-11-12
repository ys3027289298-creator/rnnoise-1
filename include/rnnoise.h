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
 * Error codes returned by model loading and hot-swap operations.
 *
 * RNNOISE_ERROR_NONE is zero, so a zero return value always means success.
 */
typedef enum RNNoiseError {
  RNNOISE_ERROR_NONE = 0,
  RNNOISE_ERROR_INVALID_ARGUMENT = 1,
  RNNOISE_ERROR_IO = 2,
  RNNOISE_ERROR_MALFORMED_MODEL = 3,
  RNNOISE_ERROR_INCOMPATIBLE_MODEL = 4,
  RNNOISE_ERROR_OUT_OF_MEMORY = 5,
  RNNOISE_ERROR_NO_MODEL = 6
} RNNoiseError;

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
 * The returned pointer MUST be freed with rnnoise_destroy(). The state takes
 * a reference to model, so the caller may release its own reference at any
 * time.
 */
RNNOISE_EXPORT DenoiseState *rnnoise_create(RNNModel *model);

/**
 * Free a DenoiseState produced by rnnoise_create.
 *
 * This releases the state's reference to its active and pending models.
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
 * valid and unmodified until after the returned object is destroyed.
 */
RNNOISE_EXPORT RNNModel *rnnoise_model_from_buffer(const void *ptr, int len);

/**
 * Load a model from a memory buffer and report a detailed error on failure.
 *
 * This has the same buffer ownership rules as rnnoise_model_from_buffer().
 */
RNNOISE_EXPORT RNNModel *rnnoise_model_from_buffer_ex(const void *ptr, int len,
                                                       RNNoiseError *error);

/**
 * Load a model from a file
 *
 * The file contents are copied into the returned object. It must be
 * deallocated with rnnoise_model_free().
 */
RNNOISE_EXPORT RNNModel *rnnoise_model_from_file(FILE *f);

/**
 * Load a model from a file and report a detailed error on failure.
 */
RNNOISE_EXPORT RNNModel *rnnoise_model_from_file_ex(FILE *f,
                                                    RNNoiseError *error);

/**
 * Load a model from a file name
 *
 * It must be deallocated with rnnoise_model_free()
 */
RNNOISE_EXPORT RNNModel *rnnoise_model_from_filename(const char *filename);

/**
 * Load a model from a file name and report a detailed error on failure.
 */
RNNOISE_EXPORT RNNModel *rnnoise_model_from_filename_ex(const char *filename,
                                                         RNNoiseError *error);

/**
 * Add a reference to a model
 *
 * Models are immutable after they have been loaded and may be shared by any
 * number of DenoiseStates. Each successful rnnoise_model_ref() call must be
 * paired with rnnoise_model_free().
 *
 * Returns model on success, or NULL if model is NULL.
 */
RNNOISE_EXPORT RNNModel *rnnoise_model_ref(RNNModel *model);

/**
 * Free a custom model
 *
 * This releases one reference. The model is destroyed when the last reference
 * is released. DenoiseStates hold their own references, so a model may be
 * freed by its creator after it has been passed to rnnoise_create(),
 * rnnoise_init(), or rnnoise_set_model().
 */
RNNOISE_EXPORT void rnnoise_model_free(RNNModel *model);

/**
 * Queue a model to become active on the next frame boundary.
 *
 * The current model remains active until the next call to
 * rnnoise_process_frame(), at which point the queued model is used for the
 * complete frame. This is safe to call while another thread is processing a
 * frame. The DenoiseState takes a reference to the model.
 *
 * On failure the active model and all processing state are left unchanged.
 * Returns RNNOISE_ERROR_NONE on success, otherwise one of RNNoiseError.
 * If error is non-NULL, it receives the same result.
 */
RNNOISE_EXPORT int rnnoise_set_model(DenoiseState *st, RNNModel *model,
                                     RNNoiseError *error);

/**
 * Load a model from a file name and queue it for the next frame boundary.
 *
 * Loading and validation happen before the active model is changed. On
 * failure the old model continues to be used and error describes the failure.
 * Returns RNNOISE_ERROR_NONE on success, otherwise one of RNNoiseError.
 */
RNNOISE_EXPORT int rnnoise_reload_model(DenoiseState *st, const char *filename,
                                        RNNoiseError *error);

#ifdef __cplusplus
}
#endif

#endif
