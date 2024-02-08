// Custom fuzzer to receive seeds from llm, if not received then just creates random buffer <= 100 filled with 'A'
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
  #define _FIXED_CHAR 0x41 // 0x41 is 'A' to fill into seeds buffer when there is no seeds from llm.
#endif

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
  // rand here is not important. this size only be used when did't receive llm seeds, use default seed 'AAAA...'
  int size = (rand() % 100) + 1;
  
  data->afl->from_llm = false;
  data->afl->unique_id = -1;

  /* the mutation, send request to LLM, then receive mutate seed 
     my_msg.data_buff max size 2048
  */
  message_seed_t my_msg,fuzzer_seed;
  
  // Create or open the message queue
  int msqid = msgget((key_t)1234, IPC_CREAT | 0666);
  if (msqid == -1) {
    printf("msgget() failed");
  }
  
  // send the request with seed from fuzzer
  fuzzer_seed.data_type = TYPE_REQUEST;
  int snd_status;

  if (buf_size*2+1<=3696){
    for (size_t i=0; i< buf_size;i++){
      sprintf(fuzzer_seed.data_buff + (i * 2), "%02X", buf[i]);
    }
    printf("fuzzer %d seed::: %s \n",buf_size, fuzzer_seed.data_buff);
    snd_status = msgsnd(msqid, &fuzzer_seed, buf_size*2+4, 0);
  }
  else{
    snd_status = msgsnd(msqid, &fuzzer_seed, 0, 0);
  }
  memset(fuzzer_seed.data_buff, 0, sizeof(fuzzer_seed.data_buff));

  if (snd_status == -1) {
    printf("request send failed");
  }
  // receive seed info from llm
  clock_t start_time;
  start_time = clock();

  // if run time exceed 0.1s then break and mutate default one
  while (((double)(clock() - start_time)) / CLOCKS_PER_SEC < 0.01) {
    int rcv_status = msgrcv(msqid, &my_msg, sizeof(message_seed_t) - sizeof(long), -2, 0);

    if (rcv_status == -1 ) {
      printf("RECEIVE ERROR %d \n",rcv_status);
      break;
    } else {
      // receive non-empty seed(uid+seed)
      if (my_msg.data_type == TYPE_SEED){
        size_t hexLength = strlen(my_msg.data_buff);
        // makesure the hex string length is even, my_msg.data_buff has redundancies size
        if (hexLength%2!=0) {
          my_msg.data_buff[hexLength]='0';
          my_msg.data_buff[hexLength+1]='\0';
        }
        hexLength = strlen(my_msg.data_buff);
        size_t byteLength = hexLength / 2;
        if (MAX_FILE < byteLength) {
          printf("Buffer size %ld is too small for the hex string %ld.\n",MAX_FILE,byteLength);
          break;
        }
        char pair[3]; // init a tmp buffer to store hex pair
        pair[2]='\0';
        for (size_t i = 0, j = 0; i < hexLength; i += 2, j++) {
            // covert str to long. data->fuzz_buf[j] is an u8 but the convertion result is limited to 255 should be fine
            strncpy(pair, &my_msg.data_buff[i], 2);
            data->fuzz_buf[j] = strtol(pair, NULL, 16); 
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

