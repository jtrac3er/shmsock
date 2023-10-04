
#include "list.h"
#include "custom-malloc/_malloc.h"
#include <stdio.h>

int main()
{
	char region1[0x2000], region2[0x2000];

	struct malloc_context c1 = createMallocContext(region1, sizeof(region1));
	struct malloc_context c2 = createMallocContext(region2, sizeof(region2));
	
	test(&c1);
	test(&c2);
    
    return 0;
}



/* this following code is just for testing this library */

void num_teardown(void *n) {
    *(int *)n *= -1; // just so we can visually inspect removals afterwards
}

void num_printer(void *n) {
    printf(" %d", *(int *)n);
}

int num_equals_3(void *n) {
    return *(int *)n == 3;
}


int test(struct malloc_context* ctx) {
    int *_n; // for storing returned ones
    int test_count = 1;
    int fail_count = 0;
    int a = 0;
    int b = 1;
    int c = 2;
    int d = 3;
    int e = 4;
    int f = 5;
    int g = 6;
    int h = 3;
    int i = 3;

	struct llist list_s = llist_new(ctx);
	struct llist* list = &list_s;
	
    //llist_t *list = ll_new(num_teardown);
    list->list->val_printer = num_printer;

    llist_insert_first(list, &c); // 2 in front

    _n = (int *)llist_get_first(list);
    if (!(*_n == c)) {
        fprintf(stderr, "FAIL Test %d: Expected %d, but got %d.\n", test_count, c, *_n);
        fail_count++;
    } else
        fprintf(stderr, "PASS Test %d!\n", test_count);
    test_count++;

    if (list->list->len != 1) {
        fprintf(stderr, "FAIL Test %d: Expected %d, but got %d.\n", test_count, 1, list->list->len);
        fail_count++;
    } else
        fprintf(stderr, "PASS Test %d!\n", test_count);
    test_count++;

    llist_insert_first(list, &b); // 1 in front
    llist_insert_first(list, &a); // 0 in front -> 0, 1, 2

    _n = (int *)llist_get_first(list);
    if (!(*_n == a)) {
        fprintf(stderr, "FAIL Test %d: Expected %d, but got %d.\n", test_count, a, *_n);
        fail_count++;
    } else
        fprintf(stderr, "PASS Test %d!\n", test_count);
    test_count++;

    if (!(list->list->len == 3)) {
        fprintf(stderr, "FAIL Test %d: Expected %d, but got %d.\n", test_count, 3, list->list->len);
        fail_count++;
    } else
        fprintf(stderr, "PASS Test %d!\n", test_count);
    test_count++;

    llist_insert_last(list, &d); // 3 in back
    llist_insert_last(list, &e); // 4 in back
    llist_insert_last(list, &f); // 5 in back

    _n = (int *)llist_get_n(list, 5);
    if (!(*_n == f)) {
        fprintf(stderr, "FAIL Test %d: Expected %d, but got %d.\n", test_count, f, *_n);
        fail_count++;
    } else
        fprintf(stderr, "PASS Test %d!\n", test_count);
    test_count++;

    if (!(list->list->len == 6)) {
        fprintf(stderr, "FAIL Test %d: Expected %d, but got %d.\n", test_count, 6, list->list->len);
        fail_count++;
    } else
        fprintf(stderr, "PASS Test %d!\n", test_count);
    test_count++;

    llist_insert_n(list, &g, 6); // 6 at index 6 -> 0, 1, 2, 3, 4, 5, 6

    int _i;
    for (_i = 0; _i < list->list->len; _i++) { // O(n^2) test lol
        _n = (int *)llist_get_n(list, _i);
        if (!(*_n == _i)) {
            fail_count++;
            fprintf(stderr, "FAIL Test %d: Expected %d, but got %d.\n", 1, _i, *_n);
        } else
            fprintf(stderr, "PASS Test %d!\n", test_count);
        test_count++;
    }

    // (ll: 0 1 2 3 4 5 6), length: 7

    llist_remove_first(list);                // (ll: 1 2 3 4 5 6), length: 6
    llist_remove_n(list, 1);                 // (ll: 1 3 4 5 6),   length: 5
    llist_remove_n(list, 2);                 // (ll: 1 3 5 6),     length: 4
    llist_remove_n(list, 5);                 // (ll: 1 3 5 6),     length: 4; does nothing
    llist_remove_search(list, num_equals_3); // (ll: 1 5 6),       length: 3
    llist_insert_first(list, &h);            // (ll: 3 1 5 6),     length: 5
    llist_insert_last(list, &i);             // (ll: 3 1 5 6 3),   length: 5
    llist_remove_search(list, num_equals_3); // (ll: 1 5 6 3),     length: 4
    llist_remove_search(list, num_equals_3); // (ll: 1 5 6),       length: 3

    llist_print(*list);

    llist_delete(list);

    if (fail_count) {
        fprintf(stderr, "FAILED %d tests of %d.\n", fail_count, test_count);
        return fail_count;
    }

    fprintf(stderr, "PASSED all %d tests!\n", test_count);
}
