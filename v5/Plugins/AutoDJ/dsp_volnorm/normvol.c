#include "volnorm.h"

static bool normvol_init(int nch, int srate);
static void normvol_quit();
static int normvol_mod_samples(int samples, short * d, int nch, int srate);

#define MAX_SRATE 50000
#define MAX_CHANNELS 2
#define BYTES_PS 2
#define BUFFER_SAMPLES (MAX_SRATE * MAX_DELAY / 1000)
#define BUFFER_SHORTS (BUFFER_SAMPLES * MAX_CHANNELS)
#define BUFFER_BYTES (BUFFER_SHORTS * BYTES_PS)

#define NO_GAIN 0.01
#define SMOOTH_SAMPLES 100

/* Keep the log on the values, and smooth them */
static smooth_t * smooth[MAX_CHANNELS];

static void calc_power_level(short * d, int length, int nch);
static void adjust_gain(short * d, int length, int nch, double gain);

AUTODJ_DSP normvol_ep =
{
	DSP_VERSION,
	"Normalize Volume",
	NULL,

	normvol_init,
	normvol_mod_samples,
	NULL
};

#if defined(WIN32)
__declspec(dllexport) AUTODJ_DSP * GetAD_DSP() {
#else
AUTODJ_DSP * GetAD_DSP() {
#endif
	return &normvol_ep;
}

static void restart_smoothing(void) {
	int channel = 0;

	for (channel = 0; channel < MAX_CHANNELS; channel++) {
		if (smooth[channel] != NULL)
			SmoothDelete(smooth[channel]);

		smooth[channel] = SmoothNew(SMOOTH_SAMPLES);
	}
}

static void normvol_quit() {
	int channel = 0;

	for (channel = 0; channel < MAX_CHANNELS; channel++)
		SmoothDelete(smooth[channel]);
}

static bool normvol_init(int nch, int srate) {
	read_config();

	/* Initialize the smoothing filter, set it to SMOOTH_SAMPLES samples */
	{
		int channel = 0;
		for (channel = 0; channel < MAX_CHANNELS; channel++) {
			smooth[channel] = NULL;
		}
		restart_smoothing();
	}

	return TRUE;
}

static int normvol_mod_samples(int length, short * d, int nch, int srate)
{
	double level = -1.0;
	//int to_avoid_warning = srate;
	//srate = to_avoid_warning;

	/* Check only the last one, if it is allocated, most probably the others are
	 * too.
	 */
	if (smooth[MAX_CHANNELS-1] == NULL)
		return length;

	/* If there are too many channels do nothing. */
	if (nch > MAX_CHANNELS)
		return length;

	/* Calculate power level for this sample */
	calc_power_level(d, length, nch);	

	/* Get the maximum level over all channels */
	{
		int channel = 0;
		level = -1.0;

		for (channel = 0; channel < nch; channel++) {
			double channel_level = SmoothGetMax(smooth[channel]);
			if (channel_level > level)
				level = channel_level;
		}

	}

#ifdef PRINT_MONITOR
	printf("Target: %f, Level: %f", normalize_level, level);
#endif
	
	/* Only if the volume is higher than the silence level do something. */
	if (level > silence_level) {
		/* Calculate the gain for the level */
		double gain = normalize_level / level;

#ifdef PRINT_MONITOR		
		printf(", Gain: %f", gain);
#endif
		
		/* Make sure the gain is not above the maximum multiplier */
		if (gain > max_mult)
			gain = max_mult;

		/* Adjust the gain with the smoothed value */
		adjust_gain(d, length, nch, gain);
		
		/* printf("Max level is %f, Gain is %f\n", level, gain); */
	}

#ifdef PRINT_MONITOR
	printf("\n");
#endif
	
	return length;
}

static void calc_power_level(short * data, int length, int nch)
{
	int channel = 0;
	int i = 0;
	double sum[MAX_CHANNELS];

	//short * data = (short *) *d;

#ifdef DEBUG
	static int counter = 0;
#endif

	/* Zero the channel sum values */
	for (channel = 0; channel < nch; channel++) {
		sum[channel] = 0.0;
	}

	/* Calculate the square sums for all channels at once 
	 * This will be do better memory access
	 */

#ifdef DEBUG
	if ((double)*data > cutoff || counter == 250)  {
	printf("do_compress = %d, cutoff = %g, degree = %g, sample = %d\n",
			do_compress, cutoff, degree, *data);
		counter = 0;
	}
	++counter;
#endif
	
	for (i = 0, channel = 0; i < length*nch; ++i, ++data) {
		double sample = *data;
		double temp = 0.0;

		if (do_compress)
			if (sample > cutoff)
				sample = cutoff + (sample-cutoff)/degree;
	
		/* Calculate sample^2 and 
		   Adjust the level to be between 0.0 -- 1.0 */
		temp = sample*sample;
			
		sum[channel] += temp;
		
		/* Switch to the next channel */
		++channel;
		channel = channel % nch;
	}
	
	/* Add the power level to the smoothing queue */
	{
		static const double NORMAL = 1.0/( (double)0x7FFF );
		#define NORMAL_SQUARED (NORMAL*NORMAL);
		double channel_length = 2.0/length;
		
		for (channel = 0; channel < nch; channel++) {
			double level = sum[channel] * channel_length * NORMAL_SQUARED;
			
			/* printf("Internal level [%d]: %f\n", channel, level);*/

			SmoothAddSample(smooth[channel], sqrt(level));
		}
	}
}

short CLAMP(double v, short min, short max) {
	if (v < min) { return min; }
	if (v > max) { return max; }
	return v;
}

static void adjust_gain(short * data, int length, int nch, double gain)
{
	//short * data = (gint16 *) *d;
	int i = 0;

	/* Change nothing if it won't be noticed */
	if (gain >= 1.0 - NO_GAIN && gain <= 1.0 + NO_GAIN)
		return;
		
	for (i = 0; i < length*nch; ++i, ++data) {
		/* Convert sample to double */
		double sample = (double)*data;
		
		if (do_compress)
			if (sample > cutoff)
				sample = cutoff + (sample-cutoff)/degree;
	

		/* Multiply by gain */
		sample *= gain;

		/* Make sure it's within bounds and cast to gint16 */
		*data = (short) CLAMP(sample, 0x8000, 0x7FFF);
	}
}
