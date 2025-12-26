#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "patient.h"

typedef enum {
    ALG_FCFS = 0,
    ALG_SJF = 1,
    ALG_PRIORITY = 2,
    ALG_RR = 3
} Algorithm;

typedef struct {
    double avg_wait_ms;
    double avg_turnaround_ms;
} ScheduleMetrics;

// Returns an array of indices representing scheduling order.
int *schedule_order(const PatientList *list, Algorithm alg, unsigned quantum_ms);

// Compute waiting and turnaround times per patient based on order.
ScheduleMetrics compute_metrics(const PatientList *list, const int *order, Algorithm alg, unsigned quantum_ms);

const char *alg_name(Algorithm alg);

#endif // SCHEDULER_H
