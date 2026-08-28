#define _POSIX_C_SOURCE 200809L

#include "ds4_metrics.h"

uint64_t ds4_metric_value[DS4_M_COUNT];
uint64_t ds4_histogram_value[DS4_H_COUNT][13];
uint64_t ds4_metrics_boot_ns, ds4_metrics_resident_sessions;
ds4_metrics_mode ds4_metrics_mode_value;
