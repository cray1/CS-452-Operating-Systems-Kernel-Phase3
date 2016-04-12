/* This was created by Christopher Ray */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "queue_ll.h"
#include <usloss.h>
#include <phase1.h>

Queue_ll queue_ll_init() {
	node q = malloc(sizeof(node_t));
	q->next = q->prev = 0;
	return q;
}

int queue_ll_getSize(Queue_ll q) {
	int size = 0;
	node h = HEAD(q);
	while (1) {
		if (!h)
			break;
		h = h->next;
		size++;
	}
	return size;
}

int queue_ll_isEmpty(Queue_ll q) {
	return !HEAD(q);
}

void queue_ll_enqueue(Queue_ll q, void* n) {
	node nd = malloc(sizeof(node_t));
	nd->val = n;
	if (!HEAD(q))
		HEAD(q) = nd;
	nd->prev = TAIL(q);
	if (nd->prev)
		nd->prev->next = nd;
	TAIL(q) = nd;
	nd->next = 0;
}

int queue_ll_dequeue(Queue_ll q, void **val) {

	node tmp = HEAD(q);
	if (!tmp)
		return 0;
	*val = tmp->val;

	HEAD(q) = tmp->next;
	if (TAIL(q) == tmp)
		TAIL(q) = 0;
	free(tmp);

	return 1;
}
void queue_ll_dequeue_all(Queue_ll q) {
	void *temp;
	while (queue_ll_dequeue(q, &temp) != 0) {
		if (!HEAD(q)) {
			break;
		}
	}
}

void queue_ll_print(Queue_ll q, int interpretAsInts) {
	node h = HEAD(q);
	while (1) {
		if (!h)
			break;
		if (interpretAsInts == 1)
			USLOSS_Console("%d | ", (int) h->val);
		else
			USLOSS_Console("%s | ", (char*) h->val);
		h = h->next;
	}
	USLOSS_Console("\n");
}

void* queue_ll_tail_value(Queue_ll q) {
	if (TAIL(q)) {
		return TAIL(q)->val;
	}
	return (void *) -1;
}

void* queue_ll_head_value(Queue_ll q) {
	if (HEAD(q)) {
		return HEAD(q)->val;
	}
	return (void *) -1;
}

Queue_ll queue_ll_tail_node(Queue_ll q) {
	if (TAIL(q)) {
		return TAIL(q);
	}
	return NULL;
}
