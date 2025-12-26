#include "thread_worker.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static const char *service_name(ServiceType s) {
    switch (s) {
        case SERVICE_CONSULTATION: return "Consultation";
        case SERVICE_LAB_TEST: return "LabTest";
        case SERVICE_TREATMENT: return "Treatment";
        default: return "Unknown";
    }
}

void *patient_thread(void *arg) {
    WorkerArgs *wa = (WorkerArgs *)arg;
    Patient p = wa->patient;
    sem_t *res = resource_for_service(wa->resources, p.service);

    char buf[256];
    snprintf(buf, sizeof(buf), "START id=%d name=%s service=%s\n", p.id, p.name, service_name(p.service));
    write(wa->fifo_fd, buf, strlen(buf));

    sem_wait(res);
    ms_sleep(p.required_time_ms);
    sem_post(res);

    // Accumulate resource busy time (equals required time for non-preemptive service)
    pthread_mutex_lock(&wa->resources->log_mutex);
    switch (p.service) {
        case SERVICE_CONSULTATION:
            wa->resources->busy_doctors_ms += p.required_time_ms;
            break;
        case SERVICE_LAB_TEST:
            wa->resources->busy_machines_ms += p.required_time_ms;
            break;
        case SERVICE_TREATMENT:
            wa->resources->busy_rooms_ms += p.required_time_ms;
            break;
        default:
            break;
    }
    pthread_mutex_unlock(&wa->resources->log_mutex);

    snprintf(buf, sizeof(buf), "FINISH id=%d name=%s service=%s\n", p.id, p.name, service_name(p.service));
    write(wa->fifo_fd, buf, strlen(buf));

    return NULL;
}
