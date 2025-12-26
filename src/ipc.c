// Ensure POSIX prototypes (ftruncate) are visible
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "ipc.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

int ipc_setup_fifo() {
    // Create named FIFO; ignore EEXIST
    if (mkfifo(FIFO_PATH, 0666) == -1) {
        if (errno != EEXIST) {
            perror("mkfifo");
            return -1;
        }
    }
    return 0;
}

int ipc_cleanup_fifo() {
    unlink(FIFO_PATH);
    return 0;
}

mqd_t ipc_open_mq(int create) {
    struct mq_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = MQ_MSG_MAX;
    attr.mq_curmsgs = 0;

    mqd_t mq;
    if (create) {
        mq = mq_open(MQ_NAME, O_CREAT | O_RDWR, 0666, &attr);
    } else {
        mq = mq_open(MQ_NAME, O_RDWR);
    }
    if (mq == (mqd_t)-1) perror("mq_open");
    return mq;
}

int ipc_close_mq(mqd_t mq) {
    if (mq_close(mq) == -1) {
        perror("mq_close");
        return -1;
    }
    return 0;
}

int ipc_setup_shm(int *fd, SharedStats **stats_ptr, int create) {
    int flags = create ? (O_CREAT | O_RDWR) : O_RDWR;
    int shm_fd = shm_open(SHM_NAME, flags, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        return -1;
    }
    if (create) {
        if (ftruncate(shm_fd, (off_t)sizeof(SharedStats)) == -1) {
            perror("ftruncate");
            close(shm_fd);
            return -1;
        }
    }
    void *addr = mmap(NULL, sizeof(SharedStats), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap");
        close(shm_fd);
        return -1;
    }
    *fd = shm_fd;
    *stats_ptr = (SharedStats *)addr;
    return 0;
}

int ipc_cleanup_shm() {
    shm_unlink(SHM_NAME);
    return 0;
}
