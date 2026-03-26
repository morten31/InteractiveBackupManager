#define _XOPEN_SOURCE 700
#include "watcher.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void watchlist_add(WatchNode** head, int wd, const char* path)
{
    WatchNode* node = malloc(sizeof(WatchNode));
    node->wd = wd;
    node->path = strdup(path);
    node->next = *head;
    *head = node;
}

char* watchlist_get_path(WatchNode* head, int wd)
{
    WatchNode* curr = head;
    while (curr)
    {
        if (curr->wd == wd)
            return curr->path;
        curr = curr->next;
    }
    return NULL;
}

void watchlist_free_all(WatchNode* head)
{
    while (head)
    {
        WatchNode* temp = head;
        head = head->next;
        free(temp->path);
        free(temp);
    }
}
