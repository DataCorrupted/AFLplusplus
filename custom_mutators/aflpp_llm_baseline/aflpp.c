// It is a aflpp_llm baseline. Not receive msg from llm. Custom fuzzer randomly chunk the given seed and seed back.
#include "afl-fuzz.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <time.h>

typedef struct my_mutator {

  afl_state_t *afl;

  // Reused buffers:
  u8 *fuzz_buf;

} my_mutator_t;

my_mutator_t *afl_custom_init(afl_state_t *afl, unsigned int seed) {

  srand(seed);
  my_mutator_t *data = calloc(1, sizeof(my_mutator_t));
  if (!data) {

    perror("afl_custom_init alloc");
    return NULL;

  }

  data->fuzz_buf = (u8 *)malloc(MAX_FILE);
  if (!data->fuzz_buf) {

    perror("afl_custom_init malloc");
    return NULL;

  }
  data->afl = afl;

  return data;

}

size_t afl_custom_fuzz(my_mutator_t *data, uint8_t *buf, size_t buf_size,
                       u8 **out_buf, uint8_t *add_buf,
                       size_t add_buf_size,  // add_buf can be NULL
                       size_t max_size) {
  // rand here is not important. this size only be used when did't receive llm seeds, randomly chunk the given seed
  int size = (rand() % 100) + 1;
  
  data->afl->from_llm = false;
  data->afl->unique_id = -1;

  size = buf_size - size > 0 ? buf_size - size : buf_size; //randomly chunk
  size = size <max_size ? size : max_size; // mutated seed length must less than max size
  memset(data->fuzz_buf, buf, size);
  *out_buf = data->fuzz_buf;
  return size;

}

/**
 * Deinitialize everything
 *
 * @param data The data ptr from afl_custom_init
 */
void afl_custom_deinit(my_mutator_t *data) {

  free(data->fuzz_buf);
  free(data);

}