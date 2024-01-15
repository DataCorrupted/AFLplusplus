// This simple example just creates random buffer <= 100 filled with 'A'
#include "afl-fuzz.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <time.h>

#ifndef _FIXED_CHAR
  #define _FIXED_CHAR 0x41
#endif

typedef struct my_mutator {

  afl_state_t *afl;

  // Reused buffers:
  u8 *fuzz_buf;

} my_mutator_t;

int hexCharToVal(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1; // Invalid hex digit
}

void printBuffer(u8 **out_buf, size_t size) {
    if (out_buf == NULL || *out_buf == NULL) {
        printf("Buffer is null.\n");
        return;
    }

    for (size_t i = 0; i < size; i++) {
        printf("%02x ", (*out_buf)[i]);
    }
    printf("\n");
}

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

  int size = (rand() % 100) + 1;
  if (size > max_size) size = max_size;
  
  data->afl->from_llm = false;
  data->afl->unique_id = -1;

  /* the mutation, send request to LLM, then receive mutate seed */
  message_seed_t my_msg;
  int        msg = 200;

  // Create or open the message queue
  int msqid = msgget((key_t)1234, IPC_CREAT | 0666);
  if (msqid == -1) {
    printf("msgget() failed");
  }
  
  // send the request (empty message)
  memcpy(my_msg.data_buff, &msg, sizeof(int));
  my_msg.data_type = TYPE_REQUEST;

  int snd_status = msgsnd(msqid, &my_msg, 0, 0);
  if (snd_status == -1) {
    printf("request send failed");
  }
  // receive seed info from llm
  clock_t start_time;
  start_time = clock();

  // if run time exceed 0.1s then break and mutate default one
  while (((double)(clock() - start_time)) / CLOCKS_PER_SEC < 0.03) {
    int rcv_status = msgrcv(msqid, &my_msg, sizeof(message_seed_t) - sizeof(long), -2, 0);

    if (rcv_status == -1 ) {
      printf("RECEIVE ERROR %d \n",rcv_status);
      break;
    } else {
      // receive non-empty seed(uid+seed)
      if (my_msg.data_type == TYPE_SEED){
        size_t hexLength = strlen(my_msg.data_buff);
        size_t byteLength = hexLength / 2;
        if (MAX_FILE < byteLength) {
          printf("Buffer size %ld is too small for the hex string %ld.\n",MAX_FILE,byteLength);
          break;
        }

        for (size_t i = 0, j = 0; i < hexLength; i += 2, j++) {
            int high = hexCharToVal(my_msg.data_buff[i]);
            int low = hexCharToVal(my_msg.data_buff[i + 1]);

            // Invalid hex section
            if (high == -1 || low == -1) {
                // printf("Invalid hex %c%c section\n", my_msg.data_buff[i],my_msg.data_buff[i+1]);
                continue;
            }
            data->fuzz_buf[j] = (high << 4) | low;
        }

        data->afl->from_llm =true;
        data->afl->unique_id = my_msg.data_num;
        // clear the buffer, update size
        memset(my_msg.data_buff, '\0', sizeof(my_msg.data_buff));
        size = byteLength;
      }
      break;
    }
  }

  if (!data->afl->from_llm){
     memset(data->fuzz_buf, _FIXED_CHAR, size);
  }
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

