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
        ds4_metrics_request_started();
        ds4_metrics_prefill_cached_add(1);

        uint64_t wait_started =
            ds4_metrics_wait_begin(DS4_METRICS_PHASE_PREFILL);
        ds4_metrics_wait_end(DS4_METRICS_PHASE_PREFILL, wait_started);

        uint64_t compute_started =
            ds4_metrics_compute_begin(DS4_METRICS_PHASE_PREFILL);
        ds4_metrics_prefill_compute_end(compute_started, 1);

        compute_started = ds4_metrics_compute_begin(DS4_METRICS_PHASE_DECODE);
        ds4_metrics_decode_compute_end(compute_started, 3, 1);
        ds4_metrics_decode_tokens_add(2);

        ds4_metrics_observe_ttft(50000000ull);
        ds4_metrics_observe_prefill_request(200000000ull);
        ds4_metrics_observe_decode_request(2000000000ull);
        ds4_metrics_request_finished(DS4_METRICS_OUTCOME_COMPLETED);
    }
    return NULL;
}

int main(void) {
    pthread_t threads[THREADS];
    ds4_metrics_init(DS4_METRICS_MODE_CONTINUOUS, 8);
    for (int i = 0; i < THREADS; i++) {
        require(pthread_create(&threads[i], NULL, writer, NULL) == 0,
                "pthread_create");
    }
    for (int i = 0; i < THREADS; i++) {
        require(pthread_join(threads[i], NULL) == 0, "pthread_join");
    }

    const uint64_t total = (uint64_t)THREADS * ITERATIONS;
    ds4_metrics_snapshot snapshot;
    ds4_metrics_snapshot_read(&snapshot);
    require(snapshot.mode == DS4_METRICS_MODE_CONTINUOUS, "serving mode");
    require(snapshot.resident_sessions == 8, "resident sessions");
    require(snapshot.requests_started == total, "requests started");
    require(snapshot.requests[DS4_METRICS_OUTCOME_COMPLETED] == total,
            "completed requests");
    require(snapshot.requests_inflight == 0, "inflight returns to zero");
    require(snapshot.prefill_tokens_computed == total,
            "computed prefill tokens");
    require(snapshot.prefill_tokens_cached == total,
            "cached prefill tokens");
    require(snapshot.decode_tokens == total * 2, "decode tokens");
    require(snapshot.decode_steps == total, "decode steps");
    require(snapshot.decode_batch_rows == total * 3, "decode batch rows");
    require(snapshot.phase_active[DS4_METRICS_PHASE_PREFILL] == 0,
            "prefill active returns to zero");
    require(snapshot.phase_active[DS4_METRICS_PHASE_DECODE] == 0,
            "decode active returns to zero");
    require(snapshot.phase_waiting[DS4_METRICS_PHASE_PREFILL] == 0,
            "prefill waiting returns to zero");
    require(snapshot.ttft.count == total, "ttft count");
    require(snapshot.ttft.bucket[0] == total, "ttft first bucket");
    require(snapshot.prefill_request.count == total, "prefill histogram count");
    require(snapshot.decode_request.count == total, "decode histogram count");
    puts("metrics: OK");
    return 0;
}
