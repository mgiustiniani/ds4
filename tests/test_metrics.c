#include "ds4_metrics.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define THREADS 4
#define ITERATIONS 10000

static void require(int condition, const char *message) {
    if (condition) return;
    fprintf(stderr, "metrics test failed: %s\n", message);
    exit(1);
}

static void *writer(void *unused) {
    (void)unused;
    for (int i = 0; i < ITERATIONS; i++) {
        ds4_metric_add(DS4_M_REQUESTS_STARTED, 1);
        ds4_metric_add(DS4_M_REQUESTS_INFLIGHT, 1);
        ds4_metric_add(DS4_M_PREFILL_CACHED, 1);

        uint64_t started = ds4_metrics_phase_begin(
            DS4_M_WAITING_PREFILL, DS4_METRICS_PHASE_PREFILL);
        ds4_metrics_phase_end(DS4_M_WAITING_PREFILL, DS4_M_WAIT_PREFILL,
                              DS4_METRICS_PHASE_PREFILL, started);

        started = ds4_metrics_phase_begin(
            DS4_M_ACTIVE_PREFILL, DS4_METRICS_PHASE_PREFILL);
        ds4_metrics_phase_end(DS4_M_ACTIVE_PREFILL, DS4_M_COMPUTE_PREFILL,
                              DS4_METRICS_PHASE_PREFILL, started);
        ds4_metric_add(DS4_M_PREFILL_COMPUTED, 1);

        started = ds4_metrics_phase_begin(
            DS4_M_ACTIVE_PREFILL, DS4_METRICS_PHASE_DECODE);
        ds4_metrics_phase_end(DS4_M_ACTIVE_PREFILL, DS4_M_COMPUTE_PREFILL,
                              DS4_METRICS_PHASE_DECODE, started);
        ds4_metric_add(DS4_M_DECODE_STEPS, 1);
        ds4_metric_add(DS4_M_DECODE_ROWS, 3);
        ds4_metric_add(DS4_M_DECODE_TOKENS, 2);

        ds4_metrics_histogram_observe(DS4_H_TTFT, 50000000ull);
        ds4_metrics_histogram_observe(DS4_H_PREFILL, 200000000ull);
        ds4_metrics_histogram_observe(DS4_H_DECODE, 2000000000ull);
        ds4_metric_add(DS4_M_REQUEST_COMPLETED, 1);
        ds4_metric_sub(DS4_M_REQUESTS_INFLIGHT);
    }
    return NULL;
}

int main(void) {
    pthread_t threads[THREADS];
    ds4_metrics_init(DS4_METRICS_MODE_CONTINUOUS, 8);
    for (int i = 0; i < THREADS; i++)
        require(pthread_create(&threads[i], NULL, writer, NULL) == 0,
                "pthread_create");
    for (int i = 0; i < THREADS; i++)
        require(pthread_join(threads[i], NULL) == 0, "pthread_join");

    const uint64_t total = (uint64_t)THREADS * ITERATIONS;
    require(ds4_metrics_mode_value == DS4_METRICS_MODE_CONTINUOUS, "serving mode");
    require(ds4_metrics_resident_sessions == 8, "resident sessions");
    require(ds4_metric_read(DS4_M_REQUESTS_STARTED) == total, "requests started");
    require(ds4_metric_read(DS4_M_REQUEST_COMPLETED) == total, "completed requests");
    require(ds4_metric_read(DS4_M_REQUESTS_INFLIGHT) == 0, "inflight returns to zero");
    require(ds4_metric_read(DS4_M_PREFILL_COMPUTED) == total, "computed prefill tokens");
    require(ds4_metric_read(DS4_M_PREFILL_CACHED) == total, "cached prefill tokens");
    require(ds4_metric_read(DS4_M_DECODE_TOKENS) == total * 2, "decode tokens");
    require(ds4_metric_read(DS4_M_DECODE_STEPS) == total, "decode steps");
    require(ds4_metric_read(DS4_M_DECODE_ROWS) == total * 3, "decode rows");
    require(ds4_metric_read(DS4_M_ACTIVE_PREFILL) == 0, "prefill active returns to zero");
    require(ds4_metric_read(DS4_M_ACTIVE_DECODE) == 0, "decode active returns to zero");
    require(ds4_metric_read(DS4_M_WAITING_PREFILL) == 0, "prefill waiting returns to zero");
    require(ds4_histogram_read(DS4_H_TTFT, DS4_METRICS_HIST_COUNT_INDEX) == total,
            "ttft count");
    require(ds4_histogram_read(DS4_H_TTFT, 0) == total, "ttft first bucket");
    require(ds4_histogram_read(DS4_H_PREFILL, DS4_METRICS_HIST_COUNT_INDEX) == total,
            "prefill histogram count");
    require(ds4_histogram_read(DS4_H_DECODE, DS4_METRICS_HIST_COUNT_INDEX) == total,
            "decode histogram count");
    puts("metrics: OK");
    return 0;
}
