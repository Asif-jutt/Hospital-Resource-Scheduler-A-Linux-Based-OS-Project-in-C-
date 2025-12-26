# Hospital Resource Scheduler
## A Linux-Based Operating System Project in C

```
╔═══════════════════════════════════════════════════════════════════════════════╗
║         HOSPITAL RESOURCE SCHEDULER - OS Semester Project                     ║
║     Asif Hussain (2023-CS-646) | Muhammad Wakeel (2023-CS-601)                ║
╚═══════════════════════════════════════════════════════════════════════════════╝
```

---

## 📋 Table of Contents
1. [Introduction](#introduction)
2. [Features](#features)
3. [OS Concepts Demonstrated](#os-concepts-demonstrated)
4. [Project Structure](#project-structure)
5. [Prerequisites](#prerequisites)
6. [Build Instructions](#build-instructions)
7. [Running the Application](#running-the-application)
8. [Test Cases for Report](#test-cases-for-report)
9. [Screenshots & Usage](#screenshots--usage)
10. [Algorithm Comparison](#algorithm-comparison)

---

## 🏥 Introduction

Hospitals receive many patients simultaneously, and each patient requires different services. Some need doctor consultations, some need lab tests, and others need treatment. However, hospitals have limited resources like doctors, nurses, and medical machines. If resources are not allocated properly, patients face delays.

This project is a **Hospital Resource Scheduler** designed using Operating System concepts:
- Every patient request behaves like a **job/process**
- Each job requires **CPU time, a resource, and execution time**
- The **scheduler** decides which patient request should be served first
- We simulate a real hospital environment using **C on Linux**

---

## ✨ Features

### CPU Scheduling Algorithms
| Algorithm | Type | Description |
|-----------|------|-------------|
| **FCFS** | Non-preemptive | First Come First Serve - ordered by arrival time |
| **SJF** | Non-preemptive | Shortest Job First - ordered by burst time |
| **Priority** | Non-preemptive | Lower priority value = higher priority (1 = highest) |
| **Round Robin** | Preemptive | Time-sliced with configurable quantum |

### Resource Management
| Resource | Service Type | Synchronization |
|----------|-------------|-----------------|
| Doctors | Consultation | Semaphore |
| Machines | Lab Test | Semaphore |
| Rooms | Treatment | Semaphore |

### Interactive Console UI (Ncurses)
- Add, view, update, delete patients
- Configure resources (doctors/machines/rooms)
- Choose scheduling algorithm and quantum
- Run scheduler and view real-time metrics
- View execution logs
- **Gantt Chart visualization**
- **Algorithm comparison report**
- **Export detailed reports**

---

## 🔧 OS Concepts Demonstrated

### 1. CPU Scheduling Algorithms
```c
// scheduler.c - Example of scheduling order computation
int *schedule_order(const PatientList *list, Algorithm alg, unsigned quantum_ms);
ScheduleMetrics compute_metrics(const PatientList *list, const int *order, Algorithm alg, unsigned quantum_ms);
```

### 2. Multithreading (pthreads)
```c
// thread_worker.c - Each patient runs as a separate thread
void *patient_thread(void *arg) {
    // Acquire resource (semaphore wait)
    sem_wait(res);
    // Perform service (sleep for required_time_ms)
    ms_sleep(p.required_time_ms);
    // Release resource (semaphore post)
    sem_post(res);
}
```

### 3. Synchronization
```c
// resources.c - Semaphores for limited resources
sem_init(&rp->doctors, 0, num_doctors);
sem_init(&rp->machines, 0, num_machines);
sem_init(&rp->rooms, 0, num_rooms);
pthread_mutex_init(&rp->log_mutex, NULL);
```

### 4. Inter-Process Communication (IPC)
```c
// ipc.c - Three IPC mechanisms
mkfifo(FIFO_PATH, 0666);           // Named FIFO (Pipe)
mq_open(MQ_NAME, O_CREAT | O_RDWR); // POSIX Message Queue
shm_open(SHM_NAME, O_CREAT | O_RDWR); // POSIX Shared Memory
```

### 5. Process Creation
```c
// main.c - Fork and exec for logger process
pid_t pid = fork();
if (pid == 0) {
    execl("bin/logger", "logger", (char *)NULL);
    _exit(127);
}
```

### 6. Dynamic Memory Allocation
```c
// patient.c - Dynamic allocation for patient data
list.items = (Patient *)calloc(n, sizeof(Patient));
// ui.c - Resize patient list
st->items = (Patient *)realloc(st->items, sizeof(Patient) * (st->count + 1));
```

---

## 📁 Project Structure

```
Semester_project/
├── bin/                    # Compiled binaries
│   ├── hospital_scheduler  # CLI scheduler
│   ├── hospital_ui         # Interactive UI
│   └── logger              # Logger process
├── data/                   # Data files
│   ├── patients.csv        # Saved patient data
│   ├── report.txt          # Generated reports
│   └── test_case_*.csv     # Test case files
├── include/                # Header files
│   ├── common.h            # Common definitions
│   ├── ipc.h               # IPC declarations
│   ├── patient.h           # Patient structure
│   ├── resources.h         # Resource pool
│   ├── scheduler.h         # Scheduling algorithms
│   ├── storage.h           # CSV file I/O
│   └── thread_worker.h     # Thread worker
├── logs/                   # Log output
│   └── log.txt             # Execution logs
├── src/                    # Source files
│   ├── ipc.c               # IPC implementation
│   ├── logger.c            # Logger process
│   ├── main.c              # CLI main
│   ├── patient.c           # Patient functions
│   ├── resources.c         # Resource management
│   ├── scheduler.c         # Scheduling algorithms
│   ├── storage.c           # CSV I/O
│   ├── thread_worker.c     # Thread worker
│   └── ui.c                # Ncurses UI
├── Makefile                # Build configuration
└── README.md               # This file
```

---

## 📦 Prerequisites (Ubuntu VM)

Install the required packages:
```bash
sudo apt-get update
sudo apt-get install -y build-essential libncurses-dev
```

---

## 🔨 Build Instructions

### In your VirtualBox Ubuntu Terminal:

```bash
# Navigate to project directory
cd ~/Semester_project

# Clean previous build
make clean

# Build all targets
make

# Or build and run UI directly
make run-ui
```

---

## 🚀 Running the Application

### Option 1: Interactive UI (Recommended)
```bash
cd ~/Semester_project
make run-ui
```

### Option 2: Command Line Interface
```bash
cd ~/Semester_project
bin/hospital_scheduler --alg fcfs --patients 10 --doctors 3 --machines 2 --rooms 4 --quantum 3
```

#### CLI Options:
| Option | Description | Default |
|--------|-------------|---------|
| `--alg` | Algorithm: fcfs, sjf, priority, rr | fcfs |
| `--patients` | Number of random patients | 10 |
| `--doctors` | Number of doctors | 3 |
| `--machines` | Number of machines | 2 |
| `--rooms` | Number of rooms | 4 |
| `--quantum` | Round Robin quantum (ms) | 3 |

---

## 📊 Test Cases for Report

We provide 5 pre-configured test case files in `data/` directory:

### Test Case 1: Equal Burst Times (FCFS Test)
```bash
# Load in UI: Press 'l' and enter: data/test_case_1_equal.csv
# 5 patients with identical burst times (200ms each)
# Shows that FCFS = arrival order when all jobs are equal
```

### Test Case 2: SJF Advantage
```bash
# Load: data/test_case_2_sjf.csv
# 8 patients with varying burst times (50-800ms)
# Demonstrates SJF minimizes average waiting time
```

### Test Case 3: Priority Scheduling
```bash
# Load: data/test_case_3_priority.csv
# 10 patients with priorities 1-5
# Shows high-priority patients (1,2) are served first
```

### Test Case 4: Round Robin Fairness
```bash
# Load: data/test_case_4_rr.csv
# 8 patients with short (10-20ms) and long (90-120ms) jobs
# Demonstrates RR gives fair time slices to all
```

### Test Case 5: Comprehensive Scenario
```bash
# Load: data/test_case_5_comprehensive.csv
# 15 patients with Pakistani names, mixed services, priorities
# Perfect for generating final comparison report
```

### Quick Test Workflow:
```bash
cd ~/Semester_project
make run-ui

# In the UI:
# 1. Press 'l' to load CSV → Enter: data/test_case_5_comprehensive.csv
# 2. Press '2' to view patients
# 3. Press 'x' to compare all algorithms
# 4. Press 'r' to generate detailed report (saved to data/report.txt)
# 5. Press '5' to run scheduler with selected algorithm
# 6. Press 'v' to view Gantt chart
# 7. Press '6' to view execution logs
```

---

## 🖥️ Screenshots & Usage

### Main Menu
```
+------------------------------------------------------------------------------+
|         HOSPITAL RESOURCE SCHEDULER - OS Semester Project                    |
|     Asif Hussain (2023-CS-646) | Muhammad Wakeel (2023-CS-601)               |
+------------------------------------------------------------------------------+

[1] Add Patient       [2] View Patients    [3] Update Patient
[4] Delete Patient    [5] Run Scheduler    [6] View Logs
[7] Set Resources     [8] Set Algorithm    [9] Set Quantum
[g] Generate N Pts    [s] Save CSV         [l] Load CSV
[c] Clear List        [x] Compare Algs     [v] View Gantt Chart
[r] Generate Report   [h] Help             [q] Exit

Resources: Doctors=3  Machines=2  Rooms=4
Algorithm: FCFS          Quantum: 3 ms  Patients: 15
```

### Algorithm Comparison View (Press 'x')
```
+------------------------------------------------------------------------------+
|                    ALGORITHM COMPARISON REPORT                                |
+------------------------------------------------------------------------------+

Patients: 15 | RR Quantum: 3 ms | Doctors: 3 | Machines: 2 | Rooms: 4

Algorithm        Avg Wait (ms)       Avg Turnaround (ms)   Winner
--------------------------------------------------------------------------------
FCFS             1245.67             1495.67              
SJF              856.33              1106.33              Wait
Priority         1023.45             1273.45              
Round Robin      1312.89             1562.89              

ANALYSIS:
  Best for Waiting Time:    SJF (856.33 ms)
  Best for Turnaround Time: SJF (1106.33 ms)
```

### Gantt Chart View (Press 'v')
```
+------------------------------------------------------------------------------+
|                   SCHEDULING VISUALIZATION: FCFS                              |
+------------------------------------------------------------------------------+

GANTT CHART (Timeline: 0 - 3580 ms)
----------------------------------------------------------------------
Ahmad_Khan   ################
Sara_Ali                     ########
Bilal_Ahmed                          ####################
...

Legend: # Consultation  # Lab Test  # Treatment
```

---

## 📈 Algorithm Comparison

### When to Use Each Algorithm:

| Algorithm | Best When | Pros | Cons |
|-----------|-----------|------|------|
| **FCFS** | Jobs arrive in order of importance | Simple, fair | Convoy effect |
| **SJF** | Job lengths are known | Minimum avg wait time | Starvation of long jobs |
| **Priority** | Some patients are more urgent | Critical cases first | Low priority starvation |
| **Round Robin** | Interactive/fairness needed | Fair time distribution | Higher overhead |

### Sample Results (Test Case 5):

| Algorithm | Avg Wait (ms) | Avg Turnaround (ms) |
|-----------|---------------|---------------------|
| FCFS | ~1200 | ~1450 |
| SJF | ~850 | ~1100 |
| Priority | ~1000 | ~1250 |
| RR (q=3) | ~1300 | ~1550 |

---

## 📝 Generated Report

Press 'r' in the UI to generate a detailed report saved to `data/report.txt`:

```
================================================================================
                    HOSPITAL RESOURCE SCHEDULER - DETAILED REPORT
================================================================================

GROUP MEMBERS:
  Asif Hussain     (2023-CS-646) - CPU Scheduling & Threads
  Muhammad Wakeel  (2023-CS-601) - Process Creation, IPC & Synchronization

CONFIGURATION:
  Total Patients: 15
  Doctors: 3 | Machines: 2 | Rooms: 4
  RR Quantum: 3 ms

PATIENT LIST:
--------------------------------------------------------------------------------
ID   Name                 Service        Priority   Req(ms)     Arrival(ms)
--------------------------------------------------------------------------------
1    Ahmad_Khan           Consultation   2          250         0
2    Sara_Ali             Lab Test       4          180         20
...

================================================================================
                           ALGORITHM COMPARISON
================================================================================

Algorithm        Avg Wait (ms)       Avg Turnaround (ms)
--------------------------------------------------------------------------------
FCFS             1245.67             1495.67
SJF              856.33              1106.33
Priority         1023.45             1273.45
Round Robin      1312.89             1562.89

ANALYSIS:
  Best for Waiting Time:    SJF (856.33 ms)
  Best for Turnaround Time: SJF (1106.33 ms)

================================================================================
                              OS CONCEPTS USED
================================================================================

1. CPU SCHEDULING ALGORITHMS:
   - FCFS: First Come First Serve (non-preemptive)
   - SJF: Shortest Job First (non-preemptive)
   - Priority: Jobs with higher priority run first
   - Round Robin: Time-sliced preemptive scheduling

2. MULTITHREADING (pthreads):
   - Each patient request runs as a separate thread
   - Parallel execution for concurrent patient processing

3. SYNCHRONIZATION:
   - Semaphores: Control access to limited resources
   - Mutex: Protect shared data structures and logging

4. INTER-PROCESS COMMUNICATION (IPC):
   - Named FIFO (Pipe): Transfer log messages
   - Message Queue: Notify logger of stats
   - Shared Memory: Share performance metrics

5. PROCESS CREATION:
   - fork(): Create child process for logger
   - exec(): Replace child process with logger program

6. DYNAMIC MEMORY ALLOCATION:
   - malloc/calloc: Allocate patient data
   - realloc: Resize patient queue
   - free: Release memory when done

================================================================================
                              END OF REPORT
================================================================================
```

---

## 🔗 IPC Paths

| Mechanism | Path/Name | Purpose |
|-----------|-----------|---------|
| FIFO | `/tmp/hospital_log_fifo` | Stream log messages |
| Message Queue | `/hospital_log_mq` | Event notifications |
| Shared Memory | `/hospital_stats` | Share final stats |

---

## 👥 Group Members

| Name | Roll No | Responsibilities |
|------|---------|------------------|
| **Asif Hussain** | 2023-CS-646 | CPU Scheduling & Threads |
| **Muhammad Wakeel** | 2023-CS-601 | Process Creation, IPC & Synchronization |

---

## 📄 License

This project is developed for academic purposes as part of the Operating Systems course.

---

## 🎯 Conclusion

This project demonstrates how Operating System concepts can help manage real-life problems like hospital resource allocation. It covers:
- ✅ CPU Scheduling (FCFS, SJF, Priority, RR)
- ✅ Processes (fork/exec)
- ✅ Threads (pthreads)
- ✅ IPC (Pipes, Message Queues, Shared Memory)
- ✅ Dynamic Memory
- ✅ Synchronization (Mutex, Semaphores)

The modular design allows for future extensions like a GUI, statistical analysis, and additional resource types.
"# Hospital-Resource-Scheduler-A-Linux-Based-OS-Project-in-C-" 
