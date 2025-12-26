#include "resources.h"
#include <stdio.h>

int resources_init(ResourcePool *rp, int num_doctors, int num_machines, int num_rooms) {
    if (sem_init(&rp->doctors, 0, (unsigned int)(num_doctors > 0 ? num_doctors : 1)) != 0) return -1;
    if (sem_init(&rp->machines, 0, (unsigned int)(num_machines > 0 ? num_machines : 1)) != 0) return -1;
    if (sem_init(&rp->rooms, 0, (unsigned int)(num_rooms > 0 ? num_rooms : 1)) != 0) return -1;
    if (pthread_mutex_init(&rp->log_mutex, NULL) != 0) return -1;
    rp->num_doctors = (num_doctors > 0 ? num_doctors : 1);
    rp->num_machines = (num_machines > 0 ? num_machines : 1);
    rp->num_rooms = (num_rooms > 0 ? num_rooms : 1);
    rp->busy_doctors_ms = 0ULL;
    rp->busy_machines_ms = 0ULL;
    rp->busy_rooms_ms = 0ULL;
    return 0;
}

void resources_destroy(ResourcePool *rp) {
    sem_destroy(&rp->doctors);
    sem_destroy(&rp->machines);
    sem_destroy(&rp->rooms);
    pthread_mutex_destroy(&rp->log_mutex);
}

sem_t *resource_for_service(ResourcePool *rp, ServiceType service) {
    switch (service) {
        case SERVICE_CONSULTATION: return &rp->doctors;
        case SERVICE_LAB_TEST: return &rp->machines;
        case SERVICE_TREATMENT: return &rp->rooms;
        default: return &rp->rooms;
    }
}
