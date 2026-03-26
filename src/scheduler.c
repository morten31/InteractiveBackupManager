#define _XOPEN_SOURCE 700
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "scheduler.h"
#include "worker.h"

pid_t start_backup_process(const char* source, const char* target)
{
    pid_t pid = fork();

    if (pid < 0)
    {
        return -1;
    }
    else if (pid == 0)
    {
        // Proces dziecka
        run_backup_process(source, target);
    }
    return pid;
}

int job_exists(BackupJob* head, const char* absolute_source, const char* absolute_target)
{
    BackupJob* current_job = head;
    while (current_job != NULL)
    {
        int source_match = (strcmp(current_job->source_path, absolute_source) == 0);
        int target_match = (strcmp(current_job->target_path, absolute_target) == 0);

        if (source_match && target_match)
        {
            return 1;  // znaleziono
        }
        current_job = current_job->next;
    }
    return 0;  // nie znaleziono
}

int is_directory_empty(const char* path)
{
    DIR* dir = opendir(path);
    if (!dir)
        return 1;

    struct dirent* entry;
    int has_files = 0;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
        {
            has_files = 1;
            break;
        }
    }
    closedir(dir);
    return !has_files;
}

int is_subpath(const char* path_parent, const char* path_child)
{
    size_t len_p = strlen(path_parent);
    if (strncmp(path_child, path_parent, len_p) == 0)
    {
        if (path_child[len_p] == '/' || path_child[len_p] == '\0')
        {
            return 1;
        }
    }
    return 0;
}

int job_add(BackupJob** job_list_head, const char* source, const char* target)
{
    char absolute_source[2 * PATH_MAX];
    char absolute_target[2 * PATH_MAX];

    if (realpath(source, absolute_source) == NULL)
    {
        perror("Błąd: Nie można znaleźć katalogu źródłowego");
        return -1;
    }

    if (realpath(target, absolute_target) == NULL)
    {
        if (errno == ENOENT)
        {
            char cwd[PATH_MAX];
            if (!getcwd(cwd, sizeof(cwd)))
                return -1;
            if (target[0] == '/')
                snprintf(absolute_target, 2 * PATH_MAX, "%s", target);
            else
                snprintf(absolute_target, 2 * PATH_MAX, "%s/%s", cwd, target);
        }
        else
        {
            perror("Błąd ścieżki docelowej");
            return -1;
        }
    }
    else
    {
        if (!is_directory_empty(absolute_target))
        {
            fprintf(stderr, "Błąd: Katalog docelowy '%s' istnieje i nie jest pusty!\n", absolute_target);
            return -1;
        }
    }

    if (is_subpath(absolute_source, absolute_target))
    {
        fprintf(stderr, "Błąd: ścieżka docelowa wewnątrz ścieżki źródłowej\n");
        return -1;
    }

    if (job_exists(*job_list_head, absolute_source, absolute_target))
    {
        fprintf(stderr, "Błąd: kopia już istnieje (%s -> %s)\n", absolute_source, absolute_target);
        return -1;
    }

    pid_t new_pid = start_backup_process(absolute_source, absolute_target);
    if (new_pid < 0)
    {
        fprintf(stderr, "Błąd: nie udało się utworzyć procesu.\n");
        return -1;
    }

    BackupJob* job_node = malloc(sizeof(BackupJob));
    if (job_node == NULL)
    {
        perror("Błąd alokacji pamięci");
        return -1;
    }

    strncpy(job_node->source_path, absolute_source, PATH_MAX);
    strncpy(job_node->target_path, absolute_target, PATH_MAX);
    job_node->process_id = new_pid;
    job_node->next = *job_list_head;
    *job_list_head = job_node;

    printf("Rozpoczęto backup [PID: %d]\n   Z:  %s\n   Do: %s\n", new_pid, absolute_source, absolute_target);
    return 0;
}

pid_t job_remove(BackupJob** job_list_head, const char* source, const char* target)
{
    char absolute_source[PATH_MAX];
    if (realpath(source, absolute_source) == NULL)
    {
        strncpy(absolute_source, source, PATH_MAX);
    }

    BackupJob* current_job = *job_list_head;
    BackupJob* previous_job = NULL;

    while (current_job != NULL)
    {
        int source_matches = (strcmp(current_job->source_path, absolute_source) == 0);
        int target_matches = 0;
        char temp_abs_target[PATH_MAX];

        if (realpath(target, temp_abs_target) != NULL)
            target_matches = (strcmp(current_job->target_path, temp_abs_target) == 0);
        else
            target_matches = (strcmp(current_job->target_path, target) == 0);

        if (source_matches && target_matches)
        {
            pid_t pid_to_return = current_job->process_id;

            if (previous_job == NULL)
                *job_list_head = current_job->next;
            else
                previous_job->next = current_job->next;

            kill(pid_to_return, SIGTERM);
            int status;
            waitpid(pid_to_return, &status, 0);
            free(current_job);
            return pid_to_return;
        }

        previous_job = current_job;
        current_job = current_job->next;
    }

    return -1;  // nie znaleziono
}

void job_print_all(BackupJob* job_list_head)
{
    if (job_list_head == NULL)
    {
        printf("Lista kopii  jest pusta\n");
        return;
    }

    printf("--- Aktywne kopie zapasowe ---\n");
    BackupJob* current_job = job_list_head;
    int counter = 1;

    while (current_job != NULL)
    {
        printf("%d. [PID: %d]\n", counter++, current_job->process_id);
        printf("    Źródło: %s\n", current_job->source_path);
        printf("    Cel:    %s\n", current_job->target_path);
        current_job = current_job->next;
    }
    printf("------------------------------\n");
}

void job_end_all(BackupJob* head)
{
    BackupJob* current = head;
    while (current != NULL)
    {
        BackupJob* temp = current;
        current = current->next;

        // 1. Wyślij sygnał zakończenia
        printf("Zatrzymywanie procesu %d (%s)...\n", temp->process_id, temp->source_path);
        kill(temp->process_id, SIGTERM);

        // 2. Czekaj na zakończenie procesu (zapobiega Zombie)
        int status;
        waitpid(temp->process_id, &status, WNOHANG);

        // 3. Zwolnij pamięć
        free(temp);
    }
}
