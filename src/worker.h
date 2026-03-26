#ifndef WORKER
#define WORKER

void build_dest_path(const char* src_full_path, const char* root_src, const char* root_dest, char* dest_buffer);

// Główna funkcja workera
void run_backup_process(const char* source, const char* target);

#endif
