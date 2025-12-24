#ifndef STRLIST_H
#define STRLIST_H


#include <stddef.h>




typedef struct{
  char **data; 
  size_t size;
  size_t cap;
} StrList ;


void strlist_init(StrList *l);
void strlist_free(StrList *l);


/*Methode*/

int  strlist_add(StrList *l, const char *s);              // adds a copy (strdup)
int  strlist_add_take(StrList *l, char *owned_string);    // takes ownership (no strdup)
const char *strlist_get(const StrList *l, size_t i);      // safe getter
int  strlist_contains(const StrList *l, const char *s);   // strcmp
int  strlist_add_unique(StrList *l, const char *s);       // add if not exists

#endif // !STRLIST_H

