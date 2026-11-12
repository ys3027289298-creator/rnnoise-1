#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "nnet.h"
#include "rnnoise.h"

#define FRAME_SIZE 480
#define WARM_FRAMES 20
#define SWITCH_FRAMES 120
#define SWITCH_PERIOD 10

static void fail(const char *message) {
  fprintf(stderr, "FAIL: %s\n", message);
  exit(1);
}

static unsigned char *read_entire_file(const char *path, int *length) {
  FILE *f;
  long file_size;
  unsigned char *data;
  f = fopen(path, "rb");
  if (f == NULL) return NULL;
  if (fseek(f, 0, SEEK_END) != 0 || (file_size = ftell(f)) <= 0 ||
      file_size > 0x7fffffffL || fseek(f, 0, SEEK_SET) != 0) {
    fclose(f);
    return NULL;
  }
  data = malloc((size_t)file_size);
  if (data == NULL) {
    fclose(f);
    return NULL;
  }
  if (fread(data, (size_t)file_size, 1, f) != 1) {
    free(data);
    fclose(f);
    return NULL;
  }
  fclose(f);
  *length = (int)file_size;
  return data;
}

static int write_entire_file(const char *path, const unsigned char *data,
                             int length) {
  FILE *f = fopen(path, "wb");
  if (f == NULL) return -1;
  if (fwrite(data, (size_t)length, 1, f) != 1) {
    fclose(f);
    return -1;
  }
  fclose(f);
  return 0;
}

static size_t find_record(const unsigned char *data, int length,
                          const char *name) {
  size_t pos = 0;
  while (pos + WEIGHT_BLOCK_SIZE <= (size_t)length) {
    const WeightHead *head = (const WeightHead *)(data + pos);
    if (memcmp(head->head, "DNNw", 4) != 0 || head->block_size < 0 ||
        head->block_size > length - pos - WEIGHT_BLOCK_SIZE) {
      return (size_t)-1;
    }
    if (strcmp(head->name, name) == 0) return pos;
    pos += WEIGHT_BLOCK_SIZE + (size_t)head->block_size;
  }
  return (size_t)-1;
}

static unsigned char *make_second_model(const unsigned char *blob, int length) {
  size_t pos;
  float *bias;
  int i;
  unsigned char *variant = malloc((size_t)length);
  if (variant == NULL) fail("allocating second model");
  memcpy(variant, blob, (size_t)length);
  pos = find_record(variant, length, "dense_out_bias");
  if (pos == (size_t)-1) fail("dense_out_bias not found");
  if (((const WeightHead *)(variant + pos))->size != 32 * (int)sizeof(float))
    fail("unexpected dense_out_bias size");
  bias = (float *)(variant + pos + WEIGHT_BLOCK_SIZE);
  for (i=0;i<32;i++) bias[i] += 0.2f;
  return variant;
}

static unsigned char *make_corrupt_model(const unsigned char *blob, int length) {
  unsigned char *variant = malloc((size_t)length);
  if (variant == NULL) fail("allocating corrupt model");
  memcpy(variant, blob, (size_t)length);
  memcpy(variant, "XXXX", 4);
  return variant;
}

static unsigned char *make_incompatible_model(const unsigned char *blob,
                                              int length) {
  size_t pos;
  WeightHead *head;
  unsigned char *variant = malloc((size_t)length);
  if (variant == NULL) fail("allocating incompatible model");
  memcpy(variant, blob, (size_t)length);
  pos = find_record(variant, length, "conv1_bias");
  if (pos == (size_t)-1) fail("conv1_bias not found");
  head = (WeightHead *)(variant + pos);
  head->size -= (int)sizeof(float);
  return variant;
}

static void fill_input(float *input, int frame, unsigned *noise_state) {
  int i;
  for (i=0;i<FRAME_SIZE;i++) {
    float noise;
    int sample = frame * FRAME_SIZE + i;
    noise_state[0] = noise_state[0] * 1664525u + 1013904223u;
    noise = ((int)(noise_state[0] >> 9) / 4194304.0f - 1.0f) * 20.0f;
    input[i] = 5000.0f * sinf(2.0f * 3.14159265f * 200.0f * sample / 48000.0f)
             + noise;
  }
}

static void check_finite_output(const float *output, int frame) {
  int i;
  for (i=0;i<FRAME_SIZE;i++) {
    if (!isfinite(output[i])) {
      fprintf(stderr, "non-finite output at frame %d sample %d\n", frame, i);
      exit(1);
    }
  }
}

static void test_loader_errors(const char *corrupt_path,
                               const char *incompatible_path) {
  RNNoiseError error = RNNOISE_ERROR_NONE;
  RNNModel *model;
  model = rnnoise_model_from_filename_ex(corrupt_path, &error);
  if (model != NULL) fail("corrupt model unexpectedly loaded");
  if (error != RNNOISE_ERROR_MALFORMED_MODEL)
    fail("corrupt model did not report RNNOISE_ERROR_MALFORMED_MODEL");
  model = rnnoise_model_from_filename_ex(incompatible_path, &error);
  if (model != NULL) fail("incompatible model unexpectedly loaded");
  if (error != RNNOISE_ERROR_INCOMPATIBLE_MODEL)
    fail("incompatible model did not report RNNOISE_ERROR_INCOMPATIBLE_MODEL");
  model = rnnoise_model_from_filename_ex("/definitely/not/a/model", &error);
  if (model != NULL) fail("missing model unexpectedly loaded");
  if (error != RNNOISE_ERROR_IO)
    fail("missing model did not report RNNOISE_ERROR_IO");
  printf("loader errors: malformed=%d incompatible=%d io=%d\n",
         RNNOISE_ERROR_MALFORMED_MODEL, RNNOISE_ERROR_INCOMPATIBLE_MODEL,
         RNNOISE_ERROR_IO);
}

static float test_frame_switching(RNNModel *model_a, RNNModel *model_b) {
  DenoiseState *st = rnnoise_create(model_a);
  DenoiseState *reference = rnnoise_create(model_a);
  float input[FRAME_SIZE];
  float output[FRAME_SIZE];
  float reference_output[FRAME_SIZE];
  unsigned noise_state = 1;
  int frame;
  int switched = 0;
  float max_step = 0;
  float max_switch_step = 0;
  float previous = 0;
  float max_model_difference = 0;
  if (st == NULL || reference == NULL) fail("creating hot-swap test states");

  for (frame=0;frame<WARM_FRAMES + SWITCH_FRAMES;frame++) {
    int i;
    fill_input(input, frame, &noise_state);
    if (frame >= WARM_FRAMES && frame % SWITCH_PERIOD == 0) {
      RNNModel *next = ((frame / SWITCH_PERIOD) & 1) ? model_a : model_b;
      RNNoiseError error = RNNOISE_ERROR_NONE;
      if (rnnoise_set_model(st, next, &error) != RNNOISE_ERROR_NONE ||
          error != RNNOISE_ERROR_NONE)
        fail("rnnoise_set_model failed");
      switched = 1;
    }
    rnnoise_process_frame(st, output, input);
    rnnoise_process_frame(reference, reference_output, input);
    check_finite_output(output, frame);
    for (i=0;i<FRAME_SIZE;i++) {
      float step = fabsf(output[i] - (i == 0 ? previous : output[i-1]));
      float difference = fabsf(output[i] - reference_output[i]);
      if (step > max_step) max_step = step;
      if (frame >= WARM_FRAMES && step > max_switch_step)
        max_switch_step = step;
      if (frame >= WARM_FRAMES && difference > max_model_difference)
        max_model_difference = difference;
    }
    previous = output[FRAME_SIZE-1];
  }

  if (!switched) fail("model was never switched");
  if (max_model_difference < 1e-3f)
    fail("switching models did not change the output");
  if (max_switch_step > 1000.0f) {
    fprintf(stderr, "output discontinuity: max step %.3f\n", max_switch_step);
    fail("model switch produced an output pop");
  }
  printf("hot swap: max_step=%.3f switch_step=%.3f model_diff=%.3f\n",
         max_step, max_switch_step, max_model_difference);
  rnnoise_destroy(st);
  rnnoise_destroy(reference);
  return max_switch_step;
}

static void test_failed_reload_preserves_model(const char *corrupt_path,
                                               const char *incompatible_path,
                                               const char *model_path) {
  RNNModel *model = rnnoise_model_from_filename_ex(model_path, NULL);
  DenoiseState *st;
  DenoiseState *reference;
  float input[FRAME_SIZE];
  float output[FRAME_SIZE];
  float reference_output[FRAME_SIZE];
  unsigned noise_state = 7;
  int frame;
  RNNoiseError error = RNNOISE_ERROR_NONE;
  if (model == NULL) fail("loading model for reload failure test");
  st = rnnoise_create(model);
  reference = rnnoise_create(model);
  if (st == NULL || reference == NULL) fail("creating reload test states");

  for (frame=0;frame<20;frame++) {
    fill_input(input, frame, &noise_state);
    rnnoise_process_frame(st, output, input);
    rnnoise_process_frame(reference, reference_output, input);
  }
  if (rnnoise_reload_model(st, corrupt_path, &error) !=
      RNNOISE_ERROR_MALFORMED_MODEL || error != RNNOISE_ERROR_MALFORMED_MODEL)
    fail("corrupt reload did not return malformed-model error");
  if (rnnoise_reload_model(st, incompatible_path, &error) !=
      RNNOISE_ERROR_INCOMPATIBLE_MODEL ||
      error != RNNOISE_ERROR_INCOMPATIBLE_MODEL)
    fail("incompatible reload did not return incompatible-model error");

  for (frame=20;frame<40;frame++) {
    fill_input(input, frame, &noise_state);
    rnnoise_process_frame(st, output, input);
    rnnoise_process_frame(reference, reference_output, input);
    if (memcmp(output, reference_output, sizeof(output)) != 0) {
      fprintf(stderr, "frame %d differs after failed reload\n", frame);
      fail("failed reload changed the active model or processing state");
    }
  }
  printf("failed reloads preserved active model\n");
  rnnoise_destroy(st);
  rnnoise_destroy(reference);
  rnnoise_model_free(model);
}

static void test_shared_models(const char *model_path, RNNModel *model_b) {
  RNNModel *model_a = rnnoise_model_from_filename_ex(model_path, NULL);
  DenoiseState *st_a;
  DenoiseState *st_b;
  DenoiseState *st_c;
  float input[FRAME_SIZE];
  float output[FRAME_SIZE];
  unsigned noise_state = 99;
  int frame;
  if (model_a == NULL) fail("loading shared model");
  st_a = rnnoise_create(model_a);
  st_b = rnnoise_create(model_b);
  st_c = rnnoise_create(model_a);
  if (st_a == NULL || st_b == NULL || st_c == NULL)
    fail("creating shared-model states");
  rnnoise_model_free(model_a);
  for (frame=0;frame<20;frame++) {
    fill_input(input, frame, &noise_state);
    rnnoise_process_frame(st_a, output, input);
    check_finite_output(output, frame);
    rnnoise_process_frame(st_b, output, input);
    check_finite_output(output, frame);
    rnnoise_process_frame(st_c, output, input);
    check_finite_output(output, frame);
  }
  rnnoise_destroy(st_a);
  rnnoise_destroy(st_b);
  rnnoise_destroy(st_c);
  printf("shared models: creator references released while states active\n");
}

typedef struct {
  DenoiseState *st;
  atomic_int *running;
  int frames;
  float max_step;
} InferenceArgs;

typedef struct {
  DenoiseState *st1;
  DenoiseState *st2;
  RNNModel *model_a;
  RNNModel *model_b;
  atomic_int *running;
  int switches;
} SwapArgs;

static void *inference_thread(void *ptr) {
  InferenceArgs *args = ptr;
  float input[FRAME_SIZE];
  float output[FRAME_SIZE];
  unsigned noise_state = (unsigned)(size_t)args->st;
  float previous = 0;
  int frame;
  for (frame=0;frame<args->frames;frame++) {
    int i;
    fill_input(input, frame, &noise_state);
    rnnoise_process_frame(args->st, output, input);
    check_finite_output(output, frame);
    for (i=0;i<FRAME_SIZE;i++) {
      float step = fabsf(output[i] - (i == 0 ? previous : output[i-1]));
      if (step > args->max_step) args->max_step = step;
    }
    previous = output[FRAME_SIZE-1];
  }
  atomic_fetch_sub(args->running, 1);
  return NULL;
}

static void *swap_thread(void *ptr) {
  SwapArgs *args = ptr;
  int i;
  for (i=0;i<args->switches;i++) {
    RNNoiseError error = RNNOISE_ERROR_NONE;
    DenoiseState *st = (i & 1) ? args->st1 : args->st2;
    RNNModel *model = (i & 2) ? args->model_a : args->model_b;
    if (rnnoise_set_model(st, model, &error) != RNNOISE_ERROR_NONE ||
        error != RNNOISE_ERROR_NONE)
      fail("concurrent rnnoise_set_model failed");
  }
  return NULL;
}

static void test_concurrency(RNNModel *model_a, RNNModel *model_b) {
  DenoiseState *st1 = rnnoise_create(model_a);
  DenoiseState *st2 = rnnoise_create(model_b);
  atomic_int running = 2;
  InferenceArgs args1;
  InferenceArgs args2;
  SwapArgs swap_args;
  pthread_t infer1;
  pthread_t infer2;
  pthread_t swapper;
  if (st1 == NULL || st2 == NULL) fail("creating concurrency test states");
  args1.st = st1;
  args1.running = &running;
  args1.frames = 1200;
  args1.max_step = 0;
  args2.st = st2;
  args2.running = &running;
  args2.frames = 1200;
  args2.max_step = 0;
  swap_args.st1 = st1;
  swap_args.st2 = st2;
  swap_args.model_a = model_a;
  swap_args.model_b = model_b;
  swap_args.running = &running;
  swap_args.switches = 50000;
  if (pthread_create(&infer1, NULL, inference_thread, &args1) != 0 ||
      pthread_create(&infer2, NULL, inference_thread, &args2) != 0 ||
      pthread_create(&swapper, NULL, swap_thread, &swap_args) != 0)
    fail("creating test threads");
  pthread_join(infer1, NULL);
  pthread_join(infer2, NULL);
  pthread_join(swapper, NULL);
  if (args1.max_step > 1000 || args2.max_step > 1000)
    fail("concurrent model switching produced an output pop");
  printf("concurrency: 2 inference threads + 1 swap thread, max_step=(%.3f, %.3f)\n",
         args1.max_step, args2.max_step);
  rnnoise_destroy(st1);
  rnnoise_destroy(st2);
}

int main(void) {
  const char *model_path = getenv("RNNOISE_TEST_MODEL");
  char corrupt_path[256];
  char incompatible_path[256];
  unsigned char *blob;
  unsigned char *blob_b;
  unsigned char *corrupt;
  unsigned char *incompatible;
  int length;
  int corrupt_length;
  int incompatible_length;
  RNNModel *model_a;
  RNNModel *model_b;

  if (model_path == NULL || model_path[0] == '\0')
    model_path = "weights_blob.bin";
  blob = read_entire_file(model_path, &length);
  if (blob == NULL) fail("reading test model");
  blob_b = make_second_model(blob, length);
  corrupt = make_corrupt_model(blob, length);
  corrupt_length = length;
  incompatible = make_incompatible_model(blob, length);
  incompatible_length = length;
  snprintf(corrupt_path, sizeof(corrupt_path),
           "/tmp/rnnoise_hot_swap_corrupt_%ld.bin", (long)getpid());
  snprintf(incompatible_path, sizeof(incompatible_path),
           "/tmp/rnnoise_hot_swap_incompatible_%ld.bin", (long)getpid());
  if (write_entire_file(corrupt_path, corrupt, corrupt_length) != 0 ||
      write_entire_file(incompatible_path, incompatible, incompatible_length) != 0)
    fail("writing invalid test models");

  test_loader_errors(corrupt_path, incompatible_path);
  model_b = rnnoise_model_from_buffer_ex(blob_b, length, NULL);
  if (model_b == NULL) fail("loading second model");

  model_a = rnnoise_model_from_filename_ex(model_path, NULL);
  if (model_a == NULL) fail("loading first model");
  test_frame_switching(model_a, model_b);
  test_failed_reload_preserves_model(corrupt_path, incompatible_path,
                                     model_path);
  test_shared_models(model_path, model_b);
  test_concurrency(model_a, model_b);

  unlink(corrupt_path);
  unlink(incompatible_path);
  free(blob);
  free(blob_b);
  free(corrupt);
  free(incompatible);
  rnnoise_model_free(model_a);
  rnnoise_model_free(model_b);
  printf("all hot-swap tests passed\n");
  return 0;
}
