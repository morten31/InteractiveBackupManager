#ifndef WATCHER
#define WATCHER

typedef struct WatchNode
{
    int wd;
    char* path;
    struct WatchNode* next;
} WatchNode;

void watchlist_add(WatchNode** head, int wd, const char* path);
char* watchlist_get_path(WatchNode* head, int wd);
void watchlist_free_all(WatchNode* head);

#endif
