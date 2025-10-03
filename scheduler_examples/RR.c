#include "RR.h"

#include <stdio.h>
#include <stdlib.h>

#include "msg.h"
#include <unistd.h>

#define TIME_SLICE_MS 500

/**
 * @brief RR (Round-Robin) scheduling algorithm.
 *
 * This function implements the RR scheduling algorithm. If the CPU is not idle it
 * checks if the application is ready and frees the CPU.
 * If the CPU is idle, it selects the next task to run based on the order they were added
 * to the ready queue. The task that has been in the queue the longest is selected to run next.
 *
 * @param current_time_ms The current time in milliseconds.
 * @param rq Pointer to the ready queue containing tasks that are ready to run.
 * @param cpu_task Double pointer to the currently running task. This will be updated
 *                 to point to the next task to run.
 */
void rr_scheduler(uint32_t current_time_ms, queue_t *rq, pcb_t **cpu_task) { // Função de escalonamento RR. Recebe tempo atual, fila de prontos e ponteiro duplo para a tarefa no CPU
    if (*cpu_task) {   // se existe uma tarefa em execução
        (*cpu_task)->ellapsed_time_ms += TICKS_MS;  // Incrementa o tempo já executado do processo
        (*cpu_task)->slice_time += TICKS_MS;  // Incrementa o tempo de fatia (quantum) já usado pela tarefa
        if ((*cpu_task)->ellapsed_time_ms >= (*cpu_task)->time_ms) {  // Se o tempo executado atingiu o necessário
            msg_t msg = {    // Monta uma mensagem para sinalizar término
                .pid = (*cpu_task)->pid,    // PID do processo concluído
                .request = PROCESS_REQUEST_DONE,  // Indica fim do processo
                .time_ms = current_time_ms   // Tempo de conclusão
            };
            if (write((*cpu_task)->sockfd, &msg, sizeof(msg_t)) != sizeof(msg_t)) {   // Envia a mensagem por socket
                perror("write");
            }

            free((*cpu_task));     // Libera a memória do processo finalizado
            (*cpu_task) = NULL;    // Marca que não há mais tarefa rodando
        }
        else if((*cpu_task)->slice_time >= TIME_SLICE_MS) {  // Se a tarefa usou toda sua fatia de tempo (quantum)
            (*cpu_task)->slice_time = 0;    // Zera o contador de fatia da tarefa
            enqueue_pcb(rq,*cpu_task);    // Reinsere a tarefa no final da fila de prontos
            *cpu_task = NULL;      // Libera o processador
        }
    }
    if (*cpu_task == NULL) {    // Se o processador está livre (nenhuma tarefa rodando)
        *cpu_task = dequeue_pcb(rq);  // Retira o próximo processo da fila de prontos e coloca na CPU
        if (*cpu_task) {     // Se encontrou uma tarefa para rodar
            (*cpu_task)->slice_time = 0;   // Zera o contador de fatia da nova tarefa
        }
    }
}