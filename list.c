
#include "list.h"
#include "ll.h"
#include "_malloc.h"


#define PROLOGUE() \
	struct malloc_context* prevCtx = getCurrentMallocContext(); \
	setMallocContext(list->malloc_ctx); 
	
#define EPILOGUE() \
	setMallocContext(prevCtx); \
	return ret;


int llist_len(struct llist *list)
{
	return list->list->len;
}


int llist_remove(struct llist* list, void* val)
{
	FOREACH_LIST((*list))
	{
		if (entry == val)
		{
			// man hat es gefunden
			llist_remove_n(list, i);
			return 1;
		}
	}
	return 0;
}

// new ist speziell
struct llist llist_new(struct malloc_context* malloc_ctx)

{
	struct llist list;
	list.malloc_ctx = malloc_ctx;
	
	struct malloc_context* prevCtx = getCurrentMallocContext();
	setMallocContext(malloc_ctx);
	
	list.list = ll_new(NULL);	// nein, darf nicht sein Funkion muss fuer beide da sein
								// muss also NULL sein, NOP geht nicht
	
	setMallocContext(prevCtx);
	return list;
}


void llist_delete(struct llist *list)
{
	PROLOGUE();
	ll_delete(list->list);
	int ret = 0;
	EPILOGUE();
}


int llist_insert_n(struct llist *list, void *val, int n)
{
	PROLOGUE();
	int ret = ll_insert_n(list->list, val, n);
	EPILOGUE();
}

int llist_insert_first(struct llist *list, void *val)
{
	PROLOGUE();
	int ret = ll_insert_first(list->list, val);
	EPILOGUE();
}

int llist_insert_last(struct llist *list, void *val)
{
	PROLOGUE();
	int ret = ll_insert_last(list->list, val);
	EPILOGUE();
}

int llist_remove_n(struct llist *list, int n)
{
	PROLOGUE();
	int ret = ll_remove_n(list->list, n);
	EPILOGUE();
}

int llist_remove_first(struct llist *list)
{
	PROLOGUE();
	int ret = ll_remove_first(list->list);
	EPILOGUE();
}

int llist_remove_search(struct llist *list, int cond(void *))
{
	PROLOGUE();
	int ret = ll_remove_search(list->list, cond);
	EPILOGUE();
}

void *llist_get_n(struct llist *list, int n)
{
	PROLOGUE();
	void* ret = ll_get_n(list->list, n);
	EPILOGUE();
}

void *llist_get_first(struct llist *list)
{
	PROLOGUE();
	void* ret = ll_get_first(list->list);
	EPILOGUE();
}

void llist_map(struct llist *list, gen_fun_t f)
{
	PROLOGUE();
	int ret = 0;
	ll_map(list->list, f);
	EPILOGUE();
}


void llist_print(struct llist list_)
{
	struct llist* list = &list_;
	PROLOGUE();	
	ll_print(*list_.list);
	int ret = 0;
	EPILOGUE();
}


void llist_no_teardown(void *n)
{
	ll_no_teardown(n);
	
}

