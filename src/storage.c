#include "storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ServiceType parse_service(const char *s) {
    if (strcmp(s, "Consultation") == 0) return SERVICE_CONSULTATION;
    if (strcmp(s, "Lab Test") == 0) return SERVICE_LAB_TEST;
    if (strcmp(s, "Treatment") == 0) return SERVICE_TREATMENT;
    // Allow numeric fallback
    int v = atoi(s);
    if (v >= 0 && v <= 2) return (ServiceType)v;
    return SERVICE_CONSULTATION;
}

static const char *service_name_storage(ServiceType s) {
    switch (s) {
        case SERVICE_CONSULTATION: return "Consultation";
        case SERVICE_LAB_TEST: return "Lab Test";
        case SERVICE_TREATMENT: return "Treatment";
        default: return "Consultation";
    }
}

int load_patients_csv(const char *path, PatientList *out_list) {
    if (!path || !out_list) return -1;
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    size_t cap = 16;
    Patient *items = (Patient *)malloc(sizeof(Patient) * cap);
    if (!items) { fclose(f); return -1; }
    size_t count = 0;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || strlen(line) < 3) continue;
        // id,name,service,priority,required_ms,arrival_ms
        char *tok;
        int id, priority; unsigned req_ms, arr_ms; char name[MAX_NAME_LEN]; char svc[64];
        // Use a copy since strtok mutates
        char buf[512];
        strncpy(buf, line, sizeof(buf)); buf[sizeof(buf)-1] = '\0';
        tok = strtok(buf, ",\n"); if (!tok) continue; id = atoi(tok);
        tok = strtok(NULL, ",\n"); if (!tok) continue; snprintf(name, MAX_NAME_LEN, "%s", tok);
        tok = strtok(NULL, ",\n"); if (!tok) continue; snprintf(svc, sizeof(svc), "%s", tok);
        tok = strtok(NULL, ",\n"); if (!tok) continue; priority = atoi(tok);
        tok = strtok(NULL, ",\n"); if (!tok) continue; req_ms = (unsigned)atoi(tok);
        tok = strtok(NULL, ",\n"); if (!tok) continue; arr_ms = (unsigned)atoi(tok);

        if (count == cap) {
            cap *= 2;
            Patient *ni = (Patient *)realloc(items, sizeof(Patient) * cap);
            if (!ni) { free(items); fclose(f); return -1; }
            items = ni;
        }
        Patient *p = &items[count++];
        p->id = id > 0 ? id : (int)count;
        snprintf(p->name, MAX_NAME_LEN, "%s", name);
        p->service = parse_service(svc);
        p->priority = priority;
        p->required_time_ms = req_ms;
        p->arrival_ms = arr_ms;
    }
    fclose(f);

    out_list->items = items;
    out_list->count = count;
    return 0;
}

int save_patients_csv(const char *path, const PatientList *list) {
    if (!path || !list) return -1;
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "# id,name,service,priority,required_ms,arrival_ms\n");
    for (size_t i = 0; i < list->count; ++i) {
        const Patient *p = &list->items[i];
        fprintf(f, "%d,%s,%s,%d,%u,%u\n",
                p->id, p->name, service_name_storage(p->service), p->priority,
                p->required_time_ms, p->arrival_ms);
    }
    fclose(f);
    return 0;
}
