#ifndef IPC_H
#define IPC_H

// Enable POSIX features
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <mqueue.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>

#define FIFO_PATH "/tmp/hospital_log_fifo"
#define MQ_NAME "/hospital_log_mq"
#define SHM_NAME "/hospital_stats"

// Messages placed on the MQ
#define MQ_MSG_MAX 256

typedef struct {
    double avg_wait_ms;
    double avg_turnaround_ms;
    int completed_jobs;
} SharedStats;

int ipc_setup_fifo();
int ipc_cleanup_fifo();

mqd_t ipc_open_mq(int create);
int ipc_close_mq(mqd_t mq);

int ipc_setup_shm(int *fd, SharedStats **stats_ptr, int create);
int ipc_cleanup_shm();

#endif // IPC_H
