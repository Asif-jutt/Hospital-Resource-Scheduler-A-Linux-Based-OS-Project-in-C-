#include "scheduler.h"

#include <stdio.h>
#include <stdlib.h>

// Portable comparators using a static context pointer
static const PatientList *g_cmp_ctx = NULL;

static int cmp_fcfs(const void *a, const void *b) {
    int ia = *(const int *)a, ib = *(const int *)b;
    unsigned aa = g_cmp_ctx->items[ia].arrival_ms;
    unsigned ab = g_cmp_ctx->items[ib].arrival_ms;
    if (aa < ab) return -1;
    if (aa > ab) return 1;
    return ia - ib;
}

static int cmp_sjf(const void *a, const void *b) {
    int ia = *(const int *)a, ib = *(const int *)b;
    unsigned ba = g_cmp_ctx->items[ia].required_time_ms;
    unsigned bb = g_cmp_ctx->items[ib].required_time_ms;
    if (ba < bb) return -1;
    if (ba > bb) return 1;
    return ia - ib;
}

static int cmp_priority(const void *a, const void *b) {
    int ia = *(const int *)a, ib = *(const int *)b;
    int pa = g_cmp_ctx->items[ia].priority;
    int pb = g_cmp_ctx->items[ib].priority;
    if (pa < pb) return -1; // lower value means higher priority
    if (pa > pb) return 1;
    return ia - ib;
}

int *schedule_order(const PatientList *list, Algorithm alg, unsigned quantum_ms) {
    (void)quantum_ms; // not used in pure ordering
    int *order = (int *)malloc(sizeof(int) * list->count);
    if (!order) return NULL;
    for (size_t i = 0; i < list->count; ++i) order[i] = (int)i;

    g_cmp_ctx = list;
    switch (alg) {
        case ALG_FCFS:
            qsort(order, list->count, sizeof(int), cmp_fcfs);
            break;
        case ALG_SJF:
            qsort(order, list->count, sizeof(int), cmp_sjf);
            break;
        case ALG_PRIORITY:
            qsort(order, list->count, sizeof(int), cmp_priority);
            break;
        case ALG_RR:
            // For RR, we keep arrival order; detailed slicing handled in metrics
            qsort(order, list->count, sizeof(int), cmp_fcfs);
            break;
        default:
            break;
    }
    g_cmp_ctx = NULL;
    return order;
}

ScheduleMetrics compute_metrics(const PatientList *list, const int *order, Algorithm alg, unsigned quantum_ms) {
    ScheduleMetrics m = {0};
    size_t n = list->count;
    if (n == 0) return m;

    if (alg == ALG_RR) {
        // Accurate Round Robin with circular ready queue
        unsigned *remaining = (unsigned *)malloc(sizeof(unsigned) * n);
        unsigned *arrival = (unsigned *)malloc(sizeof(unsigned) * n);
        unsigned *finish = (unsigned *)calloc(n, sizeof(unsigned));
        unsigned *waiting = (unsigned *)calloc(n, sizeof(unsigned));
        for (size_t i = 0; i < n; ++i) {
            remaining[i] = list->items[i].required_time_ms;
            arrival[i] = list->items[i].arrival_ms;
        }

        // Make a local copy of the order and sort by arrival for queueing
        int *arrival_order = (int *)malloc(sizeof(int) * n);
        for (size_t i = 0; i < n; ++i) arrival_order[i] = order[i];
        g_cmp_ctx = list; qsort(arrival_order, n, sizeof(int), cmp_fcfs); g_cmp_ctx = NULL;

        int *queue = (int *)malloc(sizeof(int) * n);
        size_t head = 0, tail = 0, qcount = 0;

        unsigned time = 0;
        size_t completed = 0;
        size_t next_arrival = 0;

        // Jump to first arrival if needed
        if (next_arrival < n) time = arrival[arrival_order[next_arrival]];

        while (completed < n) {
            // Enqueue all that have arrived by current time
            while (next_arrival < n) {
                int pid = arrival_order[next_arrival];
                if (arrival[pid] <= time) {
                    queue[tail] = pid; tail = (tail + 1) % n; qcount++;
                    next_arrival++;
                } else break;
            }

            if (qcount == 0) {
                // No ready processes; jump to next arrival
                if (next_arrival < n) {
                    time = arrival[arrival_order[next_arrival]];
                    continue;
                }
            }

            // Dequeue next ready process
            int pid = queue[head]; head = (head + 1) % n; qcount--;
            if (remaining[pid] == 0) {
                // Already completed; skip
                continue;
            }
            unsigned slice = remaining[pid] > quantum_ms ? quantum_ms : remaining[pid];
            // Other ready processes wait during this slice
            for (size_t i = 0, idx = head; i < qcount; ++i) {
                int other = queue[idx];
                idx = (idx + 1) % n;
                if (remaining[other] > 0) waiting[other] += slice;
            }
            time += slice;
            remaining[pid] -= slice;

            // Enqueue any new arrivals that occurred during the slice
            while (next_arrival < n) {
                int np = arrival_order[next_arrival];
                if (arrival[np] <= time) {
                    queue[tail] = np; tail = (tail + 1) % n; qcount++;
                    next_arrival++;
                } else break;
            }

            if (remaining[pid] > 0) {
                // Requeue current process
                queue[tail] = pid; tail = (tail + 1) % n; qcount++;
            } else {
                finish[pid] = time;
                completed++;
            }
        }

        double total_wait = 0.0, total_turn = 0.0;
        for (size_t i = 0; i < n; ++i) {
            unsigned turnaround = finish[i] - arrival[i];
            total_turn += turnaround;
            total_wait += waiting[i];
        }
        m.avg_wait_ms = total_wait / n;
        m.avg_turnaround_ms = total_turn / n;
        free(remaining); free(arrival); free(finish); free(waiting); free(queue); free(arrival_order);
        return m;
    }

    // Non-preemptive algorithms
    unsigned time = 0;
    double total_wait = 0.0, total_turn = 0.0;
    for (size_t k = 0; k < n; ++k) {
        int i = order[k];
        Patient p = list->items[i];
        if (p.arrival_ms > time) time = p.arrival_ms;
        unsigned waiting = time - p.arrival_ms;
        total_wait += waiting;
        time += p.required_time_ms;
        unsigned turnaround = time - p.arrival_ms;
        total_turn += turnaround;
    }
    m.avg_wait_ms = total_wait / n;
    m.avg_turnaround_ms = total_turn / n;
    return m;
}

const char *alg_name(Algorithm alg) {
    switch (alg) {
        case ALG_FCFS: return "FCFS";
        case ALG_SJF: return "SJF";
        case ALG_PRIORITY: return "Priority";
        case ALG_RR: return "Round Robin";
        default: return "Unknown";
    }
}
