#include "queue.h"

#include <stdio.h>
#include <stdlib.h>

pcb_t *new_pcb(pid_t pid, uint32_t sockfd, uint32_t time_ms) {
    pcb_t * new_task = malloc(sizeof(pcb_t));
    if (!new_task) return NULL;

    new_task->pid = pid;
    new_task->status = TASK_COMMAND;
    new_task->slice_start_ms = 0;
    new_task->sockfd = sockfd;
    new_task->time_ms = time_ms;
    new_task->ellapsed_time_ms = 0;
    return new_task;
}

int enqueue_pcb(queue_t* q, pcb_t* task) {
    queue_elem_t* elem = malloc(sizeof(queue_elem_t));
    if (!elem) return 0;

    elem->pcb = task;
    elem->next = NULL;

    if (q->tail) {
        q->tail->next = elem;
    } else {
        q->head = elem;
    }
    q->tail = elem;
    return 1;
}

pcb_t* dequeue_pcb(queue_t* q) {
    if (!q || !q->head) return NULL;

    queue_elem_t* node = q->head;
    pcb_t* task = node->pcb;

    q->head = node->next;
    if (!q->head)
        q->tail = NULL;

    free(node);
    return task;
}



pcb_t* dequeue_short(queue_t* q) {
    if (!q || !q->head) return NULL;

    queue_elem_t *prev = NULL, *curr = q->head;
    queue_elem_t *min_prev = NULL, *min_node = q->head;

    //Percorre a lista á procura do com o menor tempo
    while (curr) {
        if (curr->pcb->time_ms < min_node->pcb->time_ms) {
            min_node = curr;
            min_prev = prev;
        }
        prev = curr;
        curr = curr->next;
    }

    queue_elem_t *next_task = remove_queue_elem(q, min_node);
    pcb_t *res = next_task->pcb;
    free(next_task);
    return res;

}



queue_elem_t *remove_queue_elem(queue_t* q, queue_elem_t* elem) {
    queue_elem_t* it = q->head;
    queue_elem_t* prev = NULL;
    while (it != NULL) {
        if (it == elem) {
            // Remove elem from queue
            if (prev) {
                prev->next = it->next;
            } else {
                q->head = it->next;
            }
            if (it == q->tail) {
                q->tail = prev;
            }
            return it;
        }
        prev = it;
        it = it->next;
    }
    printf("Queue element not found in queue\n");
    return NULL;
}