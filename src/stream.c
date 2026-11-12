/* Copyright (c) 2024 Mozilla */
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "rnnoise.h"

struct RNNStreamState {
  DenoiseState *st;
  /* Matches FRAME_SIZE returned by rnnoise_get_frame_size(). */
  float frame_buf[480];
  int fill;
};

RNNStreamState *rnnoise_stream_create(RNNModel *model) {
  RNNStreamState *s;
  s = (RNNStreamState *)malloc(sizeof(*s));
  if (s == NULL) return NULL;
  s->st = rnnoise_create(model);
  if (s->st == NULL) {
    free(s);
    return NULL;
  }
  s->fill = 0;
  return s;
}

void rnnoise_stream_destroy(RNNStreamState *s) {
  if (s == NULL) return;
  rnnoise_destroy(s->st);
  free(s);
}

int rnnoise_stream_process(RNNStreamState *s, float *out, const float *in, int n) {
  int frame_size;
  int produced;
  int pos;
  if (n <= 0) return 0;
  frame_size = rnnoise_get_frame_size();
  produced = 0;
  pos = 0;
  while (pos < n) {
    int take;
    take = frame_size - s->fill;
    if (take > n - pos) take = n - pos;
    memcpy(&s->frame_buf[s->fill], &in[pos], (size_t)take*sizeof(float));
    s->fill += take;
    pos += take;
    if (s->fill < frame_size) break;
    rnnoise_process_frame(s->st, &out[produced], s->frame_buf);
    produced += frame_size;
    s->fill = 0;
  }
  return produced;
}

int rnnoise_stream_flush(RNNStreamState *s, float *out) {
  int frame_size;
  int remain;
  frame_size = rnnoise_get_frame_size();
  remain = s->fill;
  if (remain == 0) return 0;
  memset(&s->frame_buf[remain], 0, (size_t)(frame_size-remain)*sizeof(float));
  rnnoise_process_frame(s->st, out, s->frame_buf);
  s->fill = 0;
  return remain;
}
