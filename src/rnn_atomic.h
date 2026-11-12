/* Copyright (c) 2026 */
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

#ifndef RNNOISE_ATOMIC_H
#define RNNOISE_ATOMIC_H

#if defined(_MSC_VER)
#include <intrin.h>

static inline void *rnn_atomic_exchange_ptr(void *volatile *ptr, void *value) {
  return _InterlockedExchangePointer(ptr, value);
}

static inline long rnn_atomic_add(long volatile *ptr, long value) {
  return _InterlockedExchangeAdd(ptr, value) + value;
}
#elif defined(__GNUC__) || defined(__clang__)
static inline void *rnn_atomic_exchange_ptr(void *volatile *ptr, void *value) {
  return __atomic_exchange_n(ptr, value, __ATOMIC_ACQ_REL);
}

static inline long rnn_atomic_add(long volatile *ptr, long value) {
  return __atomic_add_fetch(ptr, value, __ATOMIC_ACQ_REL);
}
#else
#error "RNNoise requires compiler atomic pointer and integer operations"
#endif

#endif /* RNNOISE_ATOMIC_H */
