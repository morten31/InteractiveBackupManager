#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>

#include "file_manager.h"

ssize_t bulk_read(int fd, char* buf, size_t count)
{
    ssize_t c;
    ssize_t len = 0;

    do
    {
        c = TEMP_FAILURE_RETRY(read(fd, buf, count));
        if (c < 0)
            return c;

        if (c == 0)
            return len;  // EOF

        buf += c;
        len += c;
        count -= c;
    } while (count > 0);

    return len;
}

ssize_t bulk_write(int fd, char* buf, size_t count)
{
    ssize_t c;
    ssize_t len = 0;

    do
    {
        c = TEMP_FAILURE_RETRY(write(fd, buf, count));
        if (c < 0)
            return c;

        buf += c;
        len += c;
        count -= c;
    } while (count > 0);

    return len;
}

int copy_regular_file(const char* src_path, const char* dest_path)
{
    int src_fd;
    int dest_fd;
    char* buffer = NULL;
    int status = 0;  // 0 = success, -1 = fail

    // Otwarcie src
    if ((src_fd = TEMP_FAILURE_RETRY(open(src_path, O_RDONLY))) < 0)
    {
        perror("Błąd otwarcia pliku źródłowego");
        return -1;
    }

    struct stat st;
    if (TEMP_FAILURE_RETRY(fstat(src_fd, &st)) < 0)
    {
        perror("Błąd fstat");
        TEMP_FAILURE_RETRY(close(src_fd));
        return -1;
    }

    // Otwarcie dest
    if ((dest_fd = TEMP_FAILURE_RETRY(open(dest_path, O_WRONLY | O_CREAT | O_TRUNC, 0600))) < 0)
    {
        perror("Błąd otwarcia pliku docelowego");
        TEMP_FAILURE_RETRY(close(src_fd));
        return -1;
    }

    buffer = malloc(COPY_BUFFER_SIZE);
    if (buffer == NULL)
    {
        perror("Błąd malloc");
        TEMP_FAILURE_RETRY(close(src_fd));
        TEMP_FAILURE_RETRY(close(dest_fd));
        return -1;
    }

    // kopiowanie
    while (1)
    {
        ssize_t bytes_read = bulk_read(src_fd, buffer, COPY_BUFFER_SIZE);

        if (bytes_read < 0)
        {
            perror("Błąd odczytu");
            status = -1;
            break;
        }

        if (bytes_read == 0)
        {
            break;  // koniec
        }

        ssize_t bytes_written = bulk_write(dest_fd, buffer, bytes_read);
        if (bytes_written < 0 || (size_t)bytes_written != (size_t)bytes_read)
        {
            perror("Błąd zapisu");
            status = -1;
            break;
        }
    }

    // kopiowanie informacji o pliku
    if (status == 0)
    {
        if (TEMP_FAILURE_RETRY(fchmod(dest_fd, st.st_mode)) < 0)
            perror("Nie udało się ustawić uprawnień");

        struct timespec times[2];
        times[0] = st.st_atim;  // czas dostępu
        times[1] = st.st_mtim;  // czas modyfikacji

        if (TEMP_FAILURE_RETRY(futimens(dest_fd, times)) < 0)
            perror("Nie udało się ustawić czasu modyfikacji");
    }

    free(buffer);
    TEMP_FAILURE_RETRY(close(src_fd));
    TEMP_FAILURE_RETRY(close(dest_fd));
    return status;
}

int make_directory(const char* path)
{
    struct stat st;
    if (stat(path, &st) == 0)
    {
        if (S_ISDIR(st.st_mode))
        {
            return 0;  // katalog już istnieje
        }
        else
        {
            errno = EEXIST;
            return -1;
        }
    }

    if (mkdir(path, 0755) < 0)
    {
        if (errno == EEXIST)
        {
            return 0;
        }
        perror("mkdir failed");
        return -1;
    }
    return 0;
}

ssize_t get_symlink_target(const char* path, char* buffer, size_t size)
{
    ssize_t len = TEMP_FAILURE_RETRY(readlink(path, buffer, size - 1));
    if (len < 0)
    {
        perror("readlink failed");
        return -1;
    }
    buffer[len] = '\0';
    return len;
}

int create_symlink(const char* target, const char* dest_path)
{
    if (TEMP_FAILURE_RETRY(unlink(dest_path)) < 0)
    {
        if (errno != ENOENT)
        {
            perror("unlink failed");
            return -1;
        }
    }

    if (TEMP_FAILURE_RETRY(symlink(target, dest_path)) < 0)
    {
        perror("symlink creation failed");
        return -1;
    }
    return 0;
}

int copy_symlink(const char* src_path, const char* dest_path, const char* root_src, const char* root_dest)
{
    char target_buf[PATH_MAX];

    ssize_t len = get_symlink_target(src_path, target_buf, sizeof(target_buf));
    if (len < 0)
        return -1;

    char final_target[PATH_MAX];
    size_t root_src_len = strlen(root_src);

    if (target_buf[0] == '/' && strncmp(target_buf, root_src, root_src_len) == 0)
    {
        if (target_buf[root_src_len] == '/' || target_buf[root_src_len] == '\0')
        {
            int needed = snprintf(final_target, PATH_MAX, "%s%s", root_dest, target_buf + root_src_len);

            if (needed >= PATH_MAX)
            {
                fprintf(stderr, "Błąd: Ścieżka zbyt długa\n");
                return -1;
            }
        }
        else
        {
            strncpy(final_target, target_buf, PATH_MAX);
        }
    }
    else
    {
        strncpy(final_target, target_buf, PATH_MAX);
    }

    return create_symlink(final_target, dest_path);
}

int remove_any_path(const char* path)
{
    struct stat st;

    if (TEMP_FAILURE_RETRY(lstat(path, &st)) < 0)
    {
        if (errno == ENOENT)
            return 0;
        return -1;
    }

    if (S_ISDIR(st.st_mode))
    {
        DIR* dir = opendir(path);
        if (!dir)
        {
            perror("opendir failed");
            return -1;
        }

        struct dirent* entry;
        int ret = 0;

        while ((entry = readdir(dir)) != NULL)
        {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            {
                continue;
            }

            char full_path[PATH_MAX];
            int needed = snprintf(full_path, PATH_MAX, "%s/%s", path, entry->d_name);

            if (needed < 0 || needed >= PATH_MAX)
            {
                fprintf(stderr, "Ścieżka zbyt długa: %s/%s\n", path, entry->d_name);
                ret = -1;
                break;
            }

            // Rekurencyjne usuwanie
            if (remove_any_path(full_path) < 0)
            {
                ret = -1;
                break;
            }
        }
        closedir(dir);

        if (ret != 0)
            return -1;

        if (TEMP_FAILURE_RETRY(rmdir(path)) < 0)
        {
            perror("rmdir failed");
            return -1;
        }
    }
    else
    {
        if (TEMP_FAILURE_RETRY(unlink(path)) < 0)
        {
            perror("unlink failed");
            return -1;
        }
    }
    return 0;
}

int copy_recursive(const char* src_path, const char* dest_path, const char* root_src, const char* root_dest)
{
    if (make_directory(dest_path) < 0)
    {
        return -1;
    }

    DIR* dir = opendir(src_path);
    if (!dir)
    {
        perror("opendir failed in copy_recursive");
        return -1;
    }

    struct dirent* entry;
    int status = 0;

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }
        char full_src[PATH_MAX];
        char full_dest[PATH_MAX];

        snprintf(full_src, PATH_MAX, "%s/%s", src_path, entry->d_name);
        snprintf(full_dest, PATH_MAX, "%s/%s", dest_path, entry->d_name);

        struct stat st;
        if (TEMP_FAILURE_RETRY(lstat(full_src, &st)) < 0)
        {
            perror("lstat failed");
            status = -1;
            continue;  // próba kopiowania kolejnych plików
        }

        if (S_ISDIR(st.st_mode))  // katalog
        {
            if (copy_recursive(full_src, full_dest, root_src, root_dest) < 0)
            {
                status = -1;
            }
        }
        else if (S_ISLNK(st.st_mode))  // symlink
        {
            if (copy_symlink(full_src, full_dest, root_src, root_dest) < 0)
            {
                status = -1;
            }
        }
        else if (S_ISREG(st.st_mode))  // zwykly plik
        {
            if (copy_regular_file(full_src, full_dest) < 0)
            {
                status = -1;
            }
        }
    }
    closedir(dir);
    return status;
}
