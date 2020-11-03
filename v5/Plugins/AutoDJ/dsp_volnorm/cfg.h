/* config.h */

#ifndef CFG_H
#define CFG_H

//#include <glib.h>
#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
#endif

/**
 * Read the config data into the global variables.
 */
extern void read_config(void);

/**
 * Write the config data from the global variables.
 */
extern void write_config(void);

/**
 * Print the config variables to the standard output.
 */
extern void print_config(char const *msg);

/** The level to normalize for */
extern double normalize_level;

/** The silence level, sound below this level is not adjusted. */
extern double silence_level;

/** Maximum multiplier to use. */
extern double max_mult;

extern int do_compress;
extern double cutoff;
extern double degree;

#endif
