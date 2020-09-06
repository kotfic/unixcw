#include <math.h> /* fabs() */

#include "../libcw2.h"
#include "../libcw_gen.h"
#include "../libcw_utils.h"
#include "libcw_gen_tests_state_callback.h"




/*
  This test is verifying if callback called on each change of state of
  generator is called at proper intervals.

  Client code can use cw_gen_register_value_tracking_callback_internal() to
  register a callback. The callback will be called each time the generator
  changes it's state between mark/space. The changes of state should occur at
  time intervals specified by length of marks (dots, dashes) and spaces.

  libcw is using sound card's (sound system's) blocking write property to
  measure how often and for how long a generator should stay in particular
  state. Generator will write e.g. mark to sound system, the blocking write
  will block the generator for specified time, and then the generator will be
  able to get from tone queue the next element (mark or space) and do another
  blocking write to sound system.

  Calls to the callback are made between each blocking write. The calls to
  the callback will be made with smaller or larger precision, depending on:

  1. the exact mechanism used by libcw. For now libcw uses blocking write,
     but e.g. ALSA is offering other mechanisms that are right now
     unexplored.

  2. how well the libcw has configured the mechanism. Commit
     cb99e7884dc5370519dc3a3eacfb3184959b0f87 has fixed an error in
     configuration of ALSA's HW period size. That incorrect configuration has
     led to the callback being called at totally invalid (very, very short)
     intervals.

  I started writing this test to verify that the fix from commit
  cb99e7884dc5370519dc3a3eacfb3184959b0f87 has been working, but then I
  thought that the test can work for any sound system supported by libcw. I
  don't need to verify if low-level configuration of ALSA is working
  correctly, I just need to verify if the callback mechanism (relying on
  proper configuration of a sound system) is working correctly.
*/




/*
  With this input string we expect:
  6*3 letters, each with 3 marks and 3 inter-mark-spaces = 108.
*/
static const char * input_string = "ooo""ooo""ooo""sss""sss""sss";
#define INPUT_ELEMENTS_COUNT 108

#define CW_EOE_REPRESENTATION '^'




typedef struct divergence_t {
	double min;
	double avg;
	double max;
} divergence_t;




typedef struct cw_element_t {
	char representation;
	int duration; /* microseconds */
} cw_element_t;




typedef struct cw_element_stats_t {
	int duration_min;
	int duration_avg;
	int duration_max;

	int duration_total;
	int count;
} cw_element_stats_t;




/* Type of data passed to callback.
   We need to store a persistent state of some data between callback calls. */
typedef struct callback_data_t {
	struct timeval prev_timestamp; /* Timestamp at which previous callback was made. */
	int counter;
} callback_data_t;




typedef struct test_data_t {
	/* Sound system for which we have reference data, and with which the
	   current run of a test should be done. */
	enum cw_audio_systems sound_system;

	/* Speed (WPM) for which we have reference data, and at which current
	   run of a test should be done (otherwise we will be comparing
	   results of tests made in different conditions). */
	int speed;

	/* Reference values from tests in post_3.5.1 branch. */
	struct divergence_t reference_div_dots;
	struct divergence_t reference_div_dashes;

	/* Values obtained in current test run. */
	struct divergence_t current_div_dots;
	struct divergence_t current_div_dashes;
} test_data_t;




static void gen_callback_fn(void * callback_arg, int state);
static void update_element_stats(cw_element_stats_t * stats, int element_duration);
static void print_element_stats_and_divergences(const cw_element_stats_t * stats, const divergence_t * divergences, const char * name, int duration_expected);
static void calculate_divergences_from_stats(const cw_element_stats_t * stats, divergence_t * divergences, int duration_expected);
static cwt_retv test_cw_gen_state_callback_sub(cw_test_executor_t * cte, test_data_t * test_data, const char * sound_device);

static void calculate_test_results(const cw_element_t * test_input_elements, test_data_t * test_data, int dot_usecs, int dash_usecs);
static void evaluate_test_results(cw_test_executor_t * cte, test_data_t * test_data);
static void clear_data(cw_element_t * test_input_elements);




/**
  Results of test from reference branch post_3.5.1 with:
  1. fixed ALSA HW period size,
  2. modified PA parameters, copied from these two commits:
  https://github.com/m5evt/unixcw-3.5.1/commit/2d5491a461587ac4686e2d1b897619c98be05c9e
  https://github.com/m5evt/unixcw-3.5.1/commit/c86785b595a6d711aae915150df2ccb848ace05c

Null sound system, 4 WMP
[II] duration of  dashes: min/avg/max = 900130/900208/900239, expected = 900000, divergence min/avg/max =    0.014%/   0.023%/   0.027%
[II] duration of    dots: min/avg/max = 300123/300204/300225, expected = 300000, divergence min/avg/max =    0.041%/   0.068%/   0.075%
ALSA sound system, 4 WPM
[II] duration of  dashes: min/avg/max = 892462/895727/898281, expected = 900000, divergence min/avg/max =   -0.838%/  -0.475%/  -0.191%
[II] duration of    dots: min/avg/max = 293087/296211/299484, expected = 300000, divergence min/avg/max =   -2.304%/  -1.263%/  -0.172%
PulseAudio sound system, 4 WPM
[II] duration of  dashes: min/avg/max = 897406/900200/906788, expected = 900000, divergence min/avg/max =   -0.288%/   0.022%/   0.754%
[II] duration of    dots: min/avg/max = 294511/299532/303330, expected = 300000, divergence min/avg/max =   -1.830%/  -0.156%/   1.110%


Null sound system, 12 WPM
[II] duration of  dashes: min/avg/max = 300102/300192/300238, expected = 300000, divergence min/avg/max =    0.034%/   0.064%/   0.079%
[II] duration of    dots: min/avg/max = 100113/100189/100241, expected = 100000, divergence min/avg/max =    0.113%/   0.189%/   0.241%
ALSA sound system, 12 WPM
[II] duration of  dashes: min/avg/max = 292760/295770/299096, expected = 300000, divergence min/avg/max =   -2.413%/  -1.410%/  -0.301%
[II] duration of    dots: min/avg/max =  93308/ 96458/ 98940, expected = 100000, divergence min/avg/max =   -6.692%/  -3.542%/  -1.060%
PulseAudio sound system, 12 WPM
[II] duration of  dashes: min/avg/max = 295562/299878/303009, expected = 300000, divergence min/avg/max =   -1.479%/  -0.041%/   1.003%
[II] duration of    dots: min/avg/max =  97274/ 99937/106259, expected = 100000, divergence min/avg/max =   -2.726%/  -0.063%/   6.259%


Null sound system, 24 WPM
[II] duration of  dashes: min/avg/max = 150116/150197/150255, expected = 150000, divergence min/avg/max =    0.077%/   0.131%/   0.170%
[II] duration of    dots: min/avg/max =  50110/ 50158/ 50222, expected =  50000, divergence min/avg/max =    0.220%/   0.316%/   0.444%
ALSA sound system, 24 WPM
[II] duration of  dashes: min/avg/max = 141977/147780/154997, expected = 150000, divergence min/avg/max =   -5.349%/  -1.480%/   3.331%
[II] duration of    dots: min/avg/max =  44731/ 47361/ 49472, expected =  50000, divergence min/avg/max =  -10.538%/  -5.278%/  -1.056%
PulseAudio sound system, 24 WPM
[II] duration of  dashes: min/avg/max = 142998/149541/152615, expected = 150000, divergence min/avg/max =   -4.668%/  -0.306%/   1.743%
[II] duration of    dots: min/avg/max =  45374/ 50002/ 53423, expected =  50000, divergence min/avg/max =   -9.252%/   0.004%/   6.846%


Null sound system, 36 WPM
[II] duration of  dashes: min/avg/max = 100149/100217/100260, expected =  99999, divergence min/avg/max =    0.150%/   0.218%/   0.261%
[II] duration of    dots: min/avg/max =  33451/ 33497/ 33567, expected =  33333, divergence min/avg/max =    0.354%/   0.492%/   0.702%
ALSA sound system, 36 WPM
[II] duration of  dashes: min/avg/max =  93665/ 98788/106536, expected =  99999, divergence min/avg/max =   -6.334%/  -1.211%/   6.537%
[II] duration of    dots: min/avg/max =  28537/ 32776/ 41330, expected =  33333, divergence min/avg/max =  -14.388%/  -1.671%/  23.991%
PulseAudio sound system, 36 WPM
[II] duration of  dashes: min/avg/max =  97477/100152/105279, expected =  99999, divergence min/avg/max =   -2.522%/   0.153%/   5.280%
[II] duration of    dots: min/avg/max =  27914/ 33103/ 35747, expected =  33333, divergence min/avg/max =  -16.257%/  -0.690%/   7.242%


Null sound system, 60WMP
[II] duration of  dashes: min/avg/max =  60143/ 60206/ 60256, expected =  60000, divergence min/avg/max =    0.238%/   0.343%/   0.427%
[II] duration of    dots: min/avg/max =  20126/ 20178/ 20229, expected =  20000, divergence min/avg/max =    0.630%/   0.890%/   1.145%
ALSA sound system, 60WMP
[II] duration of  dashes: min/avg/max =  52534/ 56223/ 57938, expected =  60000, divergence min/avg/max =  -12.443%/  -6.295%/  -3.437%
[II] duration of    dots: min/avg/max =  12107/ 16164/ 16757, expected =  20000, divergence min/avg/max =  -39.465%/ -19.180%/ -16.215%
PulseAudio sound system, 60WMP
[II] duration of  dashes: min/avg/max =  57549/ 60050/ 65158, expected =  60000, divergence min/avg/max =   -4.085%/   0.083%/   8.597%
[II] duration of    dots: min/avg/max =  15114/ 19959/ 25528, expected =  20000, divergence min/avg/max =  -24.430%/  -0.205%/  27.640%
*/
static test_data_t g_test_data[] = {
	{ .sound_system = CW_AUDIO_NULL,      .speed =  4, .reference_div_dots = {    0.041,    0.068,    0.075 }, .reference_div_dashes = {    0.014,    0.023,    0.027 }},
	{ .sound_system = CW_AUDIO_NULL,      .speed = 12, .reference_div_dots = {    0.113,    0.189,    0.241 }, .reference_div_dashes = {    0.034,    0.064,    0.079 }},
	{ .sound_system = CW_AUDIO_NULL,      .speed = 24, .reference_div_dots = {    0.220,    0.316,    0.444 }, .reference_div_dashes = {    0.077,    0.131,    0.170 }},
	{ .sound_system = CW_AUDIO_NULL,      .speed = 36, .reference_div_dots = {    0.354,    0.492,    0.702 }, .reference_div_dashes = {    0.150,    0.218,    0.261 }},
	{ .sound_system = CW_AUDIO_NULL,      .speed = 60, .reference_div_dots = {    0.630,    0.890,    1.145 }, .reference_div_dashes = {    0.238,    0.343,    0.427 }},

	{ .sound_system = CW_AUDIO_ALSA,      .speed =  4, .reference_div_dots = {   -2.304,   -1.263,   -0.172 }, .reference_div_dashes = {   -0.838,   -0.475,   -0.191 }},
	{ .sound_system = CW_AUDIO_ALSA,      .speed = 12, .reference_div_dots = {   -6.692,   -3.542,   -1.060 }, .reference_div_dashes = {   -2.413,   -1.410,   -0.301 }},
	{ .sound_system = CW_AUDIO_ALSA,      .speed = 24, .reference_div_dots = {  -10.538,   -5.278,   -1.056 }, .reference_div_dashes = {   -5.349,   -1.480,    3.331 }},
	{ .sound_system = CW_AUDIO_ALSA,      .speed = 36, .reference_div_dots = {  -14.388,   -1.671,   23.991 }, .reference_div_dashes = {   -6.334,   -1.211,    6.537 }},
	{ .sound_system = CW_AUDIO_ALSA,      .speed = 60, .reference_div_dots = {  -39.465,  -19.180,  -16.215 }, .reference_div_dashes = {  -12.443,   -6.295,   -3.437 }},

	{ .sound_system = CW_AUDIO_PA,        .speed =  4, .reference_div_dots = {   -1.830,   -0.156,    1.110 }, .reference_div_dashes = {   -0.288,    0.022,    0.754 }},
	{ .sound_system = CW_AUDIO_PA,        .speed = 12, .reference_div_dots = {   -2.726,   -0.063,    6.259 }, .reference_div_dashes = {   -1.479,   -0.041,    1.003 }},
	{ .sound_system = CW_AUDIO_PA,        .speed = 24, .reference_div_dots = {   -9.252,    0.004,    6.846 }, .reference_div_dashes = {   -4.668,   -0.306,    1.743 }},
	{ .sound_system = CW_AUDIO_PA,        .speed = 36, .reference_div_dots = {  -16.257,   -0.690,    7.242 }, .reference_div_dashes = {   -2.522,    0.153,    5.280 }},
	{ .sound_system = CW_AUDIO_PA,        .speed = 60, .reference_div_dots = {  -24.430,   -0.205,   27.640 }, .reference_div_dashes = {   -4.085,    0.083,    8.597 }},
};




static cw_element_t g_test_input_elements[INPUT_ELEMENTS_COUNT] = {
	/* "o" 1 */
	{ CW_DASH_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION,  0 },
	{ CW_DASH_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION,  0 },
	{ CW_DASH_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION,  0 }, /* TODO: this should be EOE + EOC, right? */

	/* "o" 2 */
	{ CW_DASH_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION,  0 },
	{ CW_DASH_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION,  0 },
	{ CW_DASH_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION,  0 },

	/* "o" 3 */
	{ CW_DASH_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION,  0 },
	{ CW_DASH_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION,  0 },
	{ CW_DASH_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION,  0 },

	/* "o" 4 */
	{ CW_DASH_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION,  0 },
	{ CW_DASH_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION,  0 },
	{ CW_DASH_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION,  0 },

	/* "o" 5 */
	{ CW_DASH_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION,  0 },
	{ CW_DASH_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION,  0 },
	{ CW_DASH_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION,  0 },

	/* "o" 6 */
	{ CW_DASH_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION,  0 },
	{ CW_DASH_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION,  0 },
	{ CW_DASH_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION,  0 },

	/* "o" 7 */
	{ CW_DASH_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION,  0 },
	{ CW_DASH_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION,  0 },
	{ CW_DASH_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION,  0 },

	/* "o" 8 */
	{ CW_DASH_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION,  0 },
	{ CW_DASH_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION,  0 },
	{ CW_DASH_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION,  0 },

	/* "o" 9 */
	{ CW_DASH_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION,  0 },
	{ CW_DASH_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION,  0 },
	{ CW_DASH_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION,  0 },



	/* "s" 1 */
	{ CW_DOT_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION, 0 },
	{ CW_DOT_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION, 0 },
	{ CW_DOT_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION, 0 },

	/* "s" 2 */
	{ CW_DOT_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION, 0 },
	{ CW_DOT_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION, 0 },
	{ CW_DOT_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION, 0 },

	/* "s" 3 */
	{ CW_DOT_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION, 0 },
	{ CW_DOT_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION, 0 },
	{ CW_DOT_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION, 0 },

	/* "s" 4 */
	{ CW_DOT_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION, 0 },
	{ CW_DOT_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION, 0 },
	{ CW_DOT_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION, 0 },

	/* "s" 5 */
	{ CW_DOT_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION, 0 },
	{ CW_DOT_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION, 0 },
	{ CW_DOT_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION, 0 },

	/* "s" 6 */
	{ CW_DOT_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION, 0 },
	{ CW_DOT_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION, 0 },
	{ CW_DOT_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION, 0 },

	/* "s" 7 */
	{ CW_DOT_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION, 0 },
	{ CW_DOT_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION, 0 },
	{ CW_DOT_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION, 0 },

	/* "s" 8 */
	{ CW_DOT_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION, 0 },
	{ CW_DOT_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION, 0 },
	{ CW_DOT_REPRESENTATION, 0 },
	{ CW_EOE_REPRESENTATION, 0 },

	/* "s" 9 */
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

	if (callback_data->counter > 0) { /* Don't do anything for zero-th element, for which there is no 'prev timestamp'. */
		const int diff = cw_timestamp_compare_internal(&callback_data->prev_timestamp, &now_timestamp);
#if 1
		fprintf(stderr, "[II] Call %3d, state %d, representation = '%c', duration of previous element = %6d us\n",
			callback_data->counter, state, g_test_input_elements[callback_data->counter].representation, diff);
#endif

		/* Notice that we do -1 here. We are at the beginning
		   of new element, and currently calculated diff is
		   how long *previous* element was. */
		g_test_input_elements[callback_data->counter - 1].duration = diff;

		if (state) {
			if (CW_DOT_REPRESENTATION != g_test_input_elements[callback_data->counter].representation
			    && CW_DASH_REPRESENTATION != g_test_input_elements[callback_data->counter].representation) {
				fprintf(stderr, "[EE] Unexpected representation '%c' at %d for state 'closed'\n",
					g_test_input_elements[callback_data->counter].representation,
					callback_data->counter);
			}
		} else {
			if (CW_EOE_REPRESENTATION != g_test_input_elements[callback_data->counter].representation) {
				fprintf(stderr, "[EE] Unexpected representation '%c' at %d for state 'open'\n",
					g_test_input_elements[callback_data->counter].representation,
					callback_data->counter);
			}
		}

	}
	callback_data->counter++;
	callback_data->prev_timestamp = now_timestamp;
}




static void update_element_stats(cw_element_stats_t * stats, int element_duration)
{
	stats->duration_total += element_duration;
	stats->count++;
	stats->duration_avg = stats->duration_total / stats->count;

	if (element_duration > stats->duration_max) {
		stats->duration_max = element_duration;
	}
	if (element_duration < stats->duration_min) {
		stats->duration_min = element_duration;
	}
}




static void calculate_divergences_from_stats(const cw_element_stats_t * stats, divergence_t * divergences, int duration_expected)
{
	divergences->min = 100 * (stats->duration_min - duration_expected) / (1.0 * duration_expected);
	divergences->avg = 100 * (stats->duration_avg - duration_expected) / (1.0 * duration_expected);
	divergences->max = 100 * (stats->duration_max - duration_expected) / (1.0 * duration_expected);
}




static void print_element_stats_and_divergences(const cw_element_stats_t * stats, const divergence_t * divergences, const char * name, int duration_expected)
{
	fprintf(stderr, "[II] duration of %7s: min/avg/max = %6d/%6d/%6d, expected = %6d, divergence min/avg/max = %8.3f%%/%8.3f%%/%8.3f%%\n",
		name,
		stats->duration_min,
		stats->duration_avg,
		stats->duration_max,
		duration_expected,
		divergences->min,
		divergences->avg,
		divergences->max);
}




/* Top-level test function. */
cwt_retv test_cw_gen_state_callback(cw_test_executor_t * cte)
{
	const int max = cte->get_repetitions_count(cte);

	cte->print_test_header(cte, "%s (%d)", __func__, max);

	cwt_retv retv = cwt_retv_ok;
	const size_t n_tests = sizeof (g_test_data) / sizeof (g_test_data[0]);
	for (size_t i = 0; i < n_tests; i++) {
		test_data_t * test_data = &g_test_data[i];
		if (CW_AUDIO_NONE == test_data->sound_system) {
			continue;
		}
		if (cte->current_sound_system != test_data->sound_system) {
			continue;
		}
		const char * sound_device = cte->current_sound_device;
		if (cwt_retv_ok != test_cw_gen_state_callback_sub(cte, test_data, sound_device)) {
			retv = cwt_retv_err;
			break;
		}
	}

	cte->print_test_footer(cte, __func__);

	return retv;
}




static cwt_retv test_cw_gen_state_callback_sub(cw_test_executor_t * cte, test_data_t * test_data, const char * sound_device)
{
	cw_gen_t * gen = cw_gen_new(test_data->sound_system,
				    sound_device);
	cw_gen_set_speed(gen, test_data->speed);
	cw_gen_set_frequency(gen, cte->config->frequency);


	callback_data_t callback_data = { 0 };
	cw_gen_register_value_tracking_callback_internal(gen, gen_callback_fn, &callback_data);


	int dot_usecs, dash_usecs,
		end_of_element_usecs,
		ics_usecs, end_of_word_usecs,
		additional_usecs, adjustment_usecs;
	cw_gen_get_timing_parameters_internal(gen,
					      &dot_usecs, &dash_usecs,
					      &end_of_element_usecs,
					      &ics_usecs, &end_of_word_usecs,
					      &additional_usecs, &adjustment_usecs);
	fprintf(stderr,
		"[II] dot duration  = %6d us\n"
		"[II] dash duration = %6d us\n"
		"[II] eoe duration  = %6d us\n"
		"[II] ics duration  = %6d us\n"
		"[II] iws duration  = %6d us\n"
		"[II] additional duration = %6d us\n"
		"[II] adjustment duration = %6d us\n",
		dot_usecs, dash_usecs,
		end_of_element_usecs,
		ics_usecs, end_of_word_usecs,
		additional_usecs, adjustment_usecs);
	fprintf(stderr, "[II] speed = %d WPM\n", test_data->speed);


	cw_gen_start(gen);
	cw_gen_enqueue_string(gen, input_string);
	cw_gen_wait_for_queue_level(gen, 0);


	cw_gen_stop(gen);
	cw_gen_delete(&gen);


	calculate_test_results(g_test_input_elements, test_data, dot_usecs, dash_usecs);
	evaluate_test_results(cte, test_data);
	clear_data(g_test_input_elements);

	return 0;
}




/**
   Calculate current divergences (from current run of test) that will be
   compared with reference values
*/
static void calculate_test_results(const cw_element_t * test_input_elements, test_data_t * test_data, int dot_usecs, int dash_usecs)
{
	cw_element_stats_t stats_dot  = { .duration_min = 1000000000, .duration_avg = 0, .duration_max = 0, .duration_total = 0, .count = 0 };
	cw_element_stats_t stats_dash = { .duration_min = 1000000000, .duration_avg = 0, .duration_max = 0, .duration_total = 0, .count = 0 };

	/* Skip first two elements and a last element. The way
	   the test is structured may impact correctness of
	   values of these elements. TODO: make the elements
	   correct. */
	for (int i = 2; i < INPUT_ELEMENTS_COUNT - 1; i++) {
		switch (test_input_elements[i].representation) {
		case CW_DOT_REPRESENTATION:
			update_element_stats(&stats_dot, test_input_elements[i].duration);
			break;
		case CW_DASH_REPRESENTATION:
			update_element_stats(&stats_dash, test_input_elements[i].duration);
			break;
		case CW_EOE_REPRESENTATION:
			/* TODO: implement. */
			break;
		default:
			break;
		}
	}

	calculate_divergences_from_stats(&stats_dot, &test_data->current_div_dots, dot_usecs);
	calculate_divergences_from_stats(&stats_dash, &test_data->current_div_dashes, dash_usecs);

	print_element_stats_and_divergences(&stats_dot, &test_data->current_div_dots, "dots", dot_usecs);
	print_element_stats_and_divergences(&stats_dash, &test_data->current_div_dashes, "dashes", dash_usecs);

	return;
}




/**
   Compare test results from current test run with reference data. Update
   test results in @p cte.
*/
static void evaluate_test_results(cw_test_executor_t * cte, test_data_t * test_data)
{
	/* Margin above 1.0: allow current results to be slightly worse than reference.
	   Margin below 1.0: accept current results only if they are better than reference. */
	const double margin = 1.5;
	{
		const double expected_div = fabs(test_data->reference_div_dots.min) * margin;
		const double current_div = fabs(test_data->current_div_dots.min);
		cte->expect_op_double(cte, expected_div, ">", current_div, "divergence of dots, min");
	}
	{
		const double expected_div = fabs(test_data->reference_div_dots.avg) * margin;
		const double current_div = fabs(test_data->current_div_dots.avg);
		cte->expect_op_double(cte, expected_div, ">", current_div, "divergence of dots, avg");
	}
	{
		const double expected_div = fabs(test_data->reference_div_dots.max) * margin;
		const double current_div = fabs(test_data->current_div_dots.max);
		cte->expect_op_double(cte, expected_div, ">", current_div, "divergence of dots, max");
	}

	{
		const double expected_div = fabs(test_data->reference_div_dashes.min) * margin;
		const double current_div = fabs(test_data->current_div_dashes.min);
		cte->expect_op_double(cte, expected_div, ">", current_div, "divergence of dashes, min");
	}
	{
		const double expected_div = fabs(test_data->reference_div_dashes.avg) * margin;
		const double current_div = fabs(test_data->current_div_dashes.avg);
		cte->expect_op_double(cte, expected_div, ">", current_div, "divergence of dashes, avg");
	}
	{
		const double expected_div = fabs(test_data->reference_div_dashes.max) * margin;
		const double current_div = fabs(test_data->current_div_dashes.max);
		cte->expect_op_double(cte, expected_div, ">", current_div, "divergence of dashes, max");
	}

	/* TODO: the test should also have test for absolute
	   value of divergence, not only for comparison with
	   post_3.5.1 branch. The production code should aim
	   at low absolute divergence, e.g. no higher than 3%. */

	return;
}




/**
   Clear data accumulated in current test run. The function should be used as
   a preparation for next test run.
*/
static void clear_data(cw_element_t * test_input_elements)
{
	/* Clear durations calculated in current test before next call of
	   this test function. */
	for (int i = 0; i < INPUT_ELEMENTS_COUNT; i++) {
		test_input_elements[i].duration = 0;
	}

	return;
}
