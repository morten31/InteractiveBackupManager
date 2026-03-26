#define _XOPEN_SOURCE 700
#include <dirent.h>
#include <errno.h>
#include <ftw.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "file_manager.h"
#include "watcher.h"
#include "worker.h"

#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUFFER_LEN (1024 * (EVENT_SIZE + 16))
#define MAX_FDS 32

static volatile sig_atomic_t keep_monitoring = 1;

void sigint_child_handler(int sig) { keep_monitoring = 0; }

// Lista watcherów
static WatchNode* watch_list = NULL;
static int inotify_fd;

int handle_nftw_entry(const char* fpath, const struct stat* sb, int tflag, struct FTW* ftwbuf)
{
    if (tflag == FTW_D)  // katalog
    {
        int wd = inotify_add_watch(inotify_fd, fpath, IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO);

        if (wd == -1)
        {
            perror("inotify_add_watch skipped");
        }
        else
        {
            watchlist_add(&watch_list, wd, fpath);
            // printf("Dodawanie obserwacji: %s (wd: %d)\n", fpath, wd);
        }
    }

    return 0;
}

void add_watch_recursive(const char* dir_path)
{
    if (nftw(dir_path, handle_nftw_entry, MAX_FDS, FTW_PHYS) == -1)
    {
        perror("nftw failed");
    }
}

void build_dest_path(const char* src_full_path, const char* root_src, const char* root_dest, char* dest_buffer)
{
    size_t root_len = strlen(root_src);
    const char* relative_part = src_full_path + root_len;
    snprintf(dest_buffer, PATH_MAX, "%s%s", root_dest, relative_part);
}

void run_backup_process(const char* source, const char* target)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_child_handler;

    if (sigaction(SIGINT, &sa, NULL) == -1)
        perror("sigaction");
    if (sigaction(SIGTERM, &sa, NULL) == -1)
        perror("sigaction");

    inotify_fd = inotify_init();
    if (inotify_fd < 0)
    {
        perror("inotify_init");
        exit(EXIT_FAILURE);
    }

    // Wstępne dodanie watcherów
    add_watch_recursive(source);

    // Wstępna synchronizacja
    if (copy_recursive(source, target, source, target) < 0)
    {
        fprintf(stderr, "Wstępna kopia zakończona z błędami.\n");
    }

    char buffer[BUFFER_LEN];
    while (keep_monitoring)
    {
        ssize_t length = read(inotify_fd, buffer, BUFFER_LEN);

        if (length < 0)
        {
            if (errno == EINTR)
            {
                if (!keep_monitoring)
                    break;
                continue;
            }
            perror("read");
            break;
        }

        int i = 0;
        while (i < length)
        {
            struct inotify_event* event = (struct inotify_event*)&buffer[i];

            if (event->len > 0)
            {
                char* src_dir = watchlist_get_path(watch_list, event->wd);

                if (src_dir)
                {
                    char full_src[PATH_MAX];
                    char full_dest[PATH_MAX];

                    // sciezka zrodlowa
                    snprintf(full_src, PATH_MAX, "%s/%s", src_dir, event->name);

                    // sciezka docelowa
                    build_dest_path(full_src, source, target, full_dest);

                    // printf("[EVENT] %s -> %s\n", full_src, full_dest);

                    // usunięcie lub przeniesienie
                    if ((event->mask & IN_DELETE) || (event->mask & IN_MOVED_FROM))
                    {
                        remove_any_path(full_dest);
                    }

                    // modyfikacja
                    else if (event->mask & IN_MODIFY)
                    {
                        // Ignorujemy modyfikacje katalogów
                        if (!(event->mask & IN_ISDIR))
                        {
                            struct stat st;
                            if (lstat(full_src, &st) == 0 && S_ISREG(st.st_mode))
                            {
                                // printf(" - Modyfikacja: %s\n", full_dest);
                                copy_regular_file(full_src, full_dest);
                            }
                        }
                    }

                    // utworzenie
                    else if ((event->mask & IN_CREATE) || (event->mask & IN_MOVED_TO))
                    {
                        if (event->mask & IN_ISDIR)  // katalog
                        {
                            // printf(" - Nowy katalog: %s\n", full_dest);
                            make_directory(full_dest);

                            // wniesiony katalog
                            if (event->mask & IN_MOVED_TO)
                            {
                                copy_recursive(full_src, full_dest, source, target);
                            }

                            // watcher na nowy katalog
                            add_watch_recursive(full_src);
                        }
                        else
                        {
                            // plik lub symlink
                            struct stat st;
                            if (lstat(full_src, &st) == 0)
                            {
                                if (S_ISLNK(st.st_mode))
                                {
                                    // printf(" - Nowy symlink: %s\n", full_dest);
                                    copy_symlink(full_src, full_dest, source, target);
                                }
                                else if (S_ISREG(st.st_mode))
                                {
                                    // printf(" - Nowy plik: %s\n", full_dest);
                                    copy_regular_file(full_src, full_dest);
                                }
                            }
                        }
                    }
                }
            }
            i += EVENT_SIZE + event->len;
        }
    }

    printf("[PID: %d] Kończenie pracy...\n", getpid());
    close(inotify_fd);
    watchlist_free_all(watch_list);
    exit(EXIT_SUCCESS);
}
