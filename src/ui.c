// Enable POSIX/GNU features (must be before any includes)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <time.h>
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>

#include "common.h"
#include "patient.h"
#include "scheduler.h"
#include "resources.h"
#include "thread_worker.h"
#include "ipc.h"
#include "storage.h"

// ─────────────────────────────────────────────────────────────────────────────
// UI State
// ─────────────────────────────────────────────────────────────────────────────
typedef struct {
    Patient *items;
    size_t count;
    int next_id;

    int doctors;
    int machines;
    int rooms;

    Algorithm alg;
    unsigned quantum_ms;
} UiState;

typedef struct {
    int idx;               // patient index in list
    unsigned start_ms;
    unsigned end_ms;
} Slice;

// ─────────────────────────────────────────────────────────────────────────────
// Helper Functions
// ─────────────────────────────────────────────────────────────────────────────
static const char *service_name(ServiceType s) {
    switch (s) {
        case SERVICE_CONSULTATION: return "Consultation";
        case SERVICE_LAB_TEST:     return "Lab Test";
        case SERVICE_TREATMENT:    return "Treatment";
        default:                   return "Unknown";
    }
}

static void ui_init(UiState *st) {
    memset(st, 0, sizeof(*st));
    st->items = NULL;
    st->count = 0;
    st->next_id = 1;
    st->doctors = 3;
    st->machines = 2;
    st->rooms = 4;
    st->alg = ALG_FCFS;
    st->quantum_ms = 3;
}

static void ui_init_colors(void) {
    if (!has_colors()) return;
    start_color();
    use_default_colors();
    init_pair(1, COLOR_CYAN, -1);     // Title/headers
    init_pair(2, COLOR_YELLOW, -1);   // Status bar
    init_pair(3, COLOR_GREEN, -1);    // FCFS/SJF
    init_pair(4, COLOR_MAGENTA, -1);  // Priority
    init_pair(5, COLOR_BLUE, -1);     // RR
    init_pair(6, COLOR_GREEN, -1);    // Consultation bar
    init_pair(7, COLOR_YELLOW, -1);   // Lab Test bar
    init_pair(8, COLOR_RED, -1);      // Treatment bar
    init_pair(9, COLOR_WHITE, COLOR_BLUE);  // Highlight
    init_pair(10, COLOR_BLACK, COLOR_GREEN); // Success
    init_pair(11, COLOR_WHITE, COLOR_RED);   // Error
}

// ─────────────────────────────────────────────────────────────────────────────
// Patient Management
// ─────────────────────────────────────────────────────────────────────────────
static void add_patient(UiState *st, const char *name, int priority, ServiceType svc, unsigned req_ms, unsigned arr_ms) {
    st->items = (Patient *)realloc(st->items, sizeof(Patient) * (st->count + 1));
    Patient *p = &st->items[st->count];
    p->id = st->next_id++;
    snprintf(p->name, MAX_NAME_LEN, "%s", name);
    p->priority = priority;
    p->service = svc;
    p->required_time_ms = req_ms;
    p->arrival_ms = arr_ms;
    st->count++;
}

static Patient *find_patient(UiState *st, int id, size_t *idx_out) {
    for (size_t i = 0; i < st->count; ++i) {
        if (st->items[i].id == id) {
            if (idx_out) *idx_out = i;
            return &st->items[i];
        }
    }
    return NULL;
}

static int delete_patient(UiState *st, int id) {
    size_t idx;
    if (!find_patient(st, id, &idx)) return -1;
    for (size_t i = idx + 1; i < st->count; ++i) st->items[i-1] = st->items[i];
    st->count--;
    if (st->count == 0) {
        free(st->items); st->items = NULL;
    } else {
        st->items = (Patient *)realloc(st->items, sizeof(Patient) * st->count);
    }
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Input Prompts
// ─────────────────────────────────────────────────────────────────────────────
static int prompt_int(const char *label, int def) {
    echo();
    curs_set(1);
    char buf[64];
    mvprintw(LINES-3, 2, "%s [%d]: ", label, def);
    clrtoeol();
    getnstr(buf, sizeof(buf)-1);
    noecho();
    curs_set(0);
    if (strlen(buf) == 0) return def;
    return atoi(buf);
}

static unsigned prompt_uint(const char *label, unsigned def) {
    int v = prompt_int(label, (int)def);
    return v <= 0 ? def : (unsigned)v;
}

static void prompt_str(const char *label, char *out, size_t outsz, const char *def) {
    echo();
    curs_set(1);
    mvprintw(LINES-3, 2, "%s [%s]: ", label, def ? def : "");
    clrtoeol();
    getnstr(out, (int)outsz-1);
    noecho();
    curs_set(0);
    if (strlen(out) == 0 && def) snprintf(out, outsz, "%s", def);
}

static ServiceType prompt_service(ServiceType def) {
    int cur = (int)def;
    const char *opts[] = {"Consultation (Doctor)", "Lab Test (Machine)", "Treatment (Room)"};
    int choice = cur;
    while (1) {
        clear();
        if (has_colors()) attron(COLOR_PAIR(1) | A_BOLD);
        mvprintw(1, 2, "+------------------------------------------+");
        mvprintw(2, 2, "|        SELECT SERVICE TYPE               |");
        mvprintw(3, 2, "+------------------------------------------+");
        if (has_colors()) attroff(COLOR_PAIR(1) | A_BOLD);
        
        for (int i = 0; i < 3; ++i) {
            if (i == choice) attron(A_REVERSE | A_BOLD);
            mvprintw(5 + i, 4, " %d. %s ", i+1, opts[i]);
            if (i == choice) attroff(A_REVERSE | A_BOLD);
        }
        mvprintw(LINES-2, 2, "Up/Down: Navigate | Enter: Select | q: Cancel");
        int ch = getch();
        if (ch == KEY_UP) { if (choice > 0) choice--; }
        else if (ch == KEY_DOWN) { if (choice < 2) choice++; }
        else if (ch == '\n') break;
        else if (ch == 'q') return def;
    }
    return (ServiceType)choice;
}

static Algorithm prompt_alg(Algorithm def) {
    const char *opts[] = {"FCFS (First Come First Serve)", "SJF (Shortest Job First)", 
                          "Priority Scheduling", "Round Robin (Preemptive)"};
    int choice = (int)def;
    while (1) {
        clear();
        if (has_colors()) attron(COLOR_PAIR(1) | A_BOLD);
        mvprintw(1, 2, "+----------------------------------------------+");
        mvprintw(2, 2, "|        SELECT SCHEDULING ALGORITHM           |");
        mvprintw(3, 2, "+----------------------------------------------+");
        if (has_colors()) attroff(COLOR_PAIR(1) | A_BOLD);
        
        for (int i = 0; i < 4; ++i) {
            if (i == choice) attron(A_REVERSE | A_BOLD);
            mvprintw(5 + i, 4, " %d. %s ", i+1, opts[i]);
            if (i == choice) attroff(A_REVERSE | A_BOLD);
        }
        mvprintw(LINES-2, 2, "Up/Down: Navigate | Enter: Select | q: Cancel");
        int ch = getch();
        if (ch == KEY_UP) { if (choice > 0) choice--; }
        else if (ch == KEY_DOWN) { if (choice < 3) choice++; }
        else if (ch == '\n') break;
        else if (ch == 'q') return def;
    }
    return (Algorithm)choice;
}

// ─────────────────────────────────────────────────────────────────────────────
// Resource Usage Pattern Display
// ─────────────────────────────────────────────────────────────────────────────
static void draw_resource_usage_pattern(const PatientList *list, const Slice *slices, size_t count, int start_row) {
    if (count == 0 || list->count == 0) {
        mvprintw(start_row, 2, "No resource usage to display.");
        return;
    }
    
    unsigned max_end = 0;
    for (size_t i = 0; i < count; ++i)
        if (slices[i].end_ms > max_end) max_end = slices[i].end_ms;
    
    int left = 18, right = COLS - 4;
    int width = right - left;
    if (width < 20) width = 20;
    
    // Resource labels
    const char *res_names[3] = {"DOCTORS", "MACHINES", "ROOMS"};
    int color_pairs[3] = {6, 7, 8}; // Consultation=green, Lab=yellow, Treatment=red
    
    if (has_colors()) attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(start_row, 2, "RESOURCE USAGE PATTERN (0 - %u ms)", max_end);
    if (has_colors()) attroff(COLOR_PAIR(1) | A_BOLD);
    mvhline(start_row + 1, 2, '-', COLS - 4);
    
    int base_row = start_row + 2;
    
    // Draw each resource row
    for (int res = 0; res < 3; ++res) {
        ServiceType target_service = (ServiceType)res;
        int row = base_row + res * 2;
        
        if (has_colors()) attron(COLOR_PAIR(2) | A_BOLD);
        mvprintw(row, 2, "%-10s", res_names[res]);
        if (has_colors()) attroff(COLOR_PAIR(2) | A_BOLD);
        
        // Draw timeline axis
        mvaddch(row, left - 1, '[');
        for (int c = left; c < right; ++c) mvaddch(row, c, '.');
        mvaddch(row, right, ']');
        
        // Draw patient blocks for this resource type
        for (size_t si = 0; si < count; ++si) {
            int idx = slices[si].idx;
            if (idx < 0 || (size_t)idx >= list->count) continue;
            Patient p = list->items[idx];
            if (p.service != target_service) continue;
            
            int col_start = left + (int)((double)slices[si].start_ms / max_end * width);
            int col_end = left + (int)((double)slices[si].end_ms / max_end * width);
            if (col_end <= col_start) col_end = col_start + 1;
            
            if (has_colors()) attron(COLOR_PAIR(color_pairs[res]) | A_BOLD);
            for (int c = col_start; c < col_end && c < right; ++c) {
                char ch = p.name[0];  // First letter of patient name
                mvaddch(row, c, ch);
            }
            if (has_colors()) attroff(COLOR_PAIR(color_pairs[res]) | A_BOLD);
        }
        
        // Patient names using this resource
        int name_row = row + 1;
        mvprintw(name_row, 4, "Patients: ");
        int col = 14;
        for (size_t i = 0; i < list->count; ++i) {
            if (list->items[i].service == target_service) {
                if (col + (int)strlen(list->items[i].name) + 3 >= COLS - 4) {
                    mvprintw(name_row, col, "...");
                    break;
                }
                if (has_colors()) attron(COLOR_PAIR(color_pairs[res]));
                mvprintw(name_row, col, "%s ", list->items[i].name);
                if (has_colors()) attroff(COLOR_PAIR(color_pairs[res]));
                col += strlen(list->items[i].name) + 1;
            }
        }
    }
    
    // Time axis
    int axis_row = base_row + 6;
    mvprintw(axis_row, left, "0");
    mvprintw(axis_row, left + width/2 - 2, "%u", max_end/2);
    mvprintw(axis_row, left + width - 6, "%u ms", max_end);
    
    // Legend
    int legend_row = axis_row + 1;
    mvprintw(legend_row, 2, "Legend: ");
    if (has_colors()) attron(COLOR_PAIR(6) | A_BOLD);
    mvprintw(legend_row, 10, "X");
    if (has_colors()) attroff(COLOR_PAIR(6) | A_BOLD);
    mvprintw(legend_row, 11, "=Doctor  ");
    if (has_colors()) attron(COLOR_PAIR(7) | A_BOLD);
    mvprintw(legend_row, 21, "X");
    if (has_colors()) attroff(COLOR_PAIR(7) | A_BOLD);
    mvprintw(legend_row, 22, "=Machine  ");
    if (has_colors()) attron(COLOR_PAIR(8) | A_BOLD);
    mvprintw(legend_row, 33, "X");
    if (has_colors()) attroff(COLOR_PAIR(8) | A_BOLD);
    mvprintw(legend_row, 34, "=Room  (X=first letter of patient name)");
}

// ─────────────────────────────────────────────────────────────────────────────
// Gantt Chart / Timeline Building
// ─────────────────────────────────────────────────────────────────────────────
static const PatientList *g_ui_cmp_ctx = NULL;

static int cmp_arrival(const void *a, const void *b) {
    int ia = *(const int *)a, ib = *(const int *)b;
    unsigned aa = g_ui_cmp_ctx->items[ia].arrival_ms;
    unsigned ab = g_ui_cmp_ctx->items[ib].arrival_ms;
    if (aa < ab) return -1;
    if (aa > ab) return 1;
    return ia - ib;
}

static Slice *build_timeline(const PatientList *list, Algorithm alg, unsigned quantum_ms, size_t *out_count) {
    size_t n = list->count;
    if (n == 0) { *out_count = 0; return NULL; }
    int *order = schedule_order(list, alg, quantum_ms);
    size_t cap = alg == ALG_RR ? (size_t)(10 * n) : n;
    Slice *slices = (Slice *)malloc(sizeof(Slice) * cap);
    size_t count = 0;

    if (alg == ALG_RR) {
        unsigned *remaining = (unsigned *)malloc(sizeof(unsigned) * n);
        unsigned *arrival = (unsigned *)malloc(sizeof(unsigned) * n);
        for (size_t i = 0; i < n; ++i) { 
            remaining[i] = list->items[i].required_time_ms; 
            arrival[i] = list->items[i].arrival_ms; 
        }
        int *arrival_order = (int *)malloc(sizeof(int) * n);
        for (size_t i = 0; i < n; ++i) arrival_order[i] = order[i];
        g_ui_cmp_ctx = list;
        qsort(arrival_order, n, sizeof(int), cmp_arrival);
        g_ui_cmp_ctx = NULL;

        int *queue = (int *)malloc(sizeof(int) * n);
        size_t head = 0, tail = 0, qcount = 0, completed = 0, next_arrival = 0;
        unsigned time = 0;
        if (next_arrival < n) time = arrival[arrival_order[next_arrival]];

        while (completed < n && count < cap) {
            while (next_arrival < n) {
                int pid = arrival_order[next_arrival];
                if (arrival[pid] <= time) { 
                    queue[tail] = pid; 
                    tail = (tail + 1) % n; 
                    qcount++; 
                    next_arrival++; 
                } else break;
            }
            if (qcount == 0) { 
                if (next_arrival < n) { 
                    time = arrival[arrival_order[next_arrival]]; 
                    continue; 
                } else break; 
            }
            int pid = queue[head]; 
            head = (head + 1) % n; 
            qcount--;
            if (remaining[pid] == 0) continue;
            unsigned slice = remaining[pid] > quantum_ms ? quantum_ms : remaining[pid];
            unsigned start = time; 
            time += slice; 
            remaining[pid] -= slice;
            slices[count++] = (Slice){ .idx = pid, .start_ms = start, .end_ms = time };
            while (next_arrival < n) {
                int np = arrival_order[next_arrival];
                if (arrival[np] <= time) { 
                    queue[tail] = np; 
                    tail = (tail + 1) % n; 
                    qcount++; 
                    next_arrival++; 
                } else break;
            }
            if (remaining[pid] > 0) { 
                queue[tail] = pid; 
                tail = (tail + 1) % n; 
                qcount++; 
            } else { 
                completed++; 
            }
        }
        free(remaining); free(arrival); free(arrival_order); free(queue);
    } else {
        unsigned time = 0;
        for (size_t k = 0; k < n && count < cap; ++k) {
            int i = order[k]; 
            Patient p = list->items[i];
            if (p.arrival_ms > time) time = p.arrival_ms;
            unsigned start = time; 
            unsigned end = start + p.required_time_ms; 
            time = end;
            slices[count++] = (Slice){ .idx = i, .start_ms = start, .end_ms = end };
        }
    }
    free(order);
    *out_count = count;
    return slices;
}

static void draw_timeline(const PatientList *list, const Slice *slices, size_t count, int start_row) {
    if (count == 0) { 
        mvprintw(start_row, 2, "No timeline to display."); 
        return; 
    }
    unsigned max_end = 0; 
    for (size_t i = 0; i < count; ++i) 
        if (slices[i].end_ms > max_end) max_end = slices[i].end_ms;
    
    int left = 16, right = COLS - 4; 
    int width = right - left;
    if (width < 10) width = 10;
    
    if (has_colors()) attron(COLOR_PAIR(1));
    mvprintw(start_row, 2, "GANTT CHART (Timeline: 0 - %u ms)", max_end);
    if (has_colors()) attroff(COLOR_PAIR(1));
    mvhline(start_row + 1, 2, '-', COLS-4);
    
    int base_row = start_row + 2;
    for (size_t pi = 0; pi < list->count && (base_row + (int)pi) < LINES - 5; ++pi) {
        Patient p = list->items[pi];
        mvprintw(base_row + (int)pi, 2, "%-12s", p.name);
        for (size_t si = 0; si < count; ++si) {
            if (slices[si].idx != (int)pi) continue;
            int col_start = left + (int)((double)slices[si].start_ms / max_end * width);
            int col_end = left + (int)((double)slices[si].end_ms / max_end * width);
            if (col_end <= col_start) col_end = col_start + 1;
            int color_pair = 6;
            if (p.service == SERVICE_LAB_TEST) color_pair = 7;
            else if (p.service == SERVICE_TREATMENT) color_pair = 8;
            if (has_colors()) attron(COLOR_PAIR(color_pair));
            for (int c = col_start; c < col_end && c < right; ++c) 
                mvaddch(base_row + (int)pi, c, '#');
            if (has_colors()) attroff(COLOR_PAIR(color_pair));
        }
    }
    int axis_row = base_row + (int)list->count + 1;
    mvprintw(axis_row, left, "0");
    mvprintw(axis_row, left + width/2 - 2, "%u", max_end/2);
    mvprintw(axis_row, left + width - 4, "%u ms", max_end);
    
    // Legend
    mvprintw(axis_row + 1, 2, "Legend: ");
    if (has_colors()) attron(COLOR_PAIR(6));
    mvaddch(axis_row + 1, 10, '#');
    if (has_colors()) attroff(COLOR_PAIR(6));
    mvprintw(axis_row + 1, 11, " Consultation  ");
    if (has_colors()) attron(COLOR_PAIR(7));
    mvaddch(axis_row + 1, 27, '#');
    if (has_colors()) attroff(COLOR_PAIR(7));
    mvprintw(axis_row + 1, 28, " Lab Test  ");
    if (has_colors()) attron(COLOR_PAIR(8));
    mvaddch(axis_row + 1, 40, '#');
    if (has_colors()) attroff(COLOR_PAIR(8));
    mvprintw(axis_row + 1, 41, " Treatment");
}

// ─────────────────────────────────────────────────────────────────────────────
// Views
// ─────────────────────────────────────────────────────────────────────────────
static void view_patients(UiState *st) {
    clear();
    if (has_colors()) attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(1, 2, "+------------------------------------------------------------------------------+");
    mvprintw(2, 2, "|                          PATIENT QUEUE (%3zu patients)                        |", st->count);
    mvprintw(3, 2, "+------------------------------------------------------------------------------+");
    if (has_colors()) attroff(COLOR_PAIR(1) | A_BOLD);
    
    if (has_colors()) attron(COLOR_PAIR(2) | A_BOLD);
    mvprintw(5, 2, "%-4s %-18s %-14s %-10s %-10s %-12s", 
             "ID", "Name", "Service", "Priority", "Req(ms)", "Arrival(ms)");
    if (has_colors()) attroff(COLOR_PAIR(2) | A_BOLD);
    mvhline(6, 2, '-', COLS-4);
    
    int row = 7;
    for (size_t i = 0; i < st->count && row < LINES-3; ++i) {
        Patient *p = &st->items[i];
        int color = 3;
        if (p->priority <= 2) color = 8; // High priority - red
        else if (p->priority == 3) color = 7; // Medium - yellow
        
        if (has_colors()) attron(COLOR_PAIR(color));
        mvprintw(row++, 2, "%-4d %-18s %-14s %-10d %-10u %-12u",
                 p->id, p->name, service_name(p->service), p->priority,
                 p->required_time_ms, p->arrival_ms);
        if (has_colors()) attroff(COLOR_PAIR(color));
    }
    
    if (st->count == 0) {
        mvprintw(row, 2, "(No patients in queue)");
    }
    
    mvprintw(LINES-2, 2, "Press any key to return...");
    getch();
}

static void view_logs(void) {
    clear();
    if (has_colors()) attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(1, 2, "+------------------------------------------------------------------------------+");
    mvprintw(2, 2, "|                            EXECUTION LOGS                                    |");
    mvprintw(3, 2, "+------------------------------------------------------------------------------+");
    if (has_colors()) attroff(COLOR_PAIR(1) | A_BOLD);
    
    FILE *f = fopen("logs/log.txt", "r");
    if (!f) {
        mvprintw(5, 2, "No log file found yet. Run the scheduler first.");
        mvprintw(LINES-2, 2, "Press any key to return...");
        getch();
        return;
    }
    char line[256];
    int row = 5;
    while (fgets(line, sizeof(line), f) && row < LINES-3) {
        // Remove newline
        line[strcspn(line, "\n")] = '\0';
        mvprintw(row++, 2, "%.76s", line);
    }
    fclose(f);
    mvprintw(LINES-2, 2, "Press any key to return...");
    getch();
}

static void show_help(void) {
    clear();
    if (has_colors()) attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(1, 2, "+------------------------------------------------------------------------------+");
    mvprintw(2, 2, "|                    HOSPITAL RESOURCE SCHEDULER - HELP                        |");
    mvprintw(3, 2, "+------------------------------------------------------------------------------+");
    if (has_colors()) attroff(COLOR_PAIR(1) | A_BOLD);
    
    int row = 5;
    if (has_colors()) attron(COLOR_PAIR(2));
    mvprintw(row++, 2, "PROJECT OVERVIEW:");
    if (has_colors()) attroff(COLOR_PAIR(2));
    mvprintw(row++, 4, "This system simulates hospital resource scheduling using OS concepts.");
    mvprintw(row++, 4, "Each patient request is treated as a job/process requiring resources.");
    row++;
    
    if (has_colors()) attron(COLOR_PAIR(2));
    mvprintw(row++, 2, "SCHEDULING ALGORITHMS:");
    if (has_colors()) attroff(COLOR_PAIR(2));
    mvprintw(row++, 4, "1. FCFS  - First Come First Serve (non-preemptive, by arrival time)");
    mvprintw(row++, 4, "2. SJF   - Shortest Job First (non-preemptive, by burst time)");
    mvprintw(row++, 4, "3. Priority - By priority value (1=highest, 5=lowest)");
    mvprintw(row++, 4, "4. Round Robin - Preemptive with time quantum");
    row++;
    
    if (has_colors()) attron(COLOR_PAIR(2));
    mvprintw(row++, 2, "RESOURCES:");
    if (has_colors()) attroff(COLOR_PAIR(2));
    mvprintw(row++, 4, "Doctors   - Handle Consultation requests");
    mvprintw(row++, 4, "Machines  - Handle Lab Test requests");
    mvprintw(row++, 4, "Rooms     - Handle Treatment requests");
    row++;
    
    if (has_colors()) attron(COLOR_PAIR(2));
    mvprintw(row++, 2, "OS CONCEPTS DEMONSTRATED:");
    if (has_colors()) attroff(COLOR_PAIR(2));
    mvprintw(row++, 4, "* Multithreading (pthreads) - Parallel patient processing");
    mvprintw(row++, 4, "* Synchronization (Mutex/Semaphores) - Resource protection");
    mvprintw(row++, 4, "* IPC (Pipes, Message Queues, Shared Memory) - Logger communication");
    mvprintw(row++, 4, "* Process Creation (fork/exec) - Logger process");
    mvprintw(row++, 4, "* Dynamic Memory (malloc/calloc/free) - Patient data");
    
    mvprintw(LINES-2, 2, "Press any key to return...");
    getch();
}

static void view_gantt(UiState *st) {
    if (st->count == 0) {
        clear();
        mvprintw(3, 2, "No patients to visualize. Add patients first.");
        mvprintw(LINES-2, 2, "Press any key to return...");
        getch();
        return;
    }
    
    Algorithm a = prompt_alg(st->alg);
    PatientList list; 
    list.items = st->items; 
    list.count = st->count;
    size_t sc = 0; 
    Slice *sl = build_timeline(&list, a, st->quantum_ms, &sc);
    
    clear();
    if (has_colors()) attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(1, 2, "+------------------------------------------------------------------------------+");
    mvprintw(2, 2, "|                   SCHEDULING VISUALIZATION: %-12s                      |", alg_name(a));
    mvprintw(3, 2, "+------------------------------------------------------------------------------+");
    if (has_colors()) attroff(COLOR_PAIR(1) | A_BOLD);
    
    draw_timeline(&list, sl, sc, 5);
    mvprintw(LINES-2, 2, "Press any key to return...");
    getch();
    free(sl);
}

// ─────────────────────────────────────────────────────────────────────────────
// Run Scheduler with IPC
// ─────────────────────────────────────────────────────────────────────────────
static void run_scheduler(UiState *st) {
    if (st->count == 0) {
        clear();
        mvprintw(3, 2, "No patients to schedule. Add patients first.");
        mvprintw(LINES-2, 2, "Press any key to return...");
        getch();
        return;
    }

    clear();
    mvprintw(2, 2, "Starting scheduler with %s algorithm...", alg_name(st->alg));
    refresh();

    // IPC setup
    if (ipc_setup_fifo() != 0) {
        clear(); mvprintw(3, 2, "Failed to setup FIFO."); getch(); return;
    }
    mqd_t mq = ipc_open_mq(1);
    int shm_fd = -1; SharedStats *stats = NULL;
    int shm_ok = (ipc_setup_shm(&shm_fd, &stats, 1) == 0);
    if (!shm_ok) stats = NULL;

    pid_t pid = fork();
    if (pid == 0) {
        execl("bin/logger", "logger", (char *)NULL);
        _exit(127);
    }

    int fifo_fd = -1;
    for (int tries = 0; tries < 200 && fifo_fd == -1; ++tries) {
        fifo_fd = open(FIFO_PATH, O_WRONLY | O_NONBLOCK);
        if (fifo_fd == -1 && errno == ENXIO) {
            ms_sleep(10);
            continue;
        }
    }
    if (fifo_fd == -1) {
        clear(); mvprintw(3, 2, "Failed to open FIFO - logger not ready."); getch(); return;
    }

    ResourcePool resources;
    if (resources_init(&resources, st->doctors, st->machines, st->rooms) != 0) {
        clear(); mvprintw(3, 2, "Failed to init resources."); getch(); return;
    }

    PatientList list;
    list.items = st->items;
    list.count = st->count;

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    int *order = schedule_order(&list, st->alg, st->quantum_ms);
    ScheduleMetrics metrics = compute_metrics(&list, order, st->alg, st->quantum_ms);

    if (stats) {
        stats->avg_wait_ms = metrics.avg_wait_ms;
        stats->avg_turnaround_ms = metrics.avg_turnaround_ms;
        stats->completed_jobs = (int)list.count;
    }
    if (mq != (mqd_t)-1 && stats) {
        mq_send(mq, "STATS_READY", strlen("STATS_READY"), 1);
    }

    pthread_t *threads = (pthread_t *)calloc(list.count, sizeof(pthread_t));
    WorkerArgs *args = (WorkerArgs *)calloc(list.count, sizeof(WorkerArgs));
    for (size_t k = 0; k < list.count; ++k) {
        int idx = order[k];
        args[k].patient = list.items[idx];
        args[k].resources = &resources;
        args[k].fifo_fd = fifo_fd;
        pthread_create(&threads[k], NULL, patient_thread, &args[k]);
        ms_sleep(10);
    }
    for (size_t k = 0; k < list.count; ++k) pthread_join(threads[k], NULL);

    clock_gettime(CLOCK_MONOTONIC, &t1);
    unsigned long long elapsed_ms = (unsigned long long)((t1.tv_sec - t0.tv_sec) * 1000ULL + (t1.tv_nsec - t0.tv_nsec) / 1000000ULL);

    free(order); free(threads); free(args);
    resources_destroy(&resources);
    close(fifo_fd);
    if (mq != (mqd_t)-1) ipc_close_mq(mq);
    ms_sleep(100);
    ipc_cleanup_fifo();
    if (stats) {
        munmap(stats, sizeof(*stats));
        close(shm_fd);
        ipc_cleanup_shm();
    }

    int status = 0; waitpid(pid, &status, 0);

    // Display results
    clear();
    if (has_colors()) attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(1, 2, "+------------------------------------------------------------------------------+");
    mvprintw(2, 2, "|                        SCHEDULING RESULTS                                    |");
    mvprintw(3, 2, "+------------------------------------------------------------------------------+");
    if (has_colors()) attroff(COLOR_PAIR(1) | A_BOLD);
    
    int row = 5;
    if (has_colors()) attron(COLOR_PAIR(2));
    mvprintw(row++, 2, "Algorithm: %s", alg_name(st->alg));
    if (st->alg == ALG_RR) mvprintw(row++, 2, "Time Quantum: %u ms", st->quantum_ms);
    if (has_colors()) attroff(COLOR_PAIR(2));
    row++;
    
    if (has_colors()) attron(COLOR_PAIR(3) | A_BOLD);
    mvprintw(row++, 2, "PERFORMANCE METRICS:");
    if (has_colors()) attroff(COLOR_PAIR(3) | A_BOLD);
    mvprintw(row++, 4, "Average Waiting Time:    %8.2f ms", metrics.avg_wait_ms);
    mvprintw(row++, 4, "Average Turnaround Time: %8.2f ms", metrics.avg_turnaround_ms);
    mvprintw(row++, 4, "Total Execution Time:    %8llu ms", elapsed_ms);
    mvprintw(row++, 4, "Completed Jobs:          %8d", (int)list.count);
    row++;
    
    double util_doctors = elapsed_ms ? (double)resources.busy_doctors_ms / (elapsed_ms * resources.num_doctors) : 0.0;
    double util_machines = elapsed_ms ? (double)resources.busy_machines_ms / (elapsed_ms * resources.num_machines) : 0.0;
    double util_rooms = elapsed_ms ? (double)resources.busy_rooms_ms / (elapsed_ms * resources.num_rooms) : 0.0;
    
    if (has_colors()) attron(COLOR_PAIR(2));
    mvprintw(row++, 2, "RESOURCE UTILIZATION:");
    if (has_colors()) attroff(COLOR_PAIR(2));
    mvprintw(row++, 4, "Doctors (%d):  %5.1f%%", resources.num_doctors, util_doctors * 100.0);
    mvprintw(row++, 4, "Machines (%d): %5.1f%%", resources.num_machines, util_machines * 100.0);
    mvprintw(row++, 4, "Rooms (%d):    %5.1f%%", resources.num_rooms, util_rooms * 100.0);
    row++;
    
    mvprintw(row, 2, "Logs saved to: logs/log.txt");
    mvprintw(LINES-2, 2, "Press any key to view RESOURCE USAGE PATTERN...");
    getch();
    
    // Build timeline for resource usage pattern
    size_t slice_count = 0;
    Slice *slices = build_timeline(&list, st->alg, st->quantum_ms, &slice_count);
    
    // Display resource usage pattern on new screen
    clear();
    if (has_colors()) attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(1, 2, "+------------------------------------------------------------------------------+");
    mvprintw(2, 2, "|                     RESOURCE USAGE PATTERN                                   |");
    mvprintw(3, 2, "+------------------------------------------------------------------------------+");
    if (has_colors()) attroff(COLOR_PAIR(1) | A_BOLD);
    
    mvprintw(5, 2, "Algorithm: %s | Patients: %zu", alg_name(st->alg), list.count);
    
    draw_resource_usage_pattern(&list, slices, slice_count, 7);
    
    if (slices) free(slices);
    
    mvprintw(LINES-2, 2, "Press any key to return...");
    getch();
}

// ─────────────────────────────────────────────────────────────────────────────
// Algorithm Comparison
// ─────────────────────────────────────────────────────────────────────────────
static void compare_algorithms(UiState *st) {
    if (st->count == 0) {
        clear();
        mvprintw(3, 2, "No patients to compare. Add patients first.");
        mvprintw(LINES-2, 2, "Press any key to return...");
        getch();
        return;
    }
    
    PatientList list; 
    list.items = st->items; 
    list.count = st->count;
    Algorithm algs[4] = { ALG_FCFS, ALG_SJF, ALG_PRIORITY, ALG_RR };
    const char *names[4] = { "FCFS", "SJF", "Priority", "Round Robin" };
    ScheduleMetrics mets[4];
    
    clear();
    if (has_colors()) attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(1, 2, "+------------------------------------------------------------------------------+");
    mvprintw(2, 2, "|                    ALGORITHM COMPARISON REPORT                               |");
    mvprintw(3, 2, "+------------------------------------------------------------------------------+");
    if (has_colors()) attroff(COLOR_PAIR(1) | A_BOLD);
    
    mvprintw(5, 2, "Patients: %zu | RR Quantum: %u ms | Doctors: %d | Machines: %d | Rooms: %d",
             list.count, st->quantum_ms, st->doctors, st->machines, st->rooms);
    mvhline(6, 2, '-', COLS-4);
    
    if (has_colors()) attron(COLOR_PAIR(2) | A_BOLD);
    mvprintw(8, 2, "%-16s %-20s %-22s %-10s", "Algorithm", "Avg Wait (ms)", "Avg Turnaround (ms)", "Winner");
    if (has_colors()) attroff(COLOR_PAIR(2) | A_BOLD);
    mvhline(9, 2, '-', COLS-4);
    
    double min_wait = 1e9, min_turn = 1e9;
    int best_wait = 0, best_turn = 0;
    
    for (int i = 0; i < 4; ++i) {
        int *order = schedule_order(&list, algs[i], st->quantum_ms);
        mets[i] = compute_metrics(&list, order, algs[i], st->quantum_ms);
        free(order);
        if (mets[i].avg_wait_ms < min_wait) { min_wait = mets[i].avg_wait_ms; best_wait = i; }
        if (mets[i].avg_turnaround_ms < min_turn) { min_turn = mets[i].avg_turnaround_ms; best_turn = i; }
    }
    
    for (int i = 0; i < 4; ++i) {
        int row = 10 + i;
        int pair = (i == 3 ? 5 : (i == 2 ? 4 : 3));
        if (has_colors()) attron(COLOR_PAIR(pair));
        
        char winner[32] = "";
        if (i == best_wait && i == best_turn) snprintf(winner, sizeof(winner), "BEST");
        else if (i == best_wait) snprintf(winner, sizeof(winner), "Wait");
        else if (i == best_turn) snprintf(winner, sizeof(winner), "Turn");
        
        mvprintw(row, 2, "%-16s %-20.2f %-22.2f %-10s", names[i], mets[i].avg_wait_ms, mets[i].avg_turnaround_ms, winner);
        if (has_colors()) attroff(COLOR_PAIR(pair));
    }
    
    mvhline(15, 2, '-', COLS-4);
    if (has_colors()) attron(COLOR_PAIR(3) | A_BOLD);
    mvprintw(17, 2, "ANALYSIS:");
    if (has_colors()) attroff(COLOR_PAIR(3) | A_BOLD);
    mvprintw(18, 4, "Best for Waiting Time:    %s (%.2f ms)", names[best_wait], mets[best_wait].avg_wait_ms);
    mvprintw(19, 4, "Best for Turnaround Time: %s (%.2f ms)", names[best_turn], mets[best_turn].avg_turnaround_ms);
    
    mvprintw(LINES-2, 2, "Press any key to return...");
    getch();
}

// ─────────────────────────────────────────────────────────────────────────────
// Generate Detailed Report to File
// ─────────────────────────────────────────────────────────────────────────────
static void generate_report(UiState *st) {
    if (st->count == 0) {
        clear();
        mvprintw(3, 2, "No patients to report. Add patients first.");
        mvprintw(LINES-2, 2, "Press any key to return...");
        getch();
        return;
    }
    
    FILE *f = fopen("data/report.txt", "w");
    if (!f) {
        clear();
        mvprintw(3, 2, "Failed to create report file.");
        mvprintw(LINES-2, 2, "Press any key to return...");
        getch();
        return;
    }
    
    PatientList list; 
    list.items = st->items; 
    list.count = st->count;
    
    fprintf(f, "================================================================================\n");
    fprintf(f, "                    HOSPITAL RESOURCE SCHEDULER - DETAILED REPORT\n");
    fprintf(f, "================================================================================\n\n");
    
    fprintf(f, "GROUP MEMBERS:\n");
    fprintf(f, "  Asif Hussain     (2023-CS-646) - CPU Scheduling & Threads\n");
    fprintf(f, "  Muhammad Wakeel  (2023-CS-601) - Process Creation, IPC & Synchronization\n\n");
    
    fprintf(f, "CONFIGURATION:\n");
    fprintf(f, "  Total Patients: %zu\n", list.count);
    fprintf(f, "  Doctors: %d | Machines: %d | Rooms: %d\n", st->doctors, st->machines, st->rooms);
    fprintf(f, "  RR Quantum: %u ms\n\n", st->quantum_ms);
    
    fprintf(f, "PATIENT LIST:\n");
    fprintf(f, "--------------------------------------------------------------------------------\n");
    fprintf(f, "%-4s %-20s %-14s %-10s %-12s %-12s\n", "ID", "Name", "Service", "Priority", "Req(ms)", "Arrival(ms)");
    fprintf(f, "--------------------------------------------------------------------------------\n");
    for (size_t i = 0; i < list.count; ++i) {
        Patient *p = &list.items[i];
        fprintf(f, "%-4d %-20s %-14s %-10d %-12u %-12u\n",
                p->id, p->name, service_name(p->service), p->priority,
                p->required_time_ms, p->arrival_ms);
    }
    fprintf(f, "\n");
    
    fprintf(f, "================================================================================\n");
    fprintf(f, "                           ALGORITHM COMPARISON\n");
    fprintf(f, "================================================================================\n\n");
    
    Algorithm algs[4] = { ALG_FCFS, ALG_SJF, ALG_PRIORITY, ALG_RR };
    const char *names[4] = { "FCFS", "SJF", "Priority", "Round Robin" };
    ScheduleMetrics mets[4];
    
    fprintf(f, "%-16s %-20s %-22s\n", "Algorithm", "Avg Wait (ms)", "Avg Turnaround (ms)");
    fprintf(f, "--------------------------------------------------------------------------------\n");
    
    double min_wait = 1e9, min_turn = 1e9;
    int best_wait = 0, best_turn = 0;
    
    for (int i = 0; i < 4; ++i) {
        int *order = schedule_order(&list, algs[i], st->quantum_ms);
        mets[i] = compute_metrics(&list, order, algs[i], st->quantum_ms);
        free(order);
        if (mets[i].avg_wait_ms < min_wait) { min_wait = mets[i].avg_wait_ms; best_wait = i; }
        if (mets[i].avg_turnaround_ms < min_turn) { min_turn = mets[i].avg_turnaround_ms; best_turn = i; }
        fprintf(f, "%-16s %-20.2f %-22.2f\n", names[i], mets[i].avg_wait_ms, mets[i].avg_turnaround_ms);
    }
    
    fprintf(f, "\nANALYSIS:\n");
    fprintf(f, "  Best for Waiting Time:    %s (%.2f ms)\n", names[best_wait], mets[best_wait].avg_wait_ms);
    fprintf(f, "  Best for Turnaround Time: %s (%.2f ms)\n", names[best_turn], mets[best_turn].avg_turnaround_ms);
    fprintf(f, "\n");
    
    fprintf(f, "================================================================================\n");
    fprintf(f, "                              OS CONCEPTS USED\n");
    fprintf(f, "================================================================================\n\n");
    fprintf(f, "1. CPU SCHEDULING ALGORITHMS:\n");
    fprintf(f, "   - FCFS: First Come First Serve (non-preemptive, ordered by arrival)\n");
    fprintf(f, "   - SJF: Shortest Job First (non-preemptive, ordered by burst time)\n");
    fprintf(f, "   - Priority: Jobs with higher priority (lower number) run first\n");
    fprintf(f, "   - Round Robin: Time-sliced preemptive scheduling\n\n");
    fprintf(f, "2. MULTITHREADING (pthreads):\n");
    fprintf(f, "   - Each patient request runs as a separate thread\n");
    fprintf(f, "   - Parallel execution for concurrent patient processing\n\n");
    fprintf(f, "3. SYNCHRONIZATION:\n");
    fprintf(f, "   - Semaphores: Control access to limited resources (doctors, machines, rooms)\n");
    fprintf(f, "   - Mutex: Protect shared data structures and logging\n\n");
    fprintf(f, "4. INTER-PROCESS COMMUNICATION (IPC):\n");
    fprintf(f, "   - Named FIFO (Pipe): Transfer log messages to logger process\n");
    fprintf(f, "   - Message Queue: Notify logger of stats availability\n");
    fprintf(f, "   - Shared Memory: Share performance metrics between processes\n\n");
    fprintf(f, "5. PROCESS CREATION:\n");
    fprintf(f, "   - fork(): Create child process for logger\n");
    fprintf(f, "   - exec(): Replace child process image with logger program\n\n");
    fprintf(f, "6. DYNAMIC MEMORY ALLOCATION:\n");
    fprintf(f, "   - malloc/calloc: Allocate memory for patient data structures\n");
    fprintf(f, "   - realloc: Resize patient queue as needed\n");
    fprintf(f, "   - free: Release memory when done\n\n");
    
    fprintf(f, "================================================================================\n");
    fprintf(f, "                              END OF REPORT\n");
    fprintf(f, "================================================================================\n");
    
    fclose(f);
    
    clear();
    if (has_colors()) attron(COLOR_PAIR(3) | A_BOLD);
    mvprintw(3, 2, "Report generated successfully!");
    if (has_colors()) attroff(COLOR_PAIR(3) | A_BOLD);
    mvprintw(5, 2, "File saved to: data/report.txt");
    mvprintw(LINES-2, 2, "Press any key to return...");
    getch();
}

// ─────────────────────────────────────────────────────────────────────────────
// Main Menu Header
// ─────────────────────────────────────────────────────────────────────────────
static void draw_header(UiState *st) {
    if (has_colors()) attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(0, 2, "+------------------------------------------------------------------------------+");
    mvprintw(1, 2, "|         HOSPITAL RESOURCE SCHEDULER - OS Semester Project                   |");
    mvprintw(2, 2, "|     Asif Hussain (2023-CS-646) | Muhammad Wakeel (2023-CS-601)              |");
    mvprintw(3, 2, "+------------------------------------------------------------------------------+");
    if (has_colors()) attroff(COLOR_PAIR(1) | A_BOLD);
    
    mvprintw(5, 2, "[1] Add Patient       [2] View Patients    [3] Update Patient");
    mvprintw(6, 2, "[4] Delete Patient    [5] Run Scheduler    [6] View Logs");
    mvprintw(7, 2, "[7] Set Resources     [8] Set Algorithm    [9] Set Quantum");
    mvprintw(8, 2, "[g] Generate N Pts    [s] Save CSV         [l] Load CSV");
    mvprintw(9, 2, "[c] Clear List        [x] Compare Algs     [v] View Gantt Chart");
    mvprintw(10, 2, "[r] Generate Report   [h] Help             [q] Exit");
    
    mvhline(11, 2, '-', COLS-4);
    
    if (has_colors()) attron(COLOR_PAIR(2) | A_BOLD);
    mvprintw(12, 2, "Resources: Doctors=%d  Machines=%d  Rooms=%d", st->doctors, st->machines, st->rooms);
    mvprintw(13, 2, "Algorithm: %-12s  Quantum: %u ms  Patients: %zu", 
             alg_name(st->alg), st->quantum_ms, st->count);
    if (has_colors()) attroff(COLOR_PAIR(2) | A_BOLD);
    
    mvhline(14, 2, '-', COLS-4);
}

// ─────────────────────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────────────────────
int main(void) {
    UiState st; 
    ui_init(&st);
    srand((unsigned)time(NULL));

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    ui_init_colors();

    int running = 1;
    while (running) {
        clear();
        draw_header(&st);
        mvprintw(LINES-2, 2, "Select an option...");
        refresh();
        
        int ch = getch();
        switch (ch) {
            case '1': {
                clear(); draw_header(&st);
                char name[MAX_NAME_LEN]; name[0] = '\0';
                prompt_str("Name", name, sizeof(name), "Patient");
                ServiceType svc = prompt_service(SERVICE_CONSULTATION);
                int pri = prompt_int("Priority (1=highest, 5=lowest)", 3);
                if (pri < 1) pri = 1;
                if (pri > 5) pri = 5;
                unsigned req = prompt_uint("Required Time (ms)", 300);
                unsigned arr = prompt_uint("Arrival Time (ms)", 0);
                if (strlen(name) == 0) snprintf(name, sizeof(name), "Patient_%02d", st.next_id);
                add_patient(&st, name, pri, svc, req, arr);
                break;
            }
            case '2': view_patients(&st); break;
            case '3': {
                int id = prompt_int("Update Patient ID", 1);
                size_t idx; Patient *p = find_patient(&st, id, &idx);
                if (!p) { 
                    clear(); 
                    mvprintw(3, 2, "Patient ID %d not found.", id); 
                    getch(); 
                    break; 
                }
                char name[MAX_NAME_LEN]; snprintf(name, sizeof(name), "%s", p->name);
                prompt_str("Name", name, sizeof(name), p->name);
                p->service = prompt_service(p->service);
                p->priority = prompt_int("Priority (1..5)", p->priority);
                if (p->priority < 1) p->priority = 1;
                if (p->priority > 5) p->priority = 5;
                p->required_time_ms = prompt_uint("Required Time (ms)", p->required_time_ms);
                p->arrival_ms = prompt_uint("Arrival Time (ms)", p->arrival_ms);
                snprintf(p->name, MAX_NAME_LEN, "%s", name);
                break;
            }
            case '4': {
                int id = prompt_int("Delete Patient ID", 1);
                if (delete_patient(&st, id) != 0) { 
                    clear(); 
                    mvprintw(3, 2, "Patient ID %d not found.", id); 
                    getch(); 
                }
                break;
            }
            case '5': run_scheduler(&st); break;
            case '6': view_logs(); break;
            case '7': {
                st.doctors = prompt_int("Number of Doctors", st.doctors);
                if (st.doctors < 1) st.doctors = 1;
                st.machines = prompt_int("Number of Machines", st.machines);
                if (st.machines < 1) st.machines = 1;
                st.rooms = prompt_int("Number of Rooms", st.rooms);
                if (st.rooms < 1) st.rooms = 1;
                break;
            }
            case '8': st.alg = prompt_alg(st.alg); break;
            case '9': st.quantum_ms = prompt_uint("RR Quantum (ms)", st.quantum_ms); break;
            case 'g': {
                int n = prompt_int("Generate how many patients?", 5);
                if (n < 1) n = 1;
                if (n > 100) n = 100;
                for (int i = 0; i < n; ++i) {
                    char name[MAX_NAME_LEN];
                    snprintf(name, sizeof(name), "Patient_%02d", st.next_id);
                    ServiceType svc = (ServiceType)(rand() % 3);
                    int pri = (rand() % 5) + 1;
                    unsigned req = (unsigned)(100 + (rand() % 900));
                    unsigned arr = (unsigned)(rand() % 500);
                    add_patient(&st, name, pri, svc, req, arr);
                }
                break;
            }
            case 's': {
                char path[256]; snprintf(path, sizeof(path), "data/patients.csv");
                PatientList list; list.items = st.items; list.count = st.count;
                if (save_patients_csv(path, &list) == 0) {
                    clear(); mvprintw(3, 2, "Saved %zu patients to %s", st.count, path);
                } else {
                    clear(); mvprintw(3, 2, "Failed to save to %s", path);
                }
                mvprintw(LINES-2, 2, "Press any key to return..."); getch();
                break;
            }
            case 'l': {
                char path[256]; snprintf(path, sizeof(path), "data/patients.csv");
                PatientList loaded = {0};
                if (load_patients_csv(path, &loaded) == 0) {
                    free(st.items);
                    st.items = loaded.items;
                    st.count = loaded.count;
                    int max_id = 0; 
                    for (size_t i = 0; i < st.count; ++i) 
                        if (st.items[i].id > max_id) max_id = st.items[i].id;
                    st.next_id = max_id + 1;
                    clear(); mvprintw(3, 2, "Loaded %zu patients from %s", st.count, path);
                } else {
                    clear(); mvprintw(3, 2, "Failed to load from %s", path);
                }
                mvprintw(LINES-2, 2, "Press any key to return..."); getch();
                break;
            }
            case 'c': {
                free(st.items); st.items = NULL; st.count = 0; st.next_id = 1;
                clear(); mvprintw(3, 2, "Patient list cleared.");
                mvprintw(LINES-2, 2, "Press any key to return..."); getch();
                break;
            }
            case 'x': compare_algorithms(&st); break;
            case 'v': view_gantt(&st); break;
            case 'r': generate_report(&st); break;
            case 'h': show_help(); break;
            case 'q':
            case 'Q':
                running = 0; 
                break;
            default:
                break;
        }
    }

    endwin();
    free(st.items);
    return 0;
}
