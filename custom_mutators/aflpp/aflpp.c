// Custom fuzzer to receive seeds from llm, if not received then randomly chunk the given seed and seed back.
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
  int size = 0;
  
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
  // message size is limited to 2048
  if (buf_size*2+1<=2040){
    for (size_t i=0; i< buf_size;i++){
      sprintf(fuzzer_seed.data_buff + (i * 2), "%02X", buf[i]);
    }
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
  int rcv_status = msgrcv(msqid, &my_msg, sizeof(message_seed_t) - sizeof(long), -2, IPC_NOWAIT);

  if (rcv_status != -1 ) {
    // receive non-empty seed(uid+seed)
    printf("::::rcv_status != -1");
    if (my_msg.data_type == TYPE_SEED){
      printf("::::my_msg.data_type == TYPE_SEED");
      size_t hexLength = strlen(my_msg.data_buff);
      // hex string length is even, my_msg.data_buff has redundancies size
      if (hexLength%2!=0) {
        my_msg.data_buff[hexLength]='0';
        my_msg.data_buff[hexLength+1]='\0';
      }
      // mutated seed length must less than max size
      hexLength = strlen(my_msg.data_buff)<=2*max_size ? strlen(my_msg.data_buff) : 2*max_size;
      size_t byteLength = hexLength / 2;
      
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
    else if (my_msg.data_type == TYPE_TEXT_SEED){
      printf("::::my_msg.data_type == TYPE_TEXT_SEED");
      size_t hexLength = strlen(my_msg.data_buff)+1<=max_size?strlen(my_msg.data_buff)+1 : max_size;
      memcpy(data->fuzz_buf, my_msg.data_buff, strlen(hexLength));
    }
  }
  else {
    // skipping this one mutation if not received from llm
    return 0;
  }
  *out_buf = data->fuzz_buf;
  return size;
}

// If this function is present, no splicing target is passed to the fuzz function. This saves time if splicing data is not needed by the custom fuzzing function. This function is never called, just needs to be present to activate.
void afl_custom_splice_optout(void *data) {}
/**
 * Deinitialize everything
 *
 * @param data The data ptr from afl_custom_init
 */
void afl_custom_deinit(my_mutator_t *data) {

  free(data->fuzz_buf);
  free(data);

}

