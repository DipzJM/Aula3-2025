#include "fifo.h"

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
void fifo_scheduler(uint32_t current_time_ms, queue_t *rq, pcb_t **cpu_task) {  // Função de escalonamento FIFO. Recebe tempo atual, fila de prontos e ponteiro duplo para a tarefa no CPU
    if (*cpu_task) {      // Se existe uma tarefa em execução
        (*cpu_task)->ellapsed_time_ms += TICKS_MS;    // Incrementa o tempo já executado do processo
        if ((*cpu_task)->ellapsed_time_ms >= (*cpu_task)->time_ms) {  // Se o tempo executado atingiu o necessário
            msg_t msg = {   // Monta uma mensagem para sinalizar término
                .pid = (*cpu_task)->pid,  // PID do processo concluído
                .request = PROCESS_REQUEST_DONE,   // Indica fim do processo
                .time_ms = current_time_ms   // Tempo de conclusão
            };
            if (write((*cpu_task)->sockfd, &msg, sizeof(msg_t)) != sizeof(msg_t)) {  // Envia a mensagem por socket
                perror("write");
            }
            // Application finished and can be removed (this is FIFO after all)
            free((*cpu_task));   // Libera a memória do processo finalizado
            (*cpu_task) = NULL;  // Marca que não há mais tarefa rodando
        }
    }
    if (*cpu_task == NULL) {     // Se o processador está livre (nenhuma tarefa rodando)
        *cpu_task = dequeue_pcb(rq);   // Retira o próximo processo da fila de prontos e coloca na CPU
    }
}