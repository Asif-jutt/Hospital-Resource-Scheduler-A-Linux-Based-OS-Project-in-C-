#include "patient.h"

static ServiceType rand_service() {
    int r = rand() % 3;
    switch (r) {
        case 0: return SERVICE_CONSULTATION;
        case 1: return SERVICE_LAB_TEST;
        default: return SERVICE_TREATMENT;
    }
}

PatientList create_patients(size_t n) {
    PatientList list = {0};
    list.count = n;
    list.items = (Patient *)calloc(n, sizeof(Patient));
    if (!list.items) {
        fprintf(stderr, "Allocation failed for patients\n");
        exit(1);
    }
    srand((unsigned)time(NULL));
    for (size_t i = 0; i < n; ++i) {
        Patient *p = &list.items[i];
        p->id = (int)i + 1;
        snprintf(p->name, MAX_NAME_LEN, "Patient_%02d", p->id);
        p->priority = (rand() % 5) + 1; // 1..5
        p->service = rand_service();
        p->required_time_ms = (unsigned)(100 + (rand() % 900)); // 100..1000 ms
        p->arrival_ms = (unsigned)(rand() % 500); // 0..500 ms
    }
    return list;
}

void free_patients(PatientList *list) {
    if (!list) return;
    free(list->items);
    list->items = NULL;
    list->count = 0;
}
