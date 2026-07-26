
#define _XOPEN_SOURCE 700
#include <dirent.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "file_manager.h"
#include "parser.h"
#include "scheduler.h"

volatile sig_atomic_t keep_running = 1;

void sigint_handler(int sig)
{
    keep_running = 0;
}

void setup_signals()
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;

    if (sigaction(SIGINT, &sa, NULL) == -1)
        perror("sigaction");
    if (sigaction(SIGTERM, &sa, NULL) == -1)
        perror("sigaction");

    sigset_t mask;
    sigfillset(&mask);
    sigdelset(&mask, SIGINT);
    sigdelset(&mask, SIGTERM);
    sigprocmask(SIG_SETMASK, &mask, NULL);
}

int main()
{
    setup_signals();

    BackupJob* jobs = NULL;

    char line[1024];
    char* args[MAX_ARGUMENTS];

    printf("=== Backup Synchronization Tool ===\n");
    printf("Available commands: add, list, end, restore, exit\n");
    printf("Example: add \"my directory\" /tmp/backup1 /tmp/backup2\n\n");

    while (keep_running)
    {
        if (fgets(line, sizeof(line), stdin) == NULL)
        {
            keep_running = 0;
            break;
        }

        // parsowanie argumentów
        int argc = parse_command(line, args);

        if (argc == 0)
        {
            continue;
        }

        if (strcmp(args[0], "exit") == 0)
        {
            keep_running = 0;
        }
        else if (strcmp(args[0], "add") == 0)
        {
            if (argc < 3)
            {
                fprintf(stderr, "Usage: add <source> <target> [<target>] ... \n");
            }
            else
            {
                for (int i = 2; i < argc; i++)
                {
                    int res = job_add(&jobs, args[1], args[i]);
                    if (res == -1)
                        fprintf(stderr, "Adding failed. \n");
                }
            }
        }
        else if (strcmp(args[0], "list") == 0)
        {
            job_print_all(jobs);
        }

        else if (strcmp(args[0], "end") == 0)
        {
            if (argc < 3)
            {
                fprintf(stderr, "Usage: end <source> <target> [<target>] ... \n");
            }
            else
            {
                for (int i = 2; i < argc; i++)
                {
                    pid_t removed_pid = job_remove(&jobs, args[1], args[i]);
                    if (removed_pid > 0)
                    {
                        printf("Ended backup [PID: %d]: %s -> %s\n", removed_pid, args[1], args[i]);
                    }
                    else
                    {
                        fprintf(stderr, "could not find backup: %s -> %s\n", args[1], args[i]);
                    }
                }
            }
        }

        else if (strcmp(args[0], "restore") == 0)
        {
            if (argc != 3)
            {
                fprintf(stderr, "Usage: restore <source> <target>\n");
            }
            else
            {
                char* src = args[1];
                char* back = args[2];

                printf("Restoring: Copy '%s' -> Source '%s'...\n", back, src);

                struct stat st;
                if (stat(back, &st) < 0 || !S_ISDIR(st.st_mode))
                {
                    fprintf(stderr, "Copy directory '%s' does not exist.\n", back);
                    continue;
                }
                if (remove_any_path(src) < 0)
                {
                    fprintf(stderr, "Cleaning error.\n");
                    continue;
                }
                make_directory(src);

                if (copy_recursive(back, src, back, src) == -1)
                    fprintf(stderr, "Restoration error.\n");
            }
        }
    }
    printf("\nClosing the program...\n");
    job_end_all(jobs);
    return 0;
}
