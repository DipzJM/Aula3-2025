#include "queue.h"

#include <stdio.h>
#include <stdlib.h>

pcb_t *new_pcb(pid_t pid, uint32_t sockfd, uint32_t time_ms) {
    pcb_t * new_task = malloc(sizeof(pcb_t));
                                            // Aloca memória para um novo processo.
    if (!new_task) return NULL;            // Inicializa os campos do processo:

    new_task->pid = pid;       // pid: identificador do processo
    new_task->status = TASK_COMMAND;  // estado inicial do processo
    new_task->slice_start_ms = 0;   // tempo inicial da fatia de CPU, começa em zero
    new_task->sockfd = sockfd;   // descritor de socket para comunicação
    new_task->time_ms = time_ms;  // tempo total que o processo precisa para executar
    new_task->ellapsed_time_ms = 0;  //  tempo já executado, inicia em zero
    return new_task;  // Retorna o ponteiro para o novo processo ou NULL se falhar
}

int enqueue_pcb(queue_t* q, pcb_t* task) {
    queue_elem_t* elem = malloc(sizeof(queue_elem_t));  // aloca memoria para novo elemento da lista
    if (!elem) return 0;   // se falhar retorna 0

    elem->pcb = task;   // Associa o PCB ao elemento
    elem->next = NULL;  // o proximo é NULL (ultimo)

    if (q->tail) {   // se a fila nao está vazia
        q->tail->next = elem;  // liga o ultimo elemento ao novo
    } else {
        q->head = elem;  // se está vazia o novo elemento é o head
    }
    q->tail = elem;  // atualiza o tail para o novo elemento
    return 1;  // retorna sucesso
}

pcb_t* dequeue_pcb(queue_t* q) {
    if (!q || !q->head) return NULL;   // Se a fila  estiver vazia, retorna NULL

    queue_elem_t* node = q->head;   // pega o primeiro elemento
    pcb_t* task = node->pcb;   // pega o PCB do elemento

    q->head = node->next;  // atualiza o head para o proximo
    if (!q->head)       // se a fila ficou vazia, zera o tail
        q->tail = NULL;

    free(node);   // liberta memoria
    return task;
}

/**
 * @brief Remove e retorna o processo com o menor tempo de execução da fila.
 *
 * Esta função percorre a fila de processos prontos e encontra o processo
 * com o menor tempo total (`time_ms`). Remove o nó correspondente da fila
 * e retorna o ponteiro para o PCB do processo.
 *
 * @param q Ponteiro para a fila de processos (`queue_t`). Não deve ser NULL.
 *
 * @return pcb_t* Ponteiro para o processo com menor tempo de execução,
 *                ou NULL se a fila estiver vazia.
 *
 * @var prev Ponteiro para o nó anterior ao nó atual durante a travessia da lista.
 *           Inicialmente NULL, usado para manter referência do nó anterior a `curr`.
 *
 * @var curr Ponteiro para o nó atual da lista que está a ser percorrido.
 *           Inicialmente aponta para o head da fila.
 *
 * @var min_prev Ponteiro para o nó anterior ao nó com menor tempo de execução encontrado até agora.
 *                Inicialmente NULL (se o nó com menor tempo for o head).
 *
 * @var min_node Ponteiro para o nó com menor tempo de execução encontrado até agora.
 *                 Inicialmente aponta para o head da fila.
 *
 * @var next_task Ponteiro temporário para armazenar o nó retornado pela função remove_queue_elem,
 *                que efetivamente remove o min_node da lista.
 *
 * @var res Ponteiro para o PCB do processo que será retornado ao escalonador.
 */


pcb_t* dequeue_short(queue_t* q) {
    if (!q || !q->head) return NULL;    // se fila vazia, retorna NULL

    queue_elem_t *prev = NULL, *curr = q->head;  // ponteiros para percorrer a fila
    queue_elem_t *min_prev = NULL, *min_node = q->head;   // ponteiros para o menor tempo

    //Percorre a lista á procura do com o menor tempo
    while (curr) {
        if (curr->pcb->time_ms < min_node->pcb->time_ms) {
            min_node = curr;  // atualiza o menor tempo
            min_prev = prev;  // salva anterior ao menor
        }
        prev = curr;  // avança ponteiro anterior
        curr = curr->next;  // avança ponteiro atual
    }

    queue_elem_t *next_task = remove_queue_elem(q, min_node);  // Remove o nó do menor tempo
    pcb_t *res = next_task->pcb;   // Pega o PCB do nó removido
    free(next_task);    // Libera memória do nó
    return res;    // Retorna o PCB

}

queue_elem_t *remove_queue_elem(queue_t* q, queue_elem_t* elem) {
    queue_elem_t* it = q->head;   // Ponteiro para percorrer a fila
    queue_elem_t* prev = NULL;   // Ponteiro para o anterior
    while (it != NULL) {
        if (it == elem) {      // Achou o elemento a remover
            // Remove elem from queue
            if (prev) {
                prev->next = it->next;    // Liga anterior ao próximo
            } else {
                q->head = it->next;     // Se era o head, atualiza head
            }
            if (it == q->tail) {
                q->tail = prev;    // Se era o tail, atualiza tail
            }
            return it;
        }
        prev = it;   // Avança ponteiro anterior
        it = it->next;   // Avança ponteiro atual
    }
    printf("Queue element not found in queue\n");
    return NULL;
}