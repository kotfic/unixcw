/*
  This file is a part of unixcw project.
  unixcw project is covered by GNU General Public License, version 2 or later.
*/

#ifndef H_LIBCW_REC
#define H_LIBCW_REC




#include <stdbool.h>
#include <sys/time.h> /* struct timeval */




#include "libcw.h"
#include "libcw2.h"
#include "libcw_debug.h"




/* Dot duration magic number.

   From PARIS calibration, 1 Dot duration [us] = 1200000 / speed [wpm].

   This variable is used in generator code as well. */
enum { CW_DOT_CALIBRATION = 1200000 };


/* State of receiver state machine.
   "RS" stands for "Receiver State" */
typedef enum {
	RS_IDLE,          /* Representation buffer is empty and ready to accept data. */
	RS_MARK,              /* Between begin and end of Mark (Dot or Dash). */
	RS_INTER_MARK_SPACE,  /* Space between Marks within one character. */
	RS_EOC_GAP,       /* Gap after a character, without error (EOC = end-of-character). */
	RS_EOW_GAP,       /* Gap after a word, without error (EOW = end-of-word). */
	RS_EOC_GAP_ERR,   /* Gap after a character, with error. */
	RS_EOW_GAP_ERR    /* Gap after a word, with error. */
} cw_rec_state_t;


/* Does receiver initially adapt to varying speed of input data? */
enum { CW_REC_ADAPTIVE_MODE_INITIAL = false };


/* TODO: it would be interesting to track (in debug mode) relationship
   between "speed threshold" and "noise threshold" parameters. */
enum { CW_REC_SPEED_THRESHOLD_INITIAL = (CW_DOT_CALIBRATION / CW_SPEED_INITIAL) * 2 };    /* Initial adaptive speed threshold. [us] */
enum { CW_REC_NOISE_THRESHOLD_INITIAL = (CW_DOT_CALIBRATION / CW_SPEED_MAX) / 2 };        /* Initial noise filter threshold. */


/* Receiver contains a fixed-length buffer for representation of
   received data.  Capacity of the buffer is vastly longer than any
   practical representation.  Don't know why, a legacy thing.

   Representation can be presented as a char string. This is the
   maximal length of the string. This value does not include string's
   ending NULL. */
enum { CW_REC_REPRESENTATION_CAPACITY = 256 };


/* TODO: what is the relationship between this constant and CW_REC_REPRESENTATION_CAPACITY?
   Both have value of 256. Coincidence? I don't think so. */
enum { CW_REC_DURATION_STATS_CAPACITY = 256 };


/* Length of array used to calculate average duration of a mark. Average
   duration of a mark is used in adaptive receiving mode to track speed
   of incoming Morse data. */
enum { CW_REC_AVERAGING_DURATIONS_COUNT = 4 };


/* Types of receiver's timing statistics.
   CW_REC_STAT_NONE must be zero so that the statistics buffer is initially empty. */
typedef enum {
	CW_REC_STAT_NONE = 0,
	CW_REC_STAT_DOT,                    /* Dot mark. */
	CW_REC_STAT_DASH,                   /* Dash mark. */
	CW_REC_STAT_INTER_MARK_SPACE,       /* Space between Dots and Dashes within one character. */
	CW_REC_STAT_INTER_CHARACTER_SPACE   /* Space between characters within one word. */
} stat_type_t;


typedef struct {
	stat_type_t type;    /* Record type */
	int duration_delta;  /* Difference between actual and ideal duration of mark or space. [us] */
} cw_rec_duration_stats_point_t;


/* A moving averages structure - circular buffer. Used for calculating
   averaged duration ([us]) of dots and dashes. */
typedef struct {
	int buffer[CW_REC_AVERAGING_DURATIONS_COUNT];  /* Buffered mark durations. */
	int cursor;                                    /* Circular buffer cursor. */
	int sum;                                       /* Running sum of durations of marks. [us] */
	int average;                                   /* Averaged duration of a mark. [us] */
} cw_rec_averaging_t;




struct cw_rec_struct {
	cw_rec_state_t state;


	/* Essential parameters. */
	/* Changing values of speed, tolerance, gap or
	   is_adaptive_receive_mode will trigger a recalculation of
	   low level timing parameters. */

	/* 'speed' is float instead of being 'int' on purpose.  It
	   makes adaptation to varying speed of incoming data more
	   smooth. This is especially important at low speeds, where
	   change/adaptation from (int) 5wpm to (int) 4wpm would
	   mean a sharp decrease by 20%. With 'float' data type the
	   adjustment of receive speeds is more gradual. */
	float speed;       /* [wpm] */
	int tolerance;
	int gap;         /* Inter-character-gap, similar as in generator. */
	bool is_adaptive_receive_mode;
	int noise_spike_threshold;
	/* Library variable which is automatically adjusted based on
	   incoming Morse data stream, rather than being settable by
	   the user.

	   Not exactly a *speed* threshold, but for a lack of a better
	   name...

	   When the library changes internally value of this variable,
	   it recalculates low level timing parameters too. */
	int adaptive_speed_threshold; /* [microseconds]/[us] */



	/* Retained timestamps of mark's begin and end. */
	struct timeval mark_start;
	struct timeval mark_end;

	/* Buffer for received representation (dots/dashes). This is a
	   fixed-length buffer, filled in as tone on/off timings are
	   taken. The buffer is vastly longer than any practical
	   representation.

	   Along with it we maintain a cursor indicating the current
	   write position. */
	char representation[CW_REC_REPRESENTATION_CAPACITY + 1];
	int representation_ind;



	/* Receiver's low-level timing parameters */

	/* These are basic timing parameters which should be
	   recalculated each time client code demands changing some
	   higher-level parameter of receiver.  How these values are
	   calculated depends on receiving mode (fixed/adaptive). */
	int dot_duration_ideal;        /* Duration of an ideal dot. [microseconds]/[us] */
	int dot_duration_min;          /* Minimal duration of mark that will be identified as dot. [us] */
	int dot_duration_max;          /* Maximal duration of mark that will be identified as dot. [us] */

	int dash_duration_ideal;       /* Duration of an ideal dash. [us] */
	int dash_duration_min;         /* Minimal duration of mark that will be identified as dash. [us] */
	int dash_duration_max;         /* Maximal duration of mark that will be identified as dash. [us] */

	int ims_duration_ideal;        /* Ideal inter-mark-space, for stats. [us] */
	int ims_duration_min;          /* Shortest inter-mark-space allowable. [us] */
	int ims_duration_max;          /* Longest inter-mark-space allowable. [us] */

	int ics_duration_ideal;        /* Ideal inter-character-space, for stats. [us] */
	int ics_duration_min;          /* Shortest inter-character-space allowable. [us] */
	int ics_duration_max;          /* Longest inter-character-space allowable. [us] */

	/* These two fields have the same function as in
	   cw_gen_t. They are needed in function re-synchronizing
	   parameters. */
	int additional_delay;     /* More delay at the end of a char. [us] */
	int adjustment_delay;     /* More delay at the end of a word. [us] */



	/* Are receiver's parameters in sync?
	   After changing receiver's essential parameters, its
	   low-level timing parameters need to be re-calculated. This
	   is a flag that shows when this needs to be done. */
	bool parameters_in_sync;



	/* Receiver statistics.
	   A circular buffer of entries indicating the difference
	   between the actual and the ideal duration of received mark or
	   space, tagged with the type of statistic held, and a
	   circular buffer pointer. */
	cw_rec_duration_stats_point_t duration_stats[CW_REC_DURATION_STATS_CAPACITY];
	int duration_stats_idx;



	/* Data structures for calculating averaged duration of dots and
	   dashes. The averaged durations are used for adaptive tracking
	   of receiver's speed (tracking of speed of incoming data). */
	cw_rec_averaging_t dot_averaging;
	cw_rec_averaging_t dash_averaging;

	/* Flag indicating if receive polling has received a
	   character, and may need to augment it with a word
	   space on a later poll. */
	bool is_pending_inter_word_space;

	char label[LIBCW_OBJECT_INSTANCE_LABEL_SIZE];
};




/* Other helper functions. */
void cw_rec_reset_parameters_internal(cw_rec_t * rec);
void cw_rec_sync_parameters_internal(cw_rec_t * rec);
void cw_rec_get_parameters_internal(cw_rec_t * rec,
				    int * dot_duration_ideal, int * dash_duration_ideal,
				    int * dot_duration_min,   int * dot_duration_max,
				    int * dash_duration_min,  int * dash_duration_max,
				    int * ims_duration_min,
				    int * ims_duration_max,
				    int * ims_duration_ideal,
				    int * ics_duration_min,
				    int * ics_duration_max,
				    int * ics_duration_ideal,
				    int * adaptive_threshold);
void cw_rec_get_statistics_internal(const cw_rec_t * rec, float * dot_sd, float * dash_sd,
				    float * inter_mark_space_sd, float * inter_character_space_sd);
int cw_rec_get_buffer_length_internal(const cw_rec_t * rec);
int cw_rec_get_receive_buffer_capacity_internal(void);




void CW_REC_SET_STATE(cw_rec_t * rec, cw_rec_state_t new_state, cw_debug_t * debug_object);




#endif /* #ifndef H_LIBCW_REC */
