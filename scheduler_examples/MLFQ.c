#include "MLFQ.h"

#include <stdio.h>
#include <stdlib.h>

#include "msg.h"
#include <unistd.h>

/**
 * @brief
 *
*  O MLFQ é um algoritmo de escalonamento de processos que utiliza várias filas de prioridades.

   Processos começam em filas de prioridade mais alta e, se usarem muito tempo de CPU, descem para filas de prioridade mais baixa.

   Este código já tem parte da lógica de execução e rebaixamento de processos, mas a função de inicialização (create_mlfq) ainda não está implementada.

* @param current_time_ms The current time in milliseconds.
 * @param rq Pointer to the ready queue containing tasks that are ready to run.
 * @param cpu_task Double pointer to the currently running task. This will be updated
 *                 to point to the next task to run.
 */
void mlfq_scheduler(uint32_t current_time_ms,mlfq_t *mq, pcb_t **cpu_task) {   // Define a função principal do escalonador
    if (*cpu_task) {  // Verifica se há um processo em execução na CPU
        (*cpu_task)->ellapsed_time_ms += TICKS_MS; // Adiciona o tempo de CPU utilizado
        (*cpu_task)->slice_time += TICKS_MS;  // Incrementa o tempo gasto na fatia atual.

        int nvl = (*cpu_task)->priority_level;   // Guarda o nível de prioridade atual

        if ((*cpu_task)->ellapsed_time_ms >= (*cpu_task)->time_ms) {  // Se o tempo total de execução atingir ou ultrapassar o tempo requerido
            msg_t msg = {
                .pid = (*cpu_task)->pid,    //  identifica o processo
                .request = PROCESS_REQUEST_DONE,   // indica que é um pedido de término
                .time_ms = current_time_ms   // marca o tempo atual
            };
            if (write((*cpu_task)->sockfd, &msg, sizeof(msg_t)) != sizeof(msg_t)) {  // Envia a mensagem pelo socket associado ao processo
                perror("write");
            }
            free((*cpu_task));   // Libera memória do PCB
            (*cpu_task) = NULL;   // CPU fica desocupado
        }
        else if ((*cpu_task)->slice_time >= mq->time_slices[nvl]) {     // Caso o processo não tenha terminado mas tenha usado todo seu time slice
            (*cpu_task)->slice_time = 0;   // Reinicia contador da fatia
            if (nvl < mq->niveis - 1) {
                (*cpu_task)->priority_level = nvl + 1; // desce de nível/prioridade
            }
            enqueue_pcb(mq->queues[(*cpu_task)->priority_level], *cpu_task);   // Recoloca o processo na fila de acordo com seu novo nível
            *cpu_task = NULL;
        }
    }
    if (*cpu_task == NULL) {         // Se CPU está livre
        for (int i = 0; i < mq->niveis; i++) {
            *cpu_task = dequeue_pcb(mq->queues[i]);  // processo da fila mais prioriatria
            if (*cpu_task) {
                (*cpu_task)->slice_time = 0;  // reinicia slice
                break;     // sai do loop
            }
        }
    }
}


// TODO: Create this function
mlfq_t *create_mlfq() {
    return NULL;
};