#ifndef RESOURCES_H
#define RESOURCES_H

#include <pthread.h>
#include <semaphore.h>
#include "patient.h"

typedef struct {
    sem_t doctors;
    sem_t machines;
    sem_t rooms;
    pthread_mutex_t log_mutex; // protects metrics/log updates
    int num_doctors;
    int num_machines;
    int num_rooms;
    unsigned long long busy_doctors_ms;
    unsigned long long busy_machines_ms;
    unsigned long long busy_rooms_ms;
} ResourcePool;

int resources_init(ResourcePool *rp, int num_doctors, int num_machines, int num_rooms);
void resources_destroy(ResourcePool *rp);

sem_t *resource_for_service(ResourcePool *rp, ServiceType service);

#endif // RESOURCES_H
