

#ifndef __LLIST_H_
#define __LLIST_H_


#include "ll.h"
#include "_malloc.h"

/*
Ein Wrapper, denn man muss immer noch angeben, bei welchem Interface man gerade den Speicher allocatet
*/


// um die Liste durchzulaufen und entries zu kriegen
#define FOREACH_LIST(list_name) \
	void* entry; int i; \
	for(i = 0, entry = llist_get_n(&list_name, 0); \
	entry != NULL; \
	i++, entry = llist_get_n(&list_name, i))
	
#define hasDataList(list) (llist_len(list) > 0)

struct llist
{
	struct ll* list;
	struct malloc_context* malloc_ctx;
};

// Funktionen

// lange der Liste
int llist_len(struct llist *list);

// entfernt das 1. Element mit val aus der Liste oder gibt 0 zurueck
int llist_remove(struct llist* list, void* val);

// new ist speziell
struct llist llist_new(struct malloc_context* malloc_ctx);


void llist_delete(struct llist *list);


int llist_insert_n(struct llist *list, void *val, int n);

int llist_insert_first(struct llist *list, void *val);

int llist_insert_last(struct llist *list, void *val);

int llist_remove_n(struct llist *list, int n);

int llist_remove_first(struct llist *list);

int llist_remove_search(struct llist *list, int cond(void *));

void *llist_get_n(struct llist *list, int n);

void *llist_get_first(struct llist *list);


void llist_map(struct llist *list, gen_fun_t f);


void llist_print(struct llist list);


void llist_no_teardown(void *n);

#endif

