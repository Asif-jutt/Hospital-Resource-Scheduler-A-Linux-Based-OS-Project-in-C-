#ifndef THREAD_WORKER_H
#define THREAD_WORKER_H

#include <pthread.h>
#include "resources.h"
#include "ipc.h"

typedef struct {
    Patient patient;
    ResourcePool *resources;
    int fifo_fd;
} WorkerArgs;

void *patient_thread(void *arg);

#endif // THREAD_WORKER_H
