#include "../libcw2.h"
#include "../libcw_gen.h"
#include "../libcw_utils.h"
#include "libcw_gen_tests_state_callback.h"




/* With this input string we expect 18 high states for dots and dashes,
   17 low states for inter-mark spaces + 1 low state for ending space. */
static const char * input_string = "ooosss";
#define TIMES_COUNT 36

#define CW_EOE_REPRESENTATION '^'


typedef struct element_stats_t {
	int duration_min;
	int duration_max;
	int duration_total;
	int count;
} element_stats_t;




/* Type of data passed to callback.
   We need to store a persistent state of some data between callback calls. */
typedef struct callback_data_t {
	struct timeval prev_timestamp;
	int counter;
} callback_data_t;



static void gen_callback_fn(void * callback_arg, int state);
static void update_element_stats(element_stats_t * stats, int element_duration);
static void print_element_stats(const element_stats_t * stats, const char * name, int duration_expected);




struct data {
	char representation;
	int duration; /* microseconds */
} times[TIMES_COUNT] = {
	/* "o" */
	{ CW_DASH_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION,  0 },
	{ CW_DASH_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION,  0 },
	{ CW_DASH_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION,  0 },

	/* "o" */
	{ CW_DASH_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION,  0 },
	{ CW_DASH_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION,  0 },
	{ CW_DASH_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION,  0 },

	/* "o" */
	{ CW_DASH_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION,  0 },
	{ CW_DASH_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION,  0 },
	{ CW_DASH_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION,  0 },

	/* "s" */
	{ CW_DOT_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION, 0 },
	{ CW_DOT_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION, 0 },
	{ CW_DOT_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION, 0 },

	/* "s" */
	{ CW_DOT_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION, 0 },
	{ CW_DOT_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION, 0 },
	{ CW_DOT_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION, 0 },

	/* "s" */
	{ CW_DOT_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION, 0 },
	{ CW_DOT_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION, 0 },
	{ CW_DOT_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION, 0 },
};




static void gen_callback_fn(void * callback_arg, int state)
{
	callback_data_t * callback_data = (callback_data_t *) callback_arg;

	struct timeval now_timestamp = { 0 };
	gettimeofday(&now_timestamp, NULL);

	if (callback_data->counter) {
		const int diff = cw_timestamp_compare_internal(&callback_data->prev_timestamp, &now_timestamp);
		fprintf(stderr, "Call %3d, state %d, representation = '%c', duration of previous element = %6d us\n",
			callback_data->counter, state, times[callback_data->counter].representation, diff);

		/* Notice that we do -1 here. We are at the beginning
		   of new element, and currently calculated diff is
		   how long *previous* element was. */
		times[callback_data->counter - 1].duration = diff;

		if (state) {
			if (CW_DOT_REPRESENTATION != times[callback_data->counter].representation
			    && CW_DASH_REPRESENTATION != times[callback_data->counter].representation) {
				fprintf(stderr, "[EE] Unexpected representation '%c' at %d for state 'closed'\n",
					times[callback_data->counter].representation,
					callback_data->counter);
			}
		} else {
			if (CW_EOE_REPRESENTATION != times[callback_data->counter].representation) {
				fprintf(stderr, "[EE] Unexpected representation '%c' at %d for state 'open'\n",
					times[callback_data->counter].representation,
					callback_data->counter);
			}
		}

	}
	callback_data->counter++;
	callback_data->prev_timestamp = now_timestamp;
}




static void update_element_stats(element_stats_t * stats, int element_duration)
{
	stats->duration_total += element_duration;
	stats->count++;

	if (element_duration > stats->duration_max) {
		stats->duration_max = element_duration;
	}
	if (element_duration < stats->duration_min) {
		stats->duration_min = element_duration;
	}
}




static void print_element_stats(const element_stats_t * stats, const char * name, int duration_expected)
{
	const int average = stats->duration_total / stats->count;

	const double divergence_min = 100 * (stats->duration_min - duration_expected) / (1.0 * duration_expected);
	const double divergence_avg = 100 * (average - duration_expected) / (1.0 * duration_expected);
	const double divergence_max = 100 * (stats->duration_max - duration_expected) / (1.0 * duration_expected);

	fprintf(stderr, "[II] duration of %7s: min/avg/max = %6d/%6d/%6d, expected = %6d, divergence min/avg/max = %8.3f%%/%8.3f%%/%8.3f%%\n",
		name,
		stats->duration_min,
		average,
		stats->duration_max,
		duration_expected,
		divergence_min,
		divergence_avg,
		divergence_max);
}




int test_cw_gen_state_callback(cw_test_executor_t * cte)
{
	const int max = cte->get_repetitions_count(cte);

	cte->print_test_header(cte, "%s (%d)", __func__, max);



	cw_gen_t * gen = cw_gen_new(cte->current_sound_system, NULL);
	cw_gen_set_speed(gen, cte->config->send_speed);
	cw_gen_set_frequency(gen, cte->config->frequency);


	callback_data_t callback_data = { 0 };
	cw_gen_register_state_tracking_callback_internal(gen, gen_callback_fn, &callback_data);



	int dot_usecs, dash_usecs,
		end_of_element_usecs,
		end_of_character_usecs, end_of_word_usecs,
		additional_usecs, adjustment_usecs;
	cw_gen_get_timing_parameters_internal(gen,
					      &dot_usecs, &dash_usecs,
					      &end_of_element_usecs,
					      &end_of_character_usecs, &end_of_word_usecs,
					      &additional_usecs, &adjustment_usecs);
	fprintf(stderr,
		"dot duration  = %6d us\n"
		"dash duration = %6d us\n"
		"eoe duration  = %6d us\n"
		"eoc duration  = %6d us\n"
		"eow duration  = %6d us\n"
		"additional duration = %6d us\n"
		"adjustment duration = %6d us\n",
		dot_usecs, dash_usecs,
		end_of_element_usecs,
		end_of_character_usecs, end_of_word_usecs,
		additional_usecs, adjustment_usecs);


	cw_gen_start(gen);
	cw_gen_enqueue_string(gen, input_string);
	cw_gen_wait_for_queue_level(gen, 0);


	cw_gen_stop(gen);
	cw_gen_delete(&gen);


	{
		element_stats_t dashes = { .duration_min = 1000000000, .duration_max = 0, .duration_total = 0, .count = 0 };
		element_stats_t dots = { .duration_min = 1000000000, .duration_max = 0, .duration_total = 0, .count = 0 };

		/* Skip first two elements and a last element. The way
		   the test is structured may impact correctness of
		   values of these elements. TODO: make the elements
		   correct. */
		for (int i = 2; i < TIMES_COUNT - 1; i++) {
			switch (times[i].representation) {
			case CW_DASH_REPRESENTATION:
				update_element_stats(&dashes, times[i].duration);
				break;
			case CW_DOT_REPRESENTATION:
				update_element_stats(&dots, times[i].duration);
				break;
			case CW_EOE_REPRESENTATION:
				/* TODO: implement. */
				break;
			default:
				break;
			}
		}

		print_element_stats(&dashes, "dashes", dash_usecs);
		print_element_stats(&dots, "dots", dot_usecs);

		/* Clear durations before next call of this test function. */
		for (int i = 0; i < TIMES_COUNT; i++) {
			times[i].duration = 0;
		}
	}

	return 0;
}
