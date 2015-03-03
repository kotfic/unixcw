/*
  This file is a part of unixcw project.
  unixcw project is covered by GNU General Public License.
*/

#ifndef H_LIBCW_GEN
#define H_LIBCW_GEN





#include "libcw.h"
#include "libcw_pa.h"
#include "libcw_alsa.h"
#include "libcw_tq.h"
#include "libcw_key.h"
#include "libcw_rec.h"





/* Allowed values of cw_tone_t.slope_mode.  This is to decide whether
   a tone has slopes at all. If there are any slopes in a tone, there
   can be only rising slope (without falling slope), falling slope
   (without rising slope), or both slopes (i.e. standard slopes).
   These values don't tell anything about shape of slopes (unless you
   consider 'no slopes' a shape ;) ). */
#define CW_SLOPE_MODE_STANDARD_SLOPES   20
#define CW_SLOPE_MODE_NO_SLOPES         21
#define CW_SLOPE_MODE_RISING_SLOPE      22
#define CW_SLOPE_MODE_FALLING_SLOPE     23





/* Symbolic name for inter-mark space. */
enum { CW_SYMBOL_SPACE = ' ' };





/* This is used in libcw_gen and libcw_debug. */
#ifdef LIBCW_WITH_DEV
#define CW_DEV_RAW_SINK           1  /* Create and use /tmp/cw_file.<audio system>.raw file with audio samples written as raw data. */
#define CW_DEV_RAW_SINK_MARKERS   0  /* Put markers in raw data saved to raw sink. */
#else
#define CW_DEV_RAW_SINK           0
#define CW_DEV_RAW_SINK_MARKERS   0
#endif





/* Forward declarations of data types. */
struct cw_key_struct;




/* Generic constants - common for all audio systems (or not used in some of systems) */

static const long int   CW_AUDIO_VOLUME_RANGE = (1 << 15);    /* 2^15 = 32768 */
static const int        CW_AUDIO_SLOPE_USECS = 5000;          /* length of a single slope in standard tone */


/* smallest duration of time (in microseconds) that is used by libcw for
   idle waiting and idle loops; if a libcw function needs to wait for
   something, or make an idle loop, it should call usleep(N * CW_AUDIO_USECS_QUANTUM) */
#define CW_AUDIO_QUANTUM_USECS 100





struct cw_gen_struct {

	int  (* open_device)(cw_gen_t *gen);
	void (* close_device)(cw_gen_t *gen);
	int  (* write)(cw_gen_t *gen);

	/* Generator can only generate tones that were first put into
	   queue, and then dequeued. Here is a tone queue associated
	   with a generator. One tone queue per generator. One
	   generator per tone queue.

	   The tone queue should be created in generator's
	   constructor, and deleted in generator's destructor using
	   tone queue's own constructor and destructor functions - see
	   cw_tq module for declarations of these functions. */
	cw_tone_queue_t *tq;



	/* buffer storing sine wave that is calculated in "calculate sine
	   wave" cycles and sent to audio system (OSS, ALSA, PulseAudio);

	   the buffer should be always filled with valid data before sending
	   it to audio system (to avoid hearing garbage).

	   we should also send exactly buffer_n_samples samples to audio
	   system, in order to avoid situation when audio system waits for
	   filling its buffer too long - this would result in errors and
	   probably audible clicks; */
	cw_sample_t *buffer;

	/* size of data buffer, in samples;

	   the size may be restricted (min,max) by current audio system
	   (OSS, ALSA, PulseAudio); the audio system may also accept only
	   specific values of the size;

	   audio libraries may provide functions that can be used to query
	   for allowed audio buffer sizes;

	   the smaller the buffer, the more often you have to call function
	   writing data to audio system, which increases CPU usage;

	   the larger the buffer, the less responsive an application may
	   be to changes of audio data parameters (depending on application
	   type); */
	int buffer_n_samples;


	/* We need two indices to gen->buffer, indicating beginning
	   and end of a subarea in the buffer.  The subarea is not
	   exactly the same as gen->buffer for variety of reasons:
	   - buffer length is almost always smaller than length of a
	   Mark or Space (in samples) that we want to produce;
	   - moreover, length of a Mark/Space (in samples) is almost
	   never an exact multiple of length of a buffer;
	   - as a result, a sound representing a Mark/Space may start
	   and end anywhere between beginning and end of the buffer;

	   A workable solution is to have a subarea of the buffer, a
	   window, into which we will write a series of fragments of
	   calculated sound.

	   "start" and "stop" mark beginning and end of the subarea
	   (inclusive: samples indexed by "start" and "stop" are part
	   of the subarea).

	   The subarea shall not wrap around boundaries of the buffer:
	   "stop" shall be no larger than "gen->buffer_n_samples - 1",
	   and it shall never be smaller than "start". "start" will be
	   somewhere between zero and "stop", inclusive.

	   "start" == "stop" is a valid situation - length of subarea
	   is then one.

	   Very often (in the middle of a long tone), "start" will be
	   zero, and "stop" will be "gen->buffer_n_samples - 1".

	   Sine wave (sometimes with amplitude = 0) will be calculated
	   for subarea's cells (samples) ranging from cell "start"
	   to cell "stop", inclusive. */
	int buffer_sub_start;
	int buffer_sub_stop;


	/* Some parameters of tones (and of tones' slopes) are common
	   for all tones generated in given time by a generator.
	   Therefore the generator should contain this struct.

	   Other parameters, such as tone's duration or frequency, are
	   strictly related to tones - you won't find them here. */
	struct {
		/* Depending on sample rate, sending speed, and user
		   preferences, length of slope of tones generated by
		   generator may vary, but once set, it is constant
		   for all generated tones (until next change of
		   sample rate, sending speed, etc.).

		   This is why we have the slope length in generator.

		   n_amplitudes declared a bit below in this struct is
		   a secondary parameter, derived from
		   length_usecs. */
		int length_usecs;

		/* Linear/raised cosine/sine/rectangle. */
		int shape;

		/* Table of amplitudes of every PCM sample of tone's
		   slope.

		   The values in amplitudes[] change from zero to max
		   (at least for any sane slope shape), so naturally
		   they can be used in forming rising slope. However
		   they can be used in forming falling slope as well -
		   just iterate the table from end to beginning. */
		float *amplitudes;

		/* This is a secondary parameter, derived from
		   length_usecs. n_amplitudes is useful when iterating
		   over amplitudes[] or reallocing the
		   amplitudes[]. */
		int n_amplitudes;
	} tone_slope;


	/* none/null/console/OSS/ALSA/PulseAudio */
	int audio_system;

	bool audio_device_is_open;

	/* Path to console file, or path to OSS soundcard file,
	   or ALSA sound device name, or PulseAudio device name
	   (it may be unused for PulseAudio) */
	char *audio_device;

	/* output file descriptor for audio data (console, OSS) */
	int audio_sink;

#ifdef LIBCW_WITH_ALSA
	/* Data used by ALSA. */
	cw_alsa_data_t alsa_data;
#endif

#ifdef LIBCW_WITH_PULSEAUDIO
	/* Data used by PulseAudio. */
	cw_pa_data_t pa_data;
#endif

	struct {
		int x;
		int y;
		int z;
	} oss_version;

	/* output file descriptor for debug data (console, OSS, ALSA, PulseAudio) */
	int dev_raw_sink;


	/* Essential sending parameters. */
	int send_speed;     /* [wpm] */
	int frequency;      /* The frequency of generated sound. [Hz] */
	int volume_percent; /* Level of sound in percents of maximum allowable level. */
	int volume_abs;     /* Level of sound in absolute terms; height of PCM samples. */
	int gap;            /* Inter-mark gap. [number of dot lengths]. */
	int weighting;      /* Dot/dash weighting. */



	/* After changing sending speed, gap or weighting, some
	   generator's internal parameters need to be
	   re-calculated. This is a flag that shows when this needs to
	   be done. */
	bool parameters_in_sync;


	int sample_rate; /* set to the same value of sample rate as
			    you have used when configuring sound card */

	/* start/stop flag.
	   Set to true before running dequeue_and_play thread
	   function.
	   Set to false to stop generator and return from
	   dequeue_and_play thread function. */
	bool do_dequeue_and_play;

	/* used to calculate sine wave;
	   phase offset needs to be stored between consecutive calls to
	   function calculating consecutive fragments of sine wave */
	double phase_offset;

	struct {
		/* generator thread function is used to generate sine wave
		   and write the wave to audio sink */
		pthread_t      id;
		pthread_attr_t attr;

		/* Call to pthread_create(&id, ...) executed and
		   succeeded? If not, don't call pthread_kill(). I
		   can't check value of thread.id, because pthread_t
		   data type is opaque.

		   This flag is a bit different than
		   cw_gen_t->do_dequeue_and_play.  Setting
		   ->do_dequeue_and_play signals intent to run a loop
		   deqeueing tones in
		   cw_gen_dequeue_and_play_internal().  Setting
		   ->thread.running means that thread function
		   cw_gen_dequeue_and_play_internal() was launched
		   successfully. */
		bool running;
	} thread;

	struct {
		/* main thread, existing from beginning to end of main process run;
		   the variable is used to send signals to main app thread; */
		pthread_t thread_id;
		char *name;
	} client;



	/* These are basic timing parameters which should be
	   recalculated each time client code demands changing some
	   higher-level parameter of generator (e.g. changing of
	   sending speed). */

	/* Watch out for "additional" key word.

	   WARNING: notice how the eoc and eow spaces are
	   calculated. They aren't full 3 units and 7 units. They are
	   2 units (which takes into account preceding eom space
	   length), and 5 units (which takes into account preceding
	   eom *and* eoc space length). So these two lengths are
	   *additional* ones, i.e. in addition to (already existing)
	   eom and/or eoc space.  Whether this is good or bad idea to
	   calculate them like this is a separate topic. Just be aware
	   of this fact.

	   Search the word "*additional*" in libcw_gen.c for
	   implementation */
	int dot_len;              /* Length of a dot. [us] */
	int dash_len;             /* Length of a dash. [us] */
	int eom_space_len;        /* Length of end-of-mark space (i.e. inter-mark space). [us] */
	int eoc_space_len;        /* Length of *additional* end-of-character space. [us] */
	int eow_space_len;        /* Length of *additional* end-of-word space. [us] */
	int additional_space_len; /* Length of additional space at the end of a character. [us] */
	int adjustment_space_len; /* Length of adjustment space at the end of a word. [us] */


	/* Key that has a generator associated with it. Can be NULL in
	   some applications (?).  Set using
	   cw_key_register_generator_internal(). */
	volatile struct cw_key_struct *key;
};




/* Basic generator functions. */
cw_gen_t *cw_gen_new_internal(int audio_system, const char *device);
void      cw_gen_delete_internal(cw_gen_t **gen);
int       cw_gen_start_internal(cw_gen_t *gen);
void      cw_gen_stop_internal(cw_gen_t *gen);





/* Setters of generator's basic parameters. */
int cw_gen_set_speed_internal(cw_gen_t *gen, int new_value);
int cw_gen_set_frequency_internal(cw_gen_t *gen, int new_value);
int cw_gen_set_volume_internal(cw_gen_t *gen, int new_value);
int cw_gen_set_gap_internal(cw_gen_t *gen, int new_value);
int cw_gen_set_weighting_internal(cw_gen_t *gen, int new_value);




/* Getters of generator's basic parameters. */
int cw_gen_get_speed_internal(cw_gen_t *gen);
int cw_gen_get_frequency_internal(cw_gen_t *gen);
int cw_gen_get_volume_internal(cw_gen_t *gen);
int cw_gen_get_gap_internal(cw_gen_t *gen);
int cw_gen_get_weighting_internal(cw_gen_t *gen);





void cw_gen_get_send_parameters_internal(cw_gen_t *gen, int *dot_len, int *dash_len, int *eom_space_len, int *eoc_space_len, int *eow_space_len, int *additional_space_len, int *adjustment_space_len);





/* Generator's 'play' primitives. */
int cw_gen_play_mark_internal(cw_gen_t *gen, char mark);
int cw_gen_play_eoc_space_internal(cw_gen_t *gen);
int cw_gen_play_eow_space_internal(cw_gen_t *gen);

/* These are also 'play' primitives, but are intended to be used on
   hardware key events. */
int cw_gen_key_begin_mark_internal(cw_gen_t *gen);
int cw_gen_key_begin_space_internal(cw_gen_t *gen);
int cw_gen_key_pure_symbol_internal(cw_gen_t *gen, char symbol);





int cw_gen_play_representation_internal(cw_gen_t *gen, const char *representation, bool partial);
int cw_gen_play_character_internal(cw_gen_t *gen, char c);
int cw_gen_play_character_parital_internal(cw_gen_t *gen, char c);
int cw_gen_play_string_internal(cw_gen_t *gen, const char *string);



int   cw_gen_set_audio_device_internal(cw_gen_t *gen, const char *device);
int   cw_gen_silence_internal(cw_gen_t *gen);
char *cw_gen_get_audio_system_label_internal(cw_gen_t *gen);

void cw_generator_delete_internal(void);

void cw_gen_reset_send_parameters_internal(cw_gen_t *gen);
void cw_gen_sync_parameters_internal(cw_gen_t *gen);





#ifdef LIBCW_UNIT_TESTS

unsigned int test_cw_gen_new_delete_internal(void);

#endif /* #ifdef LIBCW_UNIT_TESTS */





#endif /* #ifndef H_LIBCW_GEN */
