// Enable POSIX features (must be before any includes)
#define _POSIX_C_SOURCE 200809L

#include "ipc.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

static void *mq_reader(void *arg) {
    FILE *out = (FILE *)arg;
    mqd_t mq = ipc_open_mq(0);
    if (mq == (mqd_t)-1) pthread_exit(NULL);

    char msg[MQ_MSG_MAX];
    unsigned prio;
    while (1) {
        ssize_t n = mq_receive(mq, msg, sizeof(msg), &prio);
        if (n >= 0) {
            msg[n] = '\0';
            fprintf(out, "[MQ] %s\n", msg);
            fflush(out);
            if (strcmp(msg, "STATS_READY") == 0) {
                // Read shared memory stats
                int fd = -1; SharedStats *stats = NULL;
                if (ipc_setup_shm(&fd, &stats, 0) == 0) {
                    fprintf(out, "\nFinal Report:\n");
                    fprintf(out, "Average Waiting Time: %.2f ms\n", stats->avg_wait_ms);
                    fprintf(out, "Average Turnaround Time: %.2f ms\n", stats->avg_turnaround_ms);
                    fprintf(out, "Completed Jobs: %d\n", stats->completed_jobs);
                    fflush(out);
                    munmap(stats, sizeof(*stats));
                    close(fd);
                }
                // After stats, continue reading until FIFO ends; do not exit here
            }
        } else {
            // MQ error; sleep and retry
            ms_sleep(100);
        }
    }
    ipc_close_mq(mq);
    return NULL;
}

int main(void) {
    // Open FIFO for reading
    int fifo_fd = open(FIFO_PATH, O_RDONLY);
    if (fifo_fd == -1) {
        perror("open FIFO");
        return 1;
    }
    FILE *out = fopen("logs/log.txt", "a");
    if (!out) out = stdout;

    pthread_t th;
    pthread_create(&th, NULL, mq_reader, out);

    char buf[256];
    ssize_t n;
    while ((n = read(fifo_fd, buf, sizeof(buf)-1)) > 0) {
        buf[n] = '\0';
        fprintf(out, "%s", buf);
        fflush(out);
    }

    // FIFO closed: cleanup and exit
    // Keep 'out' open until process exit to avoid races with MQ thread.
    close(fifo_fd);
    return 0;
}
