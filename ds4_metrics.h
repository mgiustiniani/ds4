#ifndef DS4_METRICS_H
#define DS4_METRICS_H

#include <stdint.h>

#define DS4_METRICS_LATENCY_BUCKETS 11

typedef enum {
    DS4_METRICS_MODE_SERIAL = 0,
    DS4_METRICS_MODE_RESIDENT_BATCHED = 1,
    DS4_METRICS_MODE_CONTINUOUS = 2,
} ds4_metrics_mode;

typedef enum {
    DS4_METRICS_PHASE_PREFILL = 0,
    DS4_METRICS_PHASE_DECODE = 1,
    DS4_METRICS_PHASE_COUNT = 2,
} ds4_metrics_phase;

typedef enum {
    DS4_METRICS_OUTCOME_COMPLETED = 0,
    DS4_METRICS_OUTCOME_FAILED = 1,
    DS4_METRICS_OUTCOME_CANCELLED = 2,
    DS4_METRICS_OUTCOME_COUNT = 3,
} ds4_metrics_outcome;

typedef struct {
    uint64_t bucket[DS4_METRICS_LATENCY_BUCKETS];
    uint64_t count;
    uint64_t sum_ns;
} ds4_metrics_histogram_snapshot;

typedef struct {
    ds4_metrics_mode mode;
    uint64_t boot_ns;
    uint64_t resident_sessions;
    uint64_t requests_started;
    uint64_t requests[DS4_METRICS_OUTCOME_COUNT];
    uint64_t requests_inflight;
    uint64_t prefill_tokens_computed;
    uint64_t prefill_tokens_cached;
    uint64_t decode_tokens;
    uint64_t decode_steps;
    uint64_t decode_batch_rows;
    uint64_t compute_ns[DS4_METRICS_PHASE_COUNT];
    uint64_t phase_active[DS4_METRICS_PHASE_COUNT];
    uint64_t phase_waiting[DS4_METRICS_PHASE_COUNT];
    uint64_t scheduler_wait_ns[DS4_METRICS_PHASE_COUNT];
    ds4_metrics_histogram_snapshot ttft;
    ds4_metrics_histogram_snapshot prefill_request;
    ds4_metrics_histogram_snapshot decode_request;
} ds4_metrics_snapshot;

uint64_t ds4_metrics_now_ns(void);
const uint64_t *ds4_metrics_latency_bounds_ns(void);
const char *ds4_metrics_mode_name(ds4_metrics_mode mode);

void ds4_metrics_init(ds4_metrics_mode mode, uint64_t resident_sessions);
void ds4_metrics_snapshot_read(ds4_metrics_snapshot *out);

void ds4_metrics_request_started(void);
void ds4_metrics_request_finished(ds4_metrics_outcome outcome);
void ds4_metrics_prefill_cached_add(uint64_t tokens);
void ds4_metrics_decode_tokens_add(uint64_t tokens);

uint64_t ds4_metrics_wait_begin(ds4_metrics_phase phase);
void ds4_metrics_wait_end(ds4_metrics_phase phase, uint64_t started_ns);
uint64_t ds4_metrics_compute_begin(ds4_metrics_phase phase);
void ds4_metrics_prefill_compute_end(uint64_t started_ns,
                                     uint64_t computed_tokens);
void ds4_metrics_decode_compute_end(uint64_t started_ns,
                                    uint64_t batch_rows,
                                    int success);

void ds4_metrics_observe_ttft(uint64_t elapsed_ns);
void ds4_metrics_observe_prefill_request(uint64_t elapsed_ns);
void ds4_metrics_observe_decode_request(uint64_t elapsed_ns);

#endif
