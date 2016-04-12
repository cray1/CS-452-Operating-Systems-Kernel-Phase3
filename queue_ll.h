/* This was created by Christopher Ray */

#ifndef QUEUE_LL_H_
#define QUEUE_LL_H_
typedef struct node_t node_t, *node, *Queue_ll;
struct node_t {
	void* val;
	node prev, next;
};

#define HEAD(q) q->prev
#define TAIL(q) q->next

Queue_ll queue_ll_init();
int queue_ll_isEmpty(Queue_ll q);
void queue_ll_enqueue(Queue_ll q,  void *n);
int queue_ll_dequeue(Queue_ll q, void **val);
void queue_ll_print(Queue_ll q, int interpretAsInts);
void* queue_ll_tail_value(Queue_ll q);
void* queue_ll_head_value(Queue_ll q);
void queue_ll_dequeue_all(Queue_ll q);
int queue_ll_getSize(Queue_ll q);
Queue_ll queue_ll_tail_node(Queue_ll q);

#endif /* QUEUE_LL_H_ */
