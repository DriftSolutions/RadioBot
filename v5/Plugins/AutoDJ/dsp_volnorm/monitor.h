/* monitor.h */

/* This file includes the function definitions for functions that are used
 * in the monitoring.
 */

#ifndef MONITOR_H
#define MONITOR_H

struct _Monitor;
typedef struct _Monitor Monitor;

/* Create the monitor */
Monitor * monitor_create(void);

/* Add data points */
void monitor_add_data(Monitor * monitor, double power, double gain);

/* File change detected */
void monitor_file_change(void);

#endif
