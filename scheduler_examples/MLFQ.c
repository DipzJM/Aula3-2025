#include "MLFQ.h"

#include <stdio.h>
#include <stdlib.h>

#include "msg.h"
#include <unistd.h>

/**
 * @brief First-In-First-Out (FIFO) scheduling algorithm.
 *
 * This function implements the FIFO scheduling algorithm. If the CPU is not idle it
 * checks if the application is ready and frees the CPU.
 * If the CPU is idle, it selects the next task to run based on the order they were added
 * to the ready queue. The task that has been in the queue the longest is selected to run next.
 *
 * @param current_time_ms The current time in milliseconds.
 * @param rq Pointer to the ready queue containing tasks that are ready to run.
 * @param cpu_task Double pointer to the currently running task. This will be updated
 *                 to point to the next task to run.
 */
void mlfq_scheduler(uint32_t current_time_ms,mlfq_t *mq, pcb_t **cpu_task) {
    if (*cpu_task) {
        (*cpu_task)->ellapsed_time_ms += TICKS_MS;// Add to the running time of the application/task

        (*cpu_task)->slice_time += TICKS_MS;

        int nvl = (*cpu_task)->priority_level;

        if ((*cpu_task)->ellapsed_time_ms >= (*cpu_task)->time_ms) {
            // Task finished
            // Send msg to application
            msg_t msg = {
                .pid = (*cpu_task)->pid,
                .request = PROCESS_REQUEST_DONE,
                .time_ms = current_time_ms
            };
            if (write((*cpu_task)->sockfd, &msg, sizeof(msg_t)) != sizeof(msg_t)) {
                perror("write");
            }
            // Application finished and can be removed (this is FIFO after all)
            free((*cpu_task));
            (*cpu_task) = NULL;
        }
        else if ((*cpu_task)->slice_time >= mq->time_slices[nvl]) {
            (*cpu_task)->slice_time = 0;
            if (nvl < mq->niveis - 1) {
                (*cpu_task)->priority_level = nvl + 1; // desce de nível
            }
            enqueue_pcb(mq->queues[(*cpu_task)->priority_level], *cpu_task);
            *cpu_task = NULL;
        }
    }
    if (*cpu_task == NULL) {            // If CPU is idle
        for (int i = 0; i < mq->niveis; i++) {
            *cpu_task = dequeue_pcb(mq->queues[i]);
            if (*cpu_task) {
                (*cpu_task)->slice_time = 0;
                break;
            }
        }
    }
}


// TODO: Create this function
mlfq_t *create_mlfq() {
    return NULL;
};