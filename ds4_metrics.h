#ifndef DS4_METRICS_H
#define DS4_METRICS_H

#include <stdint.h>
#include <string.h>
#include <time.h>

#define DS4_METRICS_LATENCY_BUCKETS 11
#define DS4_METRICS_HIST_COUNT_INDEX 11
#define DS4_METRICS_HIST_SUM_INDEX 12

typedef enum { DS4_METRICS_MODE_SERIAL, DS4_METRICS_MODE_RESIDENT_BATCHED,
               DS4_METRICS_MODE_CONTINUOUS } ds4_metrics_mode;
typedef enum { DS4_METRICS_PHASE_PREFILL, DS4_METRICS_PHASE_DECODE } ds4_metrics_phase;
typedef enum { DS4_METRICS_OUTCOME_COMPLETED, DS4_METRICS_OUTCOME_FAILED,
               DS4_METRICS_OUTCOME_CANCELLED } ds4_metrics_outcome;
#define DS4_METRIC_LIST(X) \
    X(REQUESTS_STARTED, "# TYPE ds4_requests_started_total counter\nds4_requests_started_total", 0) \
    X(REQUEST_COMPLETED, "# TYPE ds4_requests_total counter\nds4_requests_total{outcome=\"completed\"}", 0) \
    X(REQUEST_FAILED, "ds4_requests_total{outcome=\"failed\"}", 0) \
    X(REQUEST_CANCELLED, "ds4_requests_total{outcome=\"cancelled\"}", 0) \
    X(REQUESTS_INFLIGHT, "# TYPE ds4_requests_inflight gauge\nds4_requests_inflight", 0) \
    X(PREFILL_COMPUTED, "# TYPE ds4_prefill_tokens_total counter\nds4_prefill_tokens_total{kind=\"computed\"}", 0) \
    X(PREFILL_CACHED, "ds4_prefill_tokens_total{kind=\"cached\"}", 0) \
    X(DECODE_TOKENS, "# TYPE ds4_decode_tokens_total counter\nds4_decode_tokens_total", 0) \
    X(DECODE_STEPS, "# TYPE ds4_decode_steps_total counter\nds4_decode_steps_total", 0) \
    X(DECODE_ROWS, "# TYPE ds4_decode_batch_rows_total counter\nds4_decode_batch_rows_total", 0) \
    X(COMPUTE_PREFILL, "# TYPE ds4_prefill_compute_seconds_total counter\nds4_prefill_compute_seconds_total", 1) \
    X(COMPUTE_DECODE, "# TYPE ds4_decode_compute_seconds_total counter\nds4_decode_compute_seconds_total", 1) \
    X(ACTIVE_PREFILL, "# TYPE ds4_phase_active gauge\nds4_phase_active{phase=\"prefill\"}", 0) \
    X(ACTIVE_DECODE, "ds4_phase_active{phase=\"decode\"}", 0) \
    X(WAITING_PREFILL, "# TYPE ds4_phase_waiting gauge\nds4_phase_waiting{phase=\"prefill\"}", 0) \
    X(WAITING_DECODE, "ds4_phase_waiting{phase=\"decode\"}", 0) \
    X(WAIT_PREFILL, "# TYPE ds4_scheduler_wait_seconds_total counter\nds4_scheduler_wait_seconds_total{phase=\"prefill\"}", 1) \
    X(WAIT_DECODE, "ds4_scheduler_wait_seconds_total{phase=\"decode\"}", 1)

typedef enum {
#define DS4_METRIC_ENUM(id, prefix, seconds) DS4_M_##id,
    DS4_METRIC_LIST(DS4_METRIC_ENUM)
#undef DS4_METRIC_ENUM
    DS4_M_COUNT
} ds4_metric;
typedef enum { DS4_H_TTFT, DS4_H_PREFILL, DS4_H_DECODE, DS4_H_COUNT } ds4_histogram;

extern uint64_t ds4_metric_value[DS4_M_COUNT];
extern uint64_t ds4_histogram_value[DS4_H_COUNT][13];
extern uint64_t ds4_metrics_boot_ns, ds4_metrics_resident_sessions;
extern ds4_metrics_mode ds4_metrics_mode_value;
static const uint64_t ds4_metrics_bounds[] = {
    100000000ull, 250000000ull, 500000000ull, 1000000000ull,
    2500000000ull, 5000000000ull, 10000000000ull, 30000000000ull,
    60000000000ull, 120000000000ull, 300000000000ull
};

static inline uint64_t ds4_metric_read(ds4_metric id) {
    return __atomic_load_n(&ds4_metric_value[id], __ATOMIC_RELAXED);
}
static inline void ds4_metric_add(ds4_metric id, uint64_t n) {
    if (n) __atomic_fetch_add(&ds4_metric_value[id], n, __ATOMIC_RELAXED);
}
static inline void ds4_metric_sub(ds4_metric id) {
    uint64_t n = ds4_metric_read(id);
    while (n && !__atomic_compare_exchange_n(&ds4_metric_value[id], &n, n - 1, 1,
           __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {}
}
static inline uint64_t ds4_metrics_now_ns(void) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}
static inline void ds4_metrics_init(ds4_metrics_mode mode, uint64_t sessions) {
    memset(ds4_metric_value, 0, sizeof(ds4_metric_value));
    memset(ds4_histogram_value, 0, sizeof(ds4_histogram_value));
    ds4_metrics_mode_value = mode; ds4_metrics_resident_sessions = sessions;
    ds4_metrics_boot_ns = ds4_metrics_now_ns();
}
static inline uint64_t ds4_metrics_phase_begin(ds4_metric gauge, ds4_metrics_phase phase) {
    ds4_metric_add((ds4_metric)(gauge + phase), 1); return ds4_metrics_now_ns();
}
static inline void ds4_metrics_phase_end(ds4_metric gauge, ds4_metric total,
                                         ds4_metrics_phase phase, uint64_t started) {
    ds4_metric_sub((ds4_metric)(gauge + phase));
    uint64_t now = ds4_metrics_now_ns();
    ds4_metric_add((ds4_metric)(total + phase), now >= started ? now - started : 0);
}
static inline void ds4_metrics_histogram_observe(ds4_histogram id, uint64_t ns) {
    uint64_t *h = ds4_histogram_value[id];
    __atomic_fetch_add(&h[11], 1, __ATOMIC_RELAXED);
    __atomic_fetch_add(&h[12], ns, __ATOMIC_RELAXED);
    for (int i = 0; i < 11; i++) if (ns <= ds4_metrics_bounds[i])
        __atomic_fetch_add(&h[i], 1, __ATOMIC_RELAXED);
}
static inline uint64_t ds4_histogram_read(ds4_histogram id, int field) {
    return __atomic_load_n(&ds4_histogram_value[id][field], __ATOMIC_RELAXED);
}
static inline const char *ds4_metrics_mode_name(void) {
    static const char *name[] = {"serial", "resident_batched", "continuous"};
    return (unsigned)ds4_metrics_mode_value < 3 ? name[ds4_metrics_mode_value] : name[0];
}
#endif
