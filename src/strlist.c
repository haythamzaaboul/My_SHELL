#include "strlist.h"


#include <stdlib.h>
#include <string.h>

static int strlist_grow(StrList *l, size_t min_cap) {
    size_t new_cap = (l->cap == 0) ? 8 : l->cap;
    while (new_cap < min_cap) new_cap *= 2;

    char **p = (char **)realloc(l->data, new_cap * sizeof(char *));
    if (!p) return -1;

    l->data = p;
    l->cap  = new_cap;
    return 0;
}

void strlist_init(StrList *l) {
    l->data = NULL;
    l->size = 0;
    l->cap  = 0;
}

void strlist_free(StrList *l) {
    if (!l) return;
    for (size_t i = 0; i < l->size; i++) {
        free(l->data[i]);
    }
    free(l->data);
    l->data = NULL;
    l->size = 0;
    l->cap  = 0;
}

int strlist_add_take(StrList *l, char *owned_string) {
    if (!l || !owned_string) return -1;

    if (l->size + 1 > l->cap) {
        if (strlist_grow(l, l->size + 1) != 0) return -1;
    }
    l->data[l->size++] = owned_string;
    return 0;
}

int strlist_add(StrList *l, const char *s) {
    if (!l || !s) return -1;
    char *copy = strdup(s);
    if (!copy) return -1;
    return strlist_add_take(l, copy);
}

const char *strlist_get(const StrList *l, size_t i) {
    if (!l || i >= l->size) return NULL;
    return l->data[i];
}

int strlist_contains(const StrList *l, const char *s) {
    if (!l || !s) return 0;
    for (size_t i = 0; i < l->size; i++) {
        if (strcmp(l->data[i], s) == 0) return 1;
    }
    return 0;
}

int strlist_add_unique(StrList *l, const char *s) {
    if (strlist_contains(l, s)) return 0;
    return strlist_add(l, s);
}
