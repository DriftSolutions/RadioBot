/**********************************************************************

  resample.h

  Real-time library interface by Dominic Mazzoni

  Based on resample-1.7:
    http://www-ccrma.stanford.edu/~jos/resample/

  License: LGPL - see the file LICENSE.txt for more information

**********************************************************************/

#ifndef LIBRESAMPLE_INCLUDED
#define LIBRESAMPLE_INCLUDED

#if defined(WIN32) && defined(LIBRESAMPLE_DLL)
	#if defined(LIBRESAMPLE_EXPORTS)
		#define LRS_API __declspec(dllexport)
	#else
		#define LRS_API __declspec(dllimport)
	#endif
#else
	#define LRS_API
#endif

#ifdef __cplusplus
extern "C" {
#endif	/* __cplusplus */

LRS_API void * resample_open(int      highQuality,
                     double   minFactor,
                     double   maxFactor);

LRS_API void * resample_dup(const void *handle);

LRS_API int resample_get_filter_width(const void *handle);

LRS_API int resample_process(void   *handle,
                     double  factor,
                     float  *inBuffer,
                     int     inBufferLen,
                     int     lastFlag,
                     int    *inBufferUsed,
                     float  *outBuffer,
                     int     outBufferLen);

LRS_API void resample_close(void *handle);

#ifdef __cplusplus
}		/* extern "C" */
#endif	/* __cplusplus */

#endif /* LIBRESAMPLE_INCLUDED */
