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
void mlfq_scheduler(uint32_t current_time_ms,mlfq_t *mq, pcb_t **cpu_task) {  // recebe o tempo atual, um ponteiro para a estrutura MLFQ e um ponteiro duplo para o processo atualmente em execução
    if (*cpu_task) {  // verifica se existe uma tarefa atualmente sendo executada pelo processador
        (*cpu_task)->ellapsed_time_ms += TICKS_MS;// Add to the running time of the application/task

        (*cpu_task)->slice_time += TICKS_MS;  // Atualiza o tempo total de execução da tarefa e o tempo de fatia (slice) que ela já usou.

        int nvl = (*cpu_task)->priority_level;  // Salva o nível de prioridade atual da tarefa

        if ((*cpu_task)->ellapsed_time_ms >= (*cpu_task)->time_ms) {  // Se o tempo de execução da tarefa atingiu ou ultrapassou o tempo total necessário, ela terminou
            // Task finished
            // Send msg to application
            msg_t msg = {    // Cria uma mensagem para informar que o processo terminou
                .pid = (*cpu_task)->pid,   // ppid do processo concluido
                .request = PROCESS_REQUEST_DONE,  // indica o fim do processo
                .time_ms = current_time_ms   // tempo de conclusao
            };
            if (write((*cpu_task)->sockfd, &msg, sizeof(msg_t)) != sizeof(msg_t)) {
                perror("write");
            } // cria uma mensagem informando que o processo terminou e envia para o aplicativo associado.
            // Application finished and can be removed (this is FIFO after all)
            free((*cpu_task));
            (*cpu_task) = NULL;
        } // Libera a memória ocupada pela tarefa e indica que o processador está livre.
        else if ((*cpu_task)->slice_time >= mq->time_slices[nvl]) {
            (*cpu_task)->slice_time = 0;   // Se a tarefa usou toda sua fatia de tempo, ela pode ser rebaixada para um nível de prioridade inferior (se não estiver no último nível).
            if (nvl < mq->niveis - 1) {
                (*cpu_task)->priority_level = nvl + 1; // desce de nível
            }
            enqueue_pcb(mq->queues[(*cpu_task)->priority_level], *cpu_task);
            *cpu_task = NULL;     // A tarefa é colocada de volta na fila do novo nível de prioridade e o processador fica livre.
        }
    }
    if (*cpu_task == NULL) { // Se o processador está livre, percorre as filas de prioridade (da mais alta para a mais baixa) e seleciona a próxima tarefa disponível.
        for (int i = 0; i < mq->niveis; i++) {
            *cpu_task = dequeue_pcb(mq->queues[i]);  // Quando encontra uma tarefa, zera o tempo de fatia dela e a coloca para execução.
            if (*cpu_task) {
                (*cpu_task)->slice_time = 0;
                break;
            }
        }
    }
}