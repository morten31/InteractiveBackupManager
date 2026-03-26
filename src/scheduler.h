#ifndef SCHEDULER
#define SCHEDULER

#include <limits.h>
#include <sys/types.h>

typedef struct BackupJob
{
    char source_path[PATH_MAX];
    char target_path[PATH_MAX];
    pid_t process_id;
    struct BackupJob* next;
} BackupJob;

// Słownik

// Dodaje nowe zadanie
int job_add(BackupJob** job_list_head, const char* source, const char* target);

// Usuwa zadanie z listy na podstawie źródła i celu
pid_t job_remove(BackupJob** job_list_head, const char* source, const char* target);

// Wypisuje na ekran wszystkie zadania
void job_print_all(BackupJob* job_list_head);

// Czyści listę
void job_end_all(BackupJob* head);

#endif
