#include "../adj_plugin.h"
#include <math.h>

int modify_samples(int samples, short * buffer, int channels, int samplerate) {

	short max=0, min=0;
	int64 sum = 0;
	int i;
	for (i=0; i < samples*channels; i++) {
		if (buffer[i] > max) { max = buffer[i]; }
		if (buffer[i] < min) { min = buffer[i]; }
		sum += abs(buffer[i]);
	}
	sum /= samples;
	printf(" Pre-Samples: %d, Min: %d, Max: %d, Avg: "I64FMT"\n", samples, min, max, sum);
	if (max < 32767 && min > -32767) {
		double diff = sum;//abs(max) > abs(min) ? abs(max):abs(min);//max - min;
		printf("Dynamic range: %f\n", diff);
		diff = diff*diff;
		//diff /= 32767;//65536;
		#define MAX_NORMAL 32767
		#define MAX_DOUBLE (MAX_NORMAL*MAX_NORMAL)
//#define MIN_NORMAL -MAX_NORMAL
		for (i=0; i < samples*channels; i++) {
			double tmp = buffer[i];
			tmp *= tmp;//pow(tmp,2);
			tmp /= diff;
			tmp *= MAX_DOUBLE;//32760;
			tmp = sqrt(tmp);
			tmp = (tmp > MAX_NORMAL) ? MAX_NORMAL:tmp;
			tmp = (tmp < 0) ? 0:tmp;
			buffer[i] = tmp * (buffer[i] < 0 ? -1:1);
		}

		max=0; min=0;
		for (i=0; i < samples*channels; i++) {
			if (buffer[i] > max) { max = buffer[i]; }
			if (buffer[i] < min) { min = buffer[i]; }
		}
		printf("Post-Samples: %d, Min: %d, Max: %d\n", samples, min, max);

	} else {
		printf("No volume adjustment\n");
	}

	return samples;
}

AUTODJ_DSP dsp = {
	DSP_VERSION,
	"DSP Normalization / Test Plugin",
	NULL,

	NULL,
	NULL,

	NULL,
	modify_samples,
	NULL

};

PLUGIN_EXPORT AUTODJ_DSP * GetAD_DSP() { return &dsp; }
