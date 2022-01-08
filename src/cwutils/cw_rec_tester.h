#ifndef CW_REC_TESTER_H
#define CW_REC_TESTER_H




#include <stdint.h>
#include "../libcw/libcw_key.h"
#include "test_framework_tools.h"

#include "cw_rec_utils.h"




#if defined(__cplusplus)
extern "C"
{
#endif




/**
   Used to determine size of input data and of buffer for received
   (polled from receiver) characters.
*/
#define REC_TEST_BUFFER_SIZE 4096




typedef struct cw_rec_tester_t {

	/* Whether generating timed events for receiver by test code
	   is in progress. */
	bool generating_in_progress;

	pthread_t receiver_test_code_thread_id;

	char input_string[REC_TEST_BUFFER_SIZE];

	/* Iterator to the array above. */
	size_t input_string_i;

	/* Array large enough to contain characters received (polled)
	   correctly and possible additional characters received
	   incorrectly. */
	char received_string[10 * REC_TEST_BUFFER_SIZE];

	/* Iterator to the array above. */
	size_t received_string_i;

	cw_gen_t * gen;
	cw_key_t key;

	cwtest_param_ranger_t speed_ranger;

	/* Parameters used in "compare" function that verifies if
	   input and received strings are similar enough to pass the
	   test. */
	float acceptable_error_rate_percent; /* [percents] */
	size_t acceptable_last_mismatch_index;

	/* Input variable for the test. Decreasing or increasing
	   decides how many characters are enqueued with the same
	   speed S1. Next batch of characters will be enqueued with
	   another speed S2. Depending on how long it will take to
	   dequeue this batch, the difference between S2 and S1 may be
	   significant and this will throw receiver off. */
	int characters_to_enqueue;

} cw_rec_tester_t;




/**
   @brief Initialize @p tester variable
*/
void cw_rec_tester_init(cw_rec_tester_t * tester);




void cw_rec_tester_configure(cw_rec_tester_t * tester, cw_easy_receiver_t * easy_rec, bool use_ranger);

void cw_rec_tester_start_test_code(cw_rec_tester_t * tester);

void cw_rec_tester_stop_test_code(cw_rec_tester_t * tester);




/**
   @brief See how well the receiver has received the data

   Compare buffers with text that was sent to test generator and text
   that was received from tested production receiver.

   Compare input text with what the receiver received.

   @return 0 if received text is similar enough to input text
   @return -1 otherwisee
*/
int cw_rec_tester_evaluate_receive_correctness(cw_rec_tester_t * tester);




int cw_rec_tester_on_character(cw_rec_tester_t * tester, cw_rec_data_t * erd, struct timeval * timer);
int cw_rec_tester_on_space(cw_rec_tester_t * tester, cw_rec_data_t * erd, struct timeval * timer);




#if defined(__cplusplus)
}
#endif




#endif /* #ifndef CW_REC_TESTER_H */
