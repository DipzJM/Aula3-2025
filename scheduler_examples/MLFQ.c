#include "MLFQ.h"
#include <stdio.h>
#include <stdlib.h>
#include "msg.h"
#include <unistd.h>

mlfq_t *create_mlfq() {
    mlfq_t *mq = malloc(sizeof(mlfq_t));
    if (!mq) {
        perror("malloc mlfq");
        return NULL;
    }
    mq->niveis = NIVEIS_MLFQ;
    mq->time_slices[0] = 100;
    mq->time_slices[1] = 200;
    mq->time_slices[2] = 400;
    for (int i = 0; i < NIVEIS_MLFQ; i++) {
        mq->queues[i] = malloc(sizeof(queue_t));
        if (!mq->queues[i]) {
            perror("malloc queue");
            for (int j = 0; j < i; j++) free(mq->queues[j]);
            free(mq);
            return NULL;
        }
        mq->queues[i]->head = NULL;
        mq->queues[i]->tail = NULL;
    }
    return mq;
}


/**
 * @brief MLFQ (Multi-Level Feedback Queue) scheduling algorithm.
 *
 * O escalonador MLFQ utiliza várias filas com diferentes prioridades e time slices.
 * - Tarefas novas inserem-se sempre na fila de prioridade mais alta.
 * - Se não concluírem dentro do timeslice, descem de nível (prioridade menor).
 * - O CPU executa sempre tarefas da fila de prioridade mais alta disponível.
 *
 * @param current_time_ms Tempo atual em ms.
 * @param mq Ponteiro para a estrutura de filas MLFQ.
 * @param cpu_task Duplo ponteiro para a tarefa atualmente em execução.
 */
void mlfq_scheduler(uint32_t current_time_ms, mlfq_t *mq, pcb_t **cpu_task) {
    if (*cpu_task) {
        (*cpu_task)->ellapsed_time_ms += TICKS_MS;
        (*cpu_task)->slice_time += TICKS_MS;
        int curr_level = (*cpu_task)->priority_level;

        // Verifica se a tarefa terminou
        if ((*cpu_task)->ellapsed_time_ms >= (*cpu_task)->time_ms) {
            msg_t msg = {
                    .pid = (*cpu_task)->pid,
                    .request = PROCESS_REQUEST_DONE,
                    .time_ms = current_time_ms
            };
            if (write((*cpu_task)->sockfd, &msg, sizeof(msg_t)) != sizeof(msg_t)) {
                perror("write");
            }else {
                printf("[Scheduler] Envia DONE para a tarefa PID %d no tempo %u ms\n", (*cpu_task)->pid, current_time_ms);
            }
            free(*cpu_task);
            *cpu_task = NULL;
        }
            // Verifica se excedeu o timeslice do seu nível
        else if ((*cpu_task)->slice_time >= mq->time_slices[curr_level]) {
            (*cpu_task)->slice_time = 0;
            // Desce de nível se não for o último
            if (curr_level < mq->niveis - 1) {
                (*cpu_task)->priority_level = curr_level + 1;
            }
            enqueue_pcb(mq->queues[(*cpu_task)->priority_level], *cpu_task);
            *cpu_task = NULL;
        }
    }
    // Seleciona sempre a tarefa da fila de maior prioridade disponível
    if (*cpu_task == NULL) {
        for (int i = 0; i < mq->niveis; i++) {
            *cpu_task = dequeue_pcb(mq->queues[i]);
            if (*cpu_task) {
                // Reinicia o tempo de slice sempre que uma tarefa entra no CPU
                (*cpu_task)->slice_time = 0;
                break;
            }
        }
    }
}
