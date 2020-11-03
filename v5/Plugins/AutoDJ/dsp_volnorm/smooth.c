#include "volnorm.h"

struct smooth_struct {
	double * data;
	double max;
	int size;
	int used;
	int current;
};

smooth_t * SmoothNew(int size)
{
	smooth_t * sm = (smooth_t *)malloc(sizeof(smooth_t));
	if (sm == NULL)
		return NULL;

	sm->data = (double *)malloc(size * sizeof(double));
	if (sm->data == NULL) {
		free(sm);
		return NULL;
	}

	sm->size = size;
	sm->current = sm->used = 0;
	sm->max = 0.0;
	return sm;
}

void SmoothDelete(smooth_t * del)
{
	if (del == NULL)
		return;

	if (del->data != NULL)
		free(del->data);
	
	free(del);
}

void SmoothAddSample(smooth_t * sm, double sample)
{
	if (sm == NULL)
		return;

	/* Put the sample in the buffer */
	sm->data[sm->current] = sample;
	
	/* Adjust the sample stats */
	++sm->current;
	
	if (sm->current > sm->used)
		++sm->used;

	if (sm->current >= sm->size)
		sm->current %= sm->size;

}

double SmoothGetMax(smooth_t * sm)
{
	if (sm == NULL)
		return -1.0;

	/* Calculate the smoothed value */
	{
		int i = 0;
		double smoothed = 0.0;

		for (i = 0; i < sm->used; ++i)
			smoothed += sm->data[i];
		smoothed = smoothed / sm->used;
	
		/* If we haven't filled the smoothing buffer, 
		 * dont save the max value. 
		 */
		if (sm->used < sm->size)
			/* Average (weighted appropriately) the smoothed with normalize level for the unknown */
			return (smoothed*sm->used + normalize_level*(sm->size - sm->used)) / sm->size;

		if (sm->max < smoothed)
			sm->max = smoothed;
	}

	return sm->max;
}
