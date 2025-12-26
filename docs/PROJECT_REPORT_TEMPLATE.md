# Hospital Resource Scheduler – Project Report

## Project Title:
**Hospital Resource Scheduler – A Linux-Based OS Project in C**

---

## Group Members:

| Name | Roll No | Responsibilities |
|------|---------|------------------|
| Asif Hussain | 2023-CS-646 | CPU Scheduling & Threads |
| Muhammad Wakeel | 2023-CS-601 | Process Creation, IPC & Synchronization |

---

## Table of Contents
1. [Introduction](#1-introduction)
2. [Objectives](#2-objectives)
3. [Implementation Details](#3-implementation-details)
   - 3.1 CPU Scheduling Algorithms
   - 3.2 Multithreaded Execution
   - 3.3 Dynamic Memory Allocation
   - 3.4 Process Creation
   - 3.5 Inter-Process Communication (IPC)
   - 3.6 Synchronization
4. [Screenshots & Output](#4-screenshots--output)
5. [Conclusion](#5-conclusion)

---

## 1. Introduction

Hospitals receive many patients at the same time, and every patient needs different services. Some need to meet a doctor, some need lab tests, and some need treatment. But a hospital has limited resources like doctors, nurses, and medical machines. If resources are not used properly, patients may face delays.

This project is a **Hospital Resource Scheduler**, designed using Operating System concepts. In our system:
- Every patient request behaves like a job/process.
- It requires CPU time, a resource, and execution time.
- The scheduler decides which patient request should be served first.
- We simulate a real hospital environment using C on Linux.

---

## 2. Objectives

1. To simulate patient requests as jobs running on a scheduler.
2. To implement multiple CPU scheduling algorithms and compare their results.
3. To use multithreading to run patient tasks in parallel.
4. To show process creation using `fork()` and `exec()`.
5. To implement Inter-Process Communication (IPC) between scheduler and logger.
6. To use semaphores and mutexes to manage shared hospital resources safely.
7. To demonstrate dynamic memory allocation for patient data.

---

## 3. Implementation Details

---

### 3.1 CPU Scheduling Algorithms

**Concept:** The scheduler supports multiple scheduling techniques to determine which patient should be served first.

#### Algorithms Implemented:
| Algorithm | Description |
|-----------|-------------|
| FCFS (First Come First Serve) | Patients served in arrival order |
| SJF (Shortest Job First) | Patients with shortest required time served first |
| Priority Scheduling | Higher priority patients (lower number) served first |
| Round Robin (RR) | Each patient gets a time quantum, then rotates |

#### Code Location: `src/scheduler.c`

```c
/* ─────────────────────────────────────────────────────────────────────────────
   📸 SCREENSHOT: Code from scheduler.c - schedule_order() function
   ───────────────────────────────────────────────────────────────────────────── */

// Comparison context for portable qsort
static struct {
    const PatientList *list;
    Algorithm alg;
} g_cmp_ctx;

// Comparison function for FCFS - sorts by arrival time
static int cmp_fcfs(const void *a, const void *b) {
    int ia = *(const int *)a, ib = *(const int *)b;
    unsigned aa = g_cmp_ctx.list->items[ia].arrival_ms;
    unsigned ab = g_cmp_ctx.list->items[ib].arrival_ms;
    if (aa < ab) return -1;
    if (aa > ab) return 1;
    return ia - ib;
}

// Comparison function for SJF - sorts by required time (shortest first)
static int cmp_sjf(const void *a, const void *b) {
    int ia = *(const int *)a, ib = *(const int *)b;
    unsigned ra = g_cmp_ctx.list->items[ia].required_time_ms;
    unsigned rb = g_cmp_ctx.list->items[ib].required_time_ms;
    if (ra < rb) return -1;
    if (ra > rb) return 1;
    return ia - ib;
}

// Comparison function for Priority - sorts by priority (lower = higher priority)
static int cmp_priority(const void *a, const void *b) {
    int ia = *(const int *)a, ib = *(const int *)b;
    int pa = g_cmp_ctx.list->items[ia].priority;
    int pb = g_cmp_ctx.list->items[ib].priority;
    if (pa < pb) return -1;
    if (pa > pb) return 1;
    return ia - ib;
}

// Main scheduling function
int *schedule_order(const PatientList *list, Algorithm alg, unsigned quantum_ms) {
    size_t n = list->count;
    int *order = (int *)malloc(sizeof(int) * n);
    for (size_t i = 0; i < n; ++i) order[i] = (int)i;
    
    g_cmp_ctx.list = list;
    g_cmp_ctx.alg = alg;
    
    switch (alg) {
        case ALG_FCFS:    qsort(order, n, sizeof(int), cmp_fcfs); break;
        case ALG_SJF:     qsort(order, n, sizeof(int), cmp_sjf); break;
        case ALG_PRIORITY: qsort(order, n, sizeof(int), cmp_priority); break;
        case ALG_RR:      qsort(order, n, sizeof(int), cmp_fcfs); break;
    }
    return order;
}
```

#### 📸 Screenshot Placeholder: Algorithm Selection Menu
```
+----------------------------------------------+
|        SELECT SCHEDULING ALGORITHM           |
+----------------------------------------------+
  1. FCFS (First Come First Serve)
  2. SJF (Shortest Job First)
  3. Priority Scheduling
  4. Round Robin (Preemptive)

Up/Down: Navigate | Enter: Select | q: Cancel
```
**[INSERT SCREENSHOT: Algorithm selection menu from UI]**

#### 📸 Screenshot Placeholder: Algorithm Comparison Results
```
+------------------------------------------------------------------------------+
|                    ALGORITHM COMPARISON REPORT                               |
+------------------------------------------------------------------------------+
Patients: 5 | RR Quantum: 100 ms | Doctors: 2 | Machines: 2 | Rooms: 2
------------------------------------------------------------------------------
Algorithm        Avg Wait (ms)        Avg Turnaround (ms)  Winner
------------------------------------------------------------------------------
FCFS             XXX.XX               XXX.XX               
SJF              XXX.XX               XXX.XX               BEST
Priority         XXX.XX               XXX.XX               
Round Robin      XXX.XX               XXX.XX               
------------------------------------------------------------------------------
```
**[INSERT SCREENSHOT: Compare Algorithms output showing all 4 algorithms]**

---

### 3.2 Multithreaded Execution

**Concept:** Each patient request is handled by a separate thread, running in parallel to simulate multiple real-time requests.

#### Thread Types by Service:
| Service Type | Resource Used | Description |
|--------------|---------------|-------------|
| Consultation | Doctor | Patient meets with a doctor |
| Lab Test | Machine | Patient uses lab equipment |
| Treatment | Room | Patient receives treatment in a room |

#### Code Location: `src/thread_worker.c`

```c
/* ─────────────────────────────────────────────────────────────────────────────
   📸 SCREENSHOT: Code from thread_worker.c - patient_thread() function
   ───────────────────────────────────────────────────────────────────────────── */

#include "thread_worker.h"
#include <unistd.h>
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
    
    // Get the appropriate resource semaphore for this service type
    sem_t *res = resource_for_service(wa->resources, p.service);

    // Log START event via FIFO
    char buf[256];
    snprintf(buf, sizeof(buf), "START id=%d name=%s service=%s\n", 
             p.id, p.name, service_name(p.service));
    write(wa->fifo_fd, buf, strlen(buf));

    // Wait for resource (semaphore) - BLOCKS if resource unavailable
    sem_wait(res);
    
    // Simulate patient service time
    ms_sleep(p.required_time_ms);
    
    // Release resource
    sem_post(res);

    // Track resource busy time for utilization metrics
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
    }
    pthread_mutex_unlock(&wa->resources->log_mutex);

    // Log FINISH event via FIFO
    snprintf(buf, sizeof(buf), "FINISH id=%d name=%s service=%s\n", 
             p.id, p.name, service_name(p.service));
    write(wa->fifo_fd, buf, strlen(buf));

    return NULL;
}
```

#### Thread Creation in UI (src/ui.c):
```c
/* ─────────────────────────────────────────────────────────────────────────────
   📸 SCREENSHOT: Thread creation code from run_scheduler()
   ───────────────────────────────────────────────────────────────────────────── */

// Create threads for each patient
pthread_t *threads = (pthread_t *)calloc(list.count, sizeof(pthread_t));
WorkerArgs *args = (WorkerArgs *)calloc(list.count, sizeof(WorkerArgs));

for (size_t k = 0; k < list.count; ++k) {
    int idx = order[k];
    args[k].patient = list.items[idx];
    args[k].resources = &resources;
    args[k].fifo_fd = fifo_fd;
    
    // Create a new thread for this patient
    pthread_create(&threads[k], NULL, patient_thread, &args[k]);
    ms_sleep(10);  // Small delay between thread creation
}

// Wait for all threads to complete
for (size_t k = 0; k < list.count; ++k) 
    pthread_join(threads[k], NULL);
```

#### 📸 Screenshot Placeholder: Resource Usage Pattern
```
RESOURCE USAGE PATTERN (0 - 1500 ms)
--------------------------------------------------
DOCTORS    [AAA..BBB......CCC...........]
  Patients: Alice Bob Carol

MACHINES   [.....DDD.....EEE............]
  Patients: Dave Eve

ROOMS      [...............FFF...GGG....]
  Patients: Frank Grace

0         750        1500 ms
Legend: X=Doctor  X=Machine  X=Room
```
**[INSERT SCREENSHOT: Resource usage pattern showing parallel thread execution]**

---

### 3.3 Dynamic Memory Allocation

**Concept:** Patient information is stored using dynamic memory allocation, allowing the system to handle any number of patients at runtime.

#### Memory Functions Used:
| Function | Purpose |
|----------|---------|
| `malloc()` | Allocate memory for patient arrays |
| `calloc()` | Allocate and zero-initialize thread arrays |
| `realloc()` | Resize patient list when adding/deleting |
| `free()` | Release memory when done |

#### Code Location: `src/ui.c`

```c
/* ─────────────────────────────────────────────────────────────────────────────
   📸 SCREENSHOT: Dynamic memory allocation for patients
   ───────────────────────────────────────────────────────────────────────────── */

// Patient data structure (include/patient.h)
typedef struct {
    int id;
    char name[64];
    ServiceType service;      // Consultation, Lab Test, or Treatment
    int priority;             // 1-5 (1 = highest priority)
    unsigned required_time_ms;
    unsigned arrival_ms;
} Patient;

// UI State with dynamic patient array
typedef struct {
    Patient *items;           // Dynamically allocated array
    size_t count;             // Current number of patients
    int next_id;              // Auto-increment ID
    // ... other fields
} UiState;

// Adding a new patient (dynamic memory)
static void add_patient(UiState *st, const char *name, ServiceType svc, 
                        int priority, unsigned req_ms, unsigned arr_ms) {
    // Reallocate array to hold one more patient
    st->items = (Patient *)realloc(st->items, sizeof(Patient) * (st->count + 1));
    
    Patient *p = &st->items[st->count];
    p->id = st->next_id++;
    snprintf(p->name, sizeof(p->name), "%s", name);
    p->service = svc;
    p->priority = priority;
    p->required_time_ms = req_ms;
    p->arrival_ms = arr_ms;
    st->count++;
}

// Deleting a patient (memory management)
static int delete_patient(UiState *st, int id) {
    size_t idx;
    if (!find_patient(st, id, &idx)) return -1;
    
    // Shift remaining elements
    for (size_t i = idx + 1; i < st->count; ++i) 
        st->items[i-1] = st->items[i];
    st->count--;
    
    if (st->count == 0) {
        free(st->items);      // Free memory when empty
        st->items = NULL;
    } else {
        // Shrink allocated memory
        st->items = (Patient *)realloc(st->items, sizeof(Patient) * st->count);
    }
    return 0;
}

// Cleanup on exit
static void cleanup_state(UiState *st) {
    if (st->items) {
        free(st->items);      // Free patient array
        st->items = NULL;
    }
    st->count = 0;
}
```

#### 📸 Screenshot Placeholder: Patient Queue Display
```
+------------------------------------------------------------------------------+
|                          PATIENT QUEUE (5 patients)                          |
+------------------------------------------------------------------------------+
ID   Name               Service        Priority   Req(ms)    Arrival(ms)
------------------------------------------------------------------------------
1    Alice              Consultation   2          300        0
2    Bob                Lab Test       3          200        50
3    Carol              Treatment      1          400        100
4    Dave               Consultation   4          150        150
5    Eve                Lab Test       5          250        200
```
**[INSERT SCREENSHOT: Patient queue showing dynamically added patients]**

---

### 3.4 Process Creation

**Concept:** A separate child process is created for logging using `fork()` and `exec()`. This child process records all scheduling events.

#### Process Flow:
```
┌─────────────────┐     fork()      ┌─────────────────┐
│  Main Process   │ ───────────────►│  Logger Process │
│  (Scheduler)    │                 │  (Child)        │
│                 │   FIFO/MQ/SHM   │                 │
│  Sends logs  ───┼────────────────►│  Writes to file │
└─────────────────┘                 └─────────────────┘
```

#### Code Location: `src/ui.c` (Process Creation) and `src/logger.c` (Logger Process)

```c
/* ─────────────────────────────────────────────────────────────────────────────
   📸 SCREENSHOT: Process creation using fork() and exec()
   File: src/ui.c - run_scheduler() function
   ───────────────────────────────────────────────────────────────────────────── */

// Create child process for logger
pid_t pid = fork();

if (pid == 0) {
    // CHILD PROCESS: Execute the logger program
    execl("bin/logger", "logger", (char *)NULL);
    _exit(127);  // Exit if exec fails
}

// PARENT PROCESS continues...
// Wait for FIFO to be ready (logger opens it for reading)
int fifo_fd = -1;
for (int tries = 0; tries < 200 && fifo_fd == -1; ++tries) {
    fifo_fd = open(FIFO_PATH, O_WRONLY | O_NONBLOCK);
    if (fifo_fd == -1 && errno == ENXIO) {
        ms_sleep(10);  // Logger not ready yet
        continue;
    }
}

// ... scheduling happens here ...

// Wait for logger process to finish
int status = 0;
waitpid(pid, &status, 0);
```

#### Logger Process Code:
```c
/* ─────────────────────────────────────────────────────────────────────────────
   📸 SCREENSHOT: Logger process code
   File: src/logger.c
   ───────────────────────────────────────────────────────────────────────────── */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#define FIFO_PATH "/tmp/hospital_log_fifo"

int main(void) {
    // Create logs directory if it doesn't exist
    mkdir("logs", 0755);
    
    // Open log file for writing
    FILE *log = fopen("logs/log.txt", "a");
    if (!log) return 1;
    
    // Open FIFO for reading (blocks until writer connects)
    int fd = open(FIFO_PATH, O_RDONLY);
    if (fd < 0) {
        fclose(log);
        return 1;
    }
    
    // Read messages from FIFO and write to log file
    char buf[512];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        fprintf(log, "%s", buf);
        fflush(log);
    }
    
    close(fd);
    fclose(log);
    return 0;
}
```

#### 📸 Screenshot Placeholder: Log File Output
```
logs/log.txt:
START id=1 name=Alice service=Consultation
START id=2 name=Bob service=LabTest
FINISH id=2 name=Bob service=LabTest
START id=3 name=Carol service=Treatment
FINISH id=1 name=Alice service=Consultation
FINISH id=3 name=Carol service=Treatment
```
**[INSERT SCREENSHOT: Content of logs/log.txt showing process communication]**

---

### 3.5 Inter-Process Communication (IPC)

**Concept:** The scheduler and logger processes communicate using multiple IPC mechanisms to demonstrate OS concepts.

#### IPC Mechanisms Implemented:
| Mechanism | Purpose | Path/Name |
|-----------|---------|-----------|
| Named FIFO (Pipe) | Real-time log messages | `/tmp/hospital_log_fifo` |
| POSIX Message Queue | Notification signals | `/hospital_log_mq` |
| Shared Memory | Statistics sharing | `/hospital_stats` |

#### Code Location: `src/ipc.c`

```c
/* ─────────────────────────────────────────────────────────────────────────────
   📸 SCREENSHOT: IPC implementation
   File: src/ipc.c
   ───────────────────────────────────────────────────────────────────────────── */

#include "ipc.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <mqueue.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

/* ════════════════════════════════════════════════════════════════════════════
   FIFO (Named Pipe) - For real-time log streaming
   ════════════════════════════════════════════════════════════════════════════ */
int ipc_setup_fifo(void) {
    unlink(FIFO_PATH);  // Remove old FIFO if exists
    if (mkfifo(FIFO_PATH, 0666) != 0 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

void ipc_cleanup_fifo(void) {
    unlink(FIFO_PATH);
}

/* ════════════════════════════════════════════════════════════════════════════
   Message Queue - For notification signals
   ════════════════════════════════════════════════════════════════════════════ */
mqd_t ipc_open_mq(int create) {
    struct mq_attr attr = {
        .mq_flags = 0,
        .mq_maxmsg = 10,
        .mq_msgsize = 256,
        .mq_curmsgs = 0
    };
    
    int flags = O_RDWR | O_NONBLOCK;
    if (create) flags |= O_CREAT;
    
    mqd_t mq = mq_open(MQ_NAME, flags, 0666, &attr);
    return mq;
}

void ipc_close_mq(mqd_t mq) {
    mq_close(mq);
    mq_unlink(MQ_NAME);
}

/* ════════════════════════════════════════════════════════════════════════════
   Shared Memory - For statistics sharing between processes
   ════════════════════════════════════════════════════════════════════════════ */
int ipc_setup_shm(int *fd_out, SharedStats **stats_out, int create) {
    int flags = O_RDWR;
    if (create) flags |= O_CREAT;
    
    int fd = shm_open(SHM_NAME, flags, 0666);
    if (fd < 0) return -1;
    
    if (create) {
        if (ftruncate(fd, sizeof(SharedStats)) != 0) {
            close(fd);
            return -1;
        }
    }
    
    SharedStats *stats = (SharedStats *)mmap(NULL, sizeof(SharedStats),
                                              PROT_READ | PROT_WRITE,
                                              MAP_SHARED, fd, 0);
    if (stats == MAP_FAILED) {
        close(fd);
        return -1;
    }
    
    if (create) memset(stats, 0, sizeof(SharedStats));
    
    *fd_out = fd;
    *stats_out = stats;
    return 0;
}

void ipc_cleanup_shm(void) {
    shm_unlink(SHM_NAME);
}
```

#### SharedStats Structure (include/ipc.h):
```c
/* ─────────────────────────────────────────────────────────────────────────────
   📸 SCREENSHOT: Shared memory structure
   File: include/ipc.h
   ───────────────────────────────────────────────────────────────────────────── */

typedef struct {
    double avg_wait_ms;        // Average waiting time
    double avg_turnaround_ms;  // Average turnaround time
    int completed_jobs;        // Number of completed patients
} SharedStats;
```

#### 📸 Screenshot Placeholder: IPC Diagram
```
┌─────────────────────────────────────────────────────────────────┐
│                    IPC COMMUNICATION FLOW                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌──────────────┐                      ┌──────────────┐         │
│  │   SCHEDULER  │                      │    LOGGER    │         │
│  │   (Parent)   │                      │   (Child)    │         │
│  └──────┬───────┘                      └──────┬───────┘         │
│         │                                      │                 │
│         │    ┌─────────────────────┐          │                 │
│         ├───►│   FIFO (Named Pipe) │──────────┤                 │
│         │    │  /tmp/hospital_log  │          │                 │
│         │    └─────────────────────┘          │                 │
│         │                                      │                 │
│         │    ┌─────────────────────┐          │                 │
│         ├───►│   Message Queue     │──────────┤                 │
│         │    │  /hospital_log_mq   │          │                 │
│         │    └─────────────────────┘          │                 │
│         │                                      │                 │
│         │    ┌─────────────────────┐          │                 │
│         └───►│   Shared Memory     │◄─────────┘                 │
│              │  /hospital_stats    │                            │
│              └─────────────────────┘                            │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```
**[INSERT SCREENSHOT: Terminal showing IPC setup messages]**

---

### 3.6 Synchronization

**Concept:** Multiple patient threads compete for limited hospital resources. Semaphores and mutexes prevent race conditions.

#### Synchronization Primitives:
| Primitive | Purpose | Usage |
|-----------|---------|-------|
| Semaphores | Control access to limited resources | Doctors, Machines, Rooms |
| Mutex | Protect critical sections | Log writing, statistics updates |

#### Code Location: `src/resources.c` and `include/resources.h`

```c
/* ─────────────────────────────────────────────────────────────────────────────
   📸 SCREENSHOT: Resource pool with synchronization
   File: include/resources.h
   ───────────────────────────────────────────────────────────────────────────── */

typedef struct {
    sem_t doctors;           // Semaphore for doctor availability
    sem_t machines;          // Semaphore for machine availability
    sem_t rooms;             // Semaphore for room availability
    
    pthread_mutex_t log_mutex;  // Mutex for thread-safe logging
    
    // Resource counts
    int num_doctors;
    int num_machines;
    int num_rooms;
    
    // Utilization tracking
    unsigned long busy_doctors_ms;
    unsigned long busy_machines_ms;
    unsigned long busy_rooms_ms;
} ResourcePool;
```

```c
/* ─────────────────────────────────────────────────────────────────────────────
   📸 SCREENSHOT: Resource initialization with semaphores
   File: src/resources.c
   ───────────────────────────────────────────────────────────────────────────── */

#include "resources.h"
#include <string.h>

int resources_init(ResourcePool *pool, int doctors, int machines, int rooms) {
    memset(pool, 0, sizeof(*pool));
    
    pool->num_doctors = doctors;
    pool->num_machines = machines;
    pool->num_rooms = rooms;
    
    // Initialize semaphores with resource counts
    // sem_init(semaphore, shared_between_processes, initial_value)
    if (sem_init(&pool->doctors, 0, doctors) != 0) return -1;
    if (sem_init(&pool->machines, 0, machines) != 0) return -1;
    if (sem_init(&pool->rooms, 0, rooms) != 0) return -1;
    
    // Initialize mutex for log protection
    pthread_mutex_init(&pool->log_mutex, NULL);
    
    return 0;
}

void resources_destroy(ResourcePool *pool) {
    sem_destroy(&pool->doctors);
    sem_destroy(&pool->machines);
    sem_destroy(&pool->rooms);
    pthread_mutex_destroy(&pool->log_mutex);
}

// Get the appropriate semaphore for a service type
sem_t *resource_for_service(ResourcePool *pool, ServiceType svc) {
    switch (svc) {
        case SERVICE_CONSULTATION: return &pool->doctors;
        case SERVICE_LAB_TEST:     return &pool->machines;
        case SERVICE_TREATMENT:    return &pool->rooms;
        default:                   return &pool->doctors;
    }
}
```

#### Semaphore Usage in Thread Worker:
```c
/* ─────────────────────────────────────────────────────────────────────────────
   📸 SCREENSHOT: Semaphore wait/post in patient thread
   File: src/thread_worker.c
   ───────────────────────────────────────────────────────────────────────────── */

void *patient_thread(void *arg) {
    WorkerArgs *wa = (WorkerArgs *)arg;
    Patient p = wa->patient;
    
    // Get semaphore for required resource
    sem_t *res = resource_for_service(wa->resources, p.service);
    
    // WAIT: Acquire resource (blocks if unavailable)
    // This prevents more patients than available resources
    sem_wait(res);      // ◄── CRITICAL: Blocks if resource busy
    
    // Use the resource (simulate service time)
    ms_sleep(p.required_time_ms);
    
    // POST: Release resource (allows waiting threads to proceed)
    sem_post(res);      // ◄── CRITICAL: Signals resource available
    
    // Update statistics with mutex protection
    pthread_mutex_lock(&wa->resources->log_mutex);    // ◄── Lock
    wa->resources->busy_doctors_ms += p.required_time_ms;
    pthread_mutex_unlock(&wa->resources->log_mutex);  // ◄── Unlock
    
    return NULL;
}
```

#### 📸 Screenshot Placeholder: Resource Utilization
```
RESOURCE UTILIZATION:
  Doctors (2):   85.3%
  Machines (2):  62.1%
  Rooms (2):     45.7%
```
**[INSERT SCREENSHOT: Resource utilization showing semaphore-controlled access]**

---

## 4. Screenshots & Output

### 4.1 Main Menu
```
+------------------------------------------------------------------------------+
|            🏥 HOSPITAL RESOURCE SCHEDULER 🏥                                 |
+------------------------------------------------------------------------------+
|                                                                              |
|    📋 PATIENT MANAGEMENT          ⚙️  SETTINGS                               |
|    ─────────────────────          ──────────                                 |
|    [1] Add Patient                [6] Configure Resources                    |
|    [2] View Patient Queue         [7] Select Algorithm                       |
|    [3] Edit Patient               [8] Set Time Quantum                       |
|    [4] Delete Patient                                                        |
|                                                                              |
|    ▶️  EXECUTION                   📊 ANALYSIS                               |
|    ────────────                   ──────────                                 |
|    [5] Run Scheduler              [x] Compare Algorithms                     |
|    [v] Simulation View            [g] View Gantt Chart                       |
|    [l] View Logs                                                             |
|                                                                              |
|    [h] Help   [s] Save   [o] Load   [q] Quit                                |
+------------------------------------------------------------------------------+
```
**[INSERT SCREENSHOT: Main menu of the application]**

---

### 4.2 Adding Patients
**[INSERT SCREENSHOT: Add patient form with name, service, priority, time]**

---

### 4.3 Patient Queue
**[INSERT SCREENSHOT: List of patients in the queue]**

---

### 4.4 Running Scheduler

#### Performance Metrics:
```
+------------------------------------------------------------------------------+
|                        SCHEDULING RESULTS                                    |
+------------------------------------------------------------------------------+
Algorithm: SJF (Shortest Job First)

PERFORMANCE METRICS:
    Average Waiting Time:       XXX.XX ms
    Average Turnaround Time:    XXX.XX ms
    Total Execution Time:       XXX ms
    Completed Jobs:             X

RESOURCE UTILIZATION:
    Doctors (2):   XX.X%
    Machines (2):  XX.X%
    Rooms (2):     XX.X%

Logs saved to: logs/log.txt
```
**[INSERT SCREENSHOT: Scheduling results with metrics]**

---

### 4.5 Resource Usage Pattern
**[INSERT SCREENSHOT: Resource usage pattern showing DOCTORS, MACHINES, ROOMS]**

---

### 4.6 Gantt Chart
```
GANTT CHART (Timeline: 0 - 1500 ms)
--------------------------------------------------
Alice       ###########
Bob              ######
Carol                   ##########
Dave                              #####
Eve                                    ########

0         750        1500 ms
Legend: # Consultation  # Lab Test  # Treatment
```
**[INSERT SCREENSHOT: Gantt chart visualization]**

---

### 4.7 Algorithm Comparison
**[INSERT SCREENSHOT: Side-by-side comparison of all 4 algorithms]**

---

### 4.8 Log File
**[INSERT SCREENSHOT: Content of logs/log.txt]**

---

## 5. Conclusion

This project successfully demonstrates how Operating System concepts can help in managing real-life problems like hospital resource allocation. 

### OS Concepts Demonstrated:

| Concept | Implementation | Files |
|---------|----------------|-------|
| CPU Scheduling | FCFS, SJF, Priority, Round Robin | scheduler.c |
| Process Management | fork(), exec(), waitpid() | ui.c, logger.c |
| Thread Management | pthread_create(), pthread_join() | thread_worker.c |
| IPC | FIFO, Message Queue, Shared Memory | ipc.c |
| Synchronization | Semaphores, Mutex | resources.c |
| Memory Management | malloc, calloc, realloc, free | ui.c, patient.c |

### Key Achievements:
1. ✅ Simulated patient requests as jobs on a scheduler
2. ✅ Implemented 4 CPU scheduling algorithms with comparison
3. ✅ Used multithreading for parallel patient processing
4. ✅ Demonstrated process creation with fork()/exec()
5. ✅ Implemented 3 IPC mechanisms (FIFO, MQ, SHM)
6. ✅ Used semaphores and mutexes for resource synchronization
7. ✅ Demonstrated dynamic memory allocation

### Future Enhancements:
- GUI interface for real-time visualization
- Statistical analysis with graphs
- Network-based distributed scheduling
- Database integration for patient records

---

## Appendix: How to Compile and Run

```bash
# In Ubuntu terminal:
cd /mnt/c/Users/asifh/Documents/Shared/Semester_project

# Clean and build
make clean && make run-ui

# Or build only
make all

# Run the UI
./bin/hospital_ui
```

---

**End of Report**
