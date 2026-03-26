#ifndef FILE_MANAGER
#define FILE_MANAGER

#include <sys/types.h>
#include <unistd.h>

// Rozmiar bufora kopiowania
#define COPY_BUFFER_SIZE 8192

// Funkcje z wykładu
ssize_t bulk_read(int fd, char* buf, size_t count);
ssize_t bulk_write(int fd, char* buf, size_t count);

// Kopiuje zawartość, uprawnienia i czasy pliku z src do dest
int copy_regular_file(const char* src_path, const char* dest_path);

// Tworzy katalog
int make_directory(const char* path);

// Usuwa rekurencyjnie dowolny plik lub katalog
int remove_any_path(const char* path);

// Odczytuje target symlinku do bufora.
ssize_t get_symlink_target(const char* path, char* buffer, size_t size);

// Tworzy symlink w dest_path wskazujący na target
int create_symlink(const char* target, const char* dest_path);

// Kopiuje symlink
int copy_symlink(const char* src_path, const char* dest_path, const char* root_src, const char* root_dest);

// Rekurencyjnie kopiuje strukture katalogow
int copy_recursive(const char* src_path, const char* dest_path, const char* root_src, const char* root_dest);

#endif
