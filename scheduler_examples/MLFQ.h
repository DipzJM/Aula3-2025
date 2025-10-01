#ifndef MLFQ_H
#define MLFQ_H
#define NIVEIS_MLFQ 3
#include "queue.h"

typedef struct {
    queue_t *queues[NIVEIS_MLFQ];
    uint32_t time_slices[NIVEIS_MLFQ];
    int niveis;
} mlfq_t;


void mlfq_scheduler(uint32_t current_time_ms,mlfq_t *mq, pcb_t **cpu_task);



#endif //MLFQ_H
