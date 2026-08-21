#define _POSIX_C_SOURCE 200809L

#include "ds4_metrics.h"

#include <stddef.h>
#include <string.h>
#include <time.h>

typedef struct {
    uint64_t bucket[DS4_METRICS_LATENCY_BUCKETS];
    uint64_t count;
    uint64_t sum_ns;
} ds4_metrics_histogram;

typedef struct {
    uint64_t mode;
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
    ds4_metrics_histogram ttft;
    ds4_metrics_histogram prefill_request;
    ds4_metrics_histogram decode_request;
} ds4_metrics_registry;

static ds4_metrics_registry registry;

static const uint64_t latency_bounds_ns[DS4_METRICS_LATENCY_BUCKETS] = {
    100000000ull,
    250000000ull,
    500000000ull,
    1000000000ull,
    2500000000ull,
    5000000000ull,
    10000000000ull,
    30000000000ull,
    60000000000ull,
    120000000000ull,
    300000000000ull,
};

static uint64_t metric_read(const uint64_t *value) {
    return __atomic_load_n(value, __ATOMIC_RELAXED);
}

static void metric_set(uint64_t *value, uint64_t next) {
    __atomic_store_n(value, next, __ATOMIC_RELAXED);
}

static void metric_add(uint64_t *value, uint64_t delta) {
    if (delta) __atomic_fetch_add(value, delta, __ATOMIC_RELAXED);
}

static void metric_increment(uint64_t *value) {
    __atomic_fetch_add(value, 1, __ATOMIC_RELAXED);
}

static void metric_decrement(uint64_t *value) {
    uint64_t current = metric_read(value);
    while (current != 0 &&
           !__atomic_compare_exchange_n(value, &current, current - 1,
                                        1, __ATOMIC_RELAXED,
                                        __ATOMIC_RELAXED)) {
    }
}

static int phase_valid(ds4_metrics_phase phase) {
    return phase >= DS4_METRICS_PHASE_PREFILL &&
           phase < DS4_METRICS_PHASE_COUNT;
}

static uint64_t elapsed_since(uint64_t started_ns) {
    uint64_t now = ds4_metrics_now_ns();
    return now >= started_ns ? now - started_ns : 0;
}

static void histogram_observe(ds4_metrics_histogram *histogram,
                              uint64_t elapsed_ns) {
    metric_increment(&histogram->count);
    metric_add(&histogram->sum_ns, elapsed_ns);
    for (size_t i = 0; i < DS4_METRICS_LATENCY_BUCKETS; i++) {
        if (elapsed_ns <= latency_bounds_ns[i]) {
            metric_increment(&histogram->bucket[i]);
        }
    }
}

static void histogram_snapshot_read(
        const ds4_metrics_histogram *histogram,
        ds4_metrics_histogram_snapshot *out) {
    for (size_t i = 0; i < DS4_METRICS_LATENCY_BUCKETS; i++) {
        out->bucket[i] = metric_read(&histogram->bucket[i]);
    }
    out->count = metric_read(&histogram->count);
    out->sum_ns = metric_read(&histogram->sum_ns);
}

uint64_t ds4_metrics_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

const uint64_t *ds4_metrics_latency_bounds_ns(void) {
    return latency_bounds_ns;
}

const char *ds4_metrics_mode_name(ds4_metrics_mode mode) {
    switch (mode) {
    case DS4_METRICS_MODE_RESIDENT_BATCHED:
        return "resident_batched";
    case DS4_METRICS_MODE_CONTINUOUS:
        return "continuous";
    case DS4_METRICS_MODE_SERIAL:
    default:
        return "serial";
    }
}

void ds4_metrics_init(ds4_metrics_mode mode, uint64_t resident_sessions) {
    memset(&registry, 0, sizeof(registry));
    metric_set(&registry.mode, (uint64_t)mode);
    metric_set(&registry.resident_sessions, resident_sessions);
    metric_set(&registry.boot_ns, ds4_metrics_now_ns());
}

void ds4_metrics_snapshot_read(ds4_metrics_snapshot *out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    out->mode = (ds4_metrics_mode)metric_read(&registry.mode);
    out->boot_ns = metric_read(&registry.boot_ns);
    out->resident_sessions = metric_read(&registry.resident_sessions);
    out->requests_started = metric_read(&registry.requests_started);
    for (size_t i = 0; i < DS4_METRICS_OUTCOME_COUNT; i++) {
        out->requests[i] = metric_read(&registry.requests[i]);
    }
    out->requests_inflight = metric_read(&registry.requests_inflight);
    out->prefill_tokens_computed = metric_read(&registry.prefill_tokens_computed);
    out->prefill_tokens_cached = metric_read(&registry.prefill_tokens_cached);
    out->decode_tokens = metric_read(&registry.decode_tokens);
    out->decode_steps = metric_read(&registry.decode_steps);
    out->decode_batch_rows = metric_read(&registry.decode_batch_rows);
    for (size_t i = 0; i < DS4_METRICS_PHASE_COUNT; i++) {
        out->compute_ns[i] = metric_read(&registry.compute_ns[i]);
        out->phase_active[i] = metric_read(&registry.phase_active[i]);
        out->phase_waiting[i] = metric_read(&registry.phase_waiting[i]);
        out->scheduler_wait_ns[i] = metric_read(&registry.scheduler_wait_ns[i]);
    }
    histogram_snapshot_read(&registry.ttft, &out->ttft);
    histogram_snapshot_read(&registry.prefill_request, &out->prefill_request);
    histogram_snapshot_read(&registry.decode_request, &out->decode_request);
}

void ds4_metrics_request_started(void) {
    metric_increment(&registry.requests_started);
    metric_increment(&registry.requests_inflight);
}

void ds4_metrics_request_finished(ds4_metrics_outcome outcome) {
    if (outcome < 0 || outcome >= DS4_METRICS_OUTCOME_COUNT) {
        outcome = DS4_METRICS_OUTCOME_FAILED;
    }
    metric_increment(&registry.requests[outcome]);
    metric_decrement(&registry.requests_inflight);
}

void ds4_metrics_prefill_cached_add(uint64_t tokens) {
    metric_add(&registry.prefill_tokens_cached, tokens);
}

void ds4_metrics_decode_tokens_add(uint64_t tokens) {
    metric_add(&registry.decode_tokens, tokens);
}

uint64_t ds4_metrics_wait_begin(ds4_metrics_phase phase) {
    if (!phase_valid(phase)) return ds4_metrics_now_ns();
    metric_increment(&registry.phase_waiting[phase]);
    return ds4_metrics_now_ns();
}

void ds4_metrics_wait_end(ds4_metrics_phase phase, uint64_t started_ns) {
    if (!phase_valid(phase)) return;
    metric_decrement(&registry.phase_waiting[phase]);
    metric_add(&registry.scheduler_wait_ns[phase], elapsed_since(started_ns));
}

uint64_t ds4_metrics_compute_begin(ds4_metrics_phase phase) {
    if (!phase_valid(phase)) return ds4_metrics_now_ns();
    metric_increment(&registry.phase_active[phase]);
    return ds4_metrics_now_ns();
}

void ds4_metrics_prefill_compute_end(uint64_t started_ns,
                                     uint64_t computed_tokens) {
    metric_add(&registry.compute_ns[DS4_METRICS_PHASE_PREFILL],
               elapsed_since(started_ns));
    metric_add(&registry.prefill_tokens_computed, computed_tokens);
    metric_decrement(&registry.phase_active[DS4_METRICS_PHASE_PREFILL]);
}

void ds4_metrics_decode_compute_end(uint64_t started_ns,
                                    uint64_t batch_rows,
                                    int success) {
    metric_add(&registry.compute_ns[DS4_METRICS_PHASE_DECODE],
               elapsed_since(started_ns));
    if (success) {
        metric_increment(&registry.decode_steps);
        metric_add(&registry.decode_batch_rows, batch_rows);
    }
    metric_decrement(&registry.phase_active[DS4_METRICS_PHASE_DECODE]);
}

void ds4_metrics_observe_ttft(uint64_t elapsed_ns) {
    histogram_observe(&registry.ttft, elapsed_ns);
}

void ds4_metrics_observe_prefill_request(uint64_t elapsed_ns) {
    histogram_observe(&registry.prefill_request, elapsed_ns);
}

void ds4_metrics_observe_decode_request(uint64_t elapsed_ns) {
    histogram_observe(&registry.decode_request, elapsed_ns);
}
