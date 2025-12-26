#ifndef STORAGE_H
#define STORAGE_H

#include "patient.h"

// Load patients from CSV file: id,name,service,priority,required_ms,arrival_ms
// Returns 0 on success; populates PatientList (allocates memory). On failure returns -1.
int load_patients_csv(const char *path, PatientList *out_list);

// Save patients to CSV file. Returns 0 on success.
int save_patients_csv(const char *path, const PatientList *list);

#endif // STORAGE_H
