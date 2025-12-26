#ifndef PATIENT_H
#define PATIENT_H

#include "common.h"

typedef enum {
    SERVICE_CONSULTATION = 0,
    SERVICE_LAB_TEST = 1,
    SERVICE_TREATMENT = 2
} ServiceType;

typedef struct {
    int id;
    char name[MAX_NAME_LEN];
    int priority;               // lower value == higher priority (1 highest)
    ServiceType service;
    unsigned required_time_ms;  // CPU burst time equivalent
    unsigned arrival_ms;        // arrival time in ms
} Patient;

typedef struct {
    Patient *items;
    size_t count;
} PatientList;

PatientList create_patients(size_t n);
void free_patients(PatientList *list);

#endif // PATIENT_H
