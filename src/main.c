#include "common.h"
#include "patient.h"
#include "scheduler.h"
#include "resources.h"
#include "thread_worker.h"
#include "ipc.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

static Algorithm parse_alg(const char *s) {
    if (strcmp(s, "fcfs") == 0) return ALG_FCFS;
    if (strcmp(s, "sjf") == 0) return ALG_SJF;
    if (strcmp(s, "priority") == 0) return ALG_PRIORITY;
    if (strcmp(s, "rr") == 0) return ALG_RR;
    return ALG_FCFS;
}

int main(int argc, char **argv) {
    // Defaults
    Algorithm alg = ALG_FCFS;
    int num_patients = 10;
    int num_doctors = 3, num_machines = 2, num_rooms = 4;
    unsigned quantum_ms = 3; // for RR

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--alg") == 0 && i+1 < argc) alg = parse_alg(argv[++i]);
        else if (strcmp(argv[i], "--patients") == 0 && i+1 < argc) num_patients = atoi(argv[++i]);
        else if (strcmp(argv[i], "--doctors") == 0 && i+1 < argc) num_doctors = atoi(argv[++i]);
        else if (strcmp(argv[i], "--machines") == 0 && i+1 < argc) num_machines = atoi(argv[++i]);
        else if (strcmp(argv[i], "--rooms") == 0 && i+1 < argc) num_rooms = atoi(argv[++i]);
        else if (strcmp(argv[i], "--quantum") == 0 && i+1 < argc) quantum_ms = (unsigned)atoi(argv[++i]);
    }

    // IPC setup
    if (ipc_setup_fifo() != 0) return 1;
    mqd_t mq = ipc_open_mq(1);
    int shm_fd = -1; SharedStats *stats = NULL;
    if (ipc_setup_shm(&shm_fd, &stats, 1) != 0) {
        fprintf(stderr, "Failed to setup shared memory\n");
        return 1;
    }

    // Fork logger and exec
    pid_t pid = fork();
    if (pid == 0) {
        execl("bin/logger", "logger", (char *)NULL);
        perror("exec logger");
        _exit(127);
    }

    // Parent opens FIFO for writing
    int fifo_fd = open(FIFO_PATH, O_WRONLY);
    if (fifo_fd == -1) {
        perror("open FIFO for write");
        return 1;
    }

    // Resources
    ResourcePool resources;
    if (resources_init(&resources, num_doctors, num_machines, num_rooms) != 0) {
        fprintf(stderr, "Failed to init resources\n");
        return 1;
    }

    // Patients
    PatientList list = create_patients((size_t)num_patients);

    // Scheduling
    int *order = schedule_order(&list, alg, quantum_ms);
    ScheduleMetrics metrics = compute_metrics(&list, order, alg, quantum_ms);

    // Write stats to shared memory and notify logger via MQ
    stats->avg_wait_ms = metrics.avg_wait_ms;
    stats->avg_turnaround_ms = metrics.avg_turnaround_ms;
    stats->completed_jobs = (int)list.count;
    mq_send(mq, "STATS_READY", strlen("STATS_READY"), 1);

    // Launch threads in scheduled order
    pthread_t *threads = (pthread_t *)calloc(list.count, sizeof(pthread_t));
    WorkerArgs *args = (WorkerArgs *)calloc(list.count, sizeof(WorkerArgs));
    for (size_t k = 0; k < list.count; ++k) {
        int idx = order[k]; 
        args[k].patient = list.items[idx];
        args[k].resources = &resources;
        args[k].fifo_fd = fifo_fd;
        pthread_create(&threads[k], NULL, patient_thread, &args[k]);
        // Space out starts slightly to reflect scheduling order
        ms_sleep(10);
    }

    for (size_t k = 0; k < list.count; ++k) 
    {
        pthread_join(threads[k], NULL);
    }

    // Cleanup
    free(order);
    free(threads);
    free(args);
    free_patients(&list);

    resources_destroy(&resources);

    close(fifo_fd);
    ipc_close_mq(mq);

    // Allow logger to drain; then cleanup IPC objects
    ms_sleep(100);
    ipc_cleanup_fifo();
    munmap(stats, sizeof(*stats));
    close(shm_fd);
    ipc_cleanup_shm();

    // Wait for logger
    int status = 0; waitpid(pid, &status, 0);

    printf("Algorithm: %s\n", alg_name(alg));
    printf("Average Waiting Time: %.2f ms\n", metrics.avg_wait_ms);
    printf("Average Turnaround Time: %.2f ms\n", metrics.avg_turnaround_ms);
    printf("Completed Jobs: %d\n", (int)list.count);

    return 0;
}
