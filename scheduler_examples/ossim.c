#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "debug.h"

#define MAX_CLIENTS 128

#include <stdlib.h>
#include <sys/errno.h>

#include "fifo.h"
#include "SJF.h"
#include "RR.h"
#include "MLFQ.h"

#include "msg.h"
#include "queue.h"

mlfq_t *mq = NULL;   // estrutura de mensagens entre scheduler e aplicações (msg_t)
static uint32_t PID = 0;   // contador estático para gerar PIDs únicos

/**
 * @brief Set up the server socket for the scheduler.
 *
 * This function creates a UNIX domain socket, binds it to a specified path,
 * and sets it to listen for incoming connections. It also sets the socket to
 * non-blocking mode.
 *
 * @param socket_path The path where the socket will be created
 * @return int Returns the server file descriptor on success, or -1 on failure
 */
int setup_server_socket(const char *socket_path) {
    int server_fd;   // descritor do socket do servidor
    struct sockaddr_un addr;   // estrutura de endereço para sockets UNIX

    unlink(socket_path);   // remove ficheiro de socket antigo (ignora erros)

    // Create UNIX socket
    if ((server_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return -1; // falha
    }

    memset(&addr, 0, sizeof(struct sockaddr_un));   // inicializa estrutura a zeros
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);   // copia o path para sun_path (seguro)

    // Bind
    if (bind(server_fd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un)) < 0) {
        perror("bind");  // erro no bind
        close(server_fd);   // fecha o descritor antes de sair
        return -1;   // falha
    }

    // Listen
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen");
        close(server_fd);   // fecha....
        return -1;  // falha
    }
    // Set the socket to non-blocking mode
    int flags = fcntl(server_fd, F_GETFL, 0); // obtém flags atuais do descritor
    if (flags != -1) {
        if (fcntl(server_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
            perror("fcntl: set non-blocking");
        }
    }
    return server_fd;   // retorna o descritor do servidor em caso de sucesso
}

/**
 * @brief Check for new client connections and add them to the queue.
 *
 * This function accepts new client connections on the server socket,
 * sets the client sockets to non-blocking mode, and enqueues them
 * into the provided queue.
 *
 * @param command_queue The queue to which new pcb will be added
 * @param server_fd The server socket file descriptor
 */
void check_new_commands(queue_t *command_queue, queue_t *blocked_queue, queue_t *ready_queue, int server_fd, uint32_t current_time_ms) {
    // Accept new client connections
    int client_fd;
    do {
        client_fd = accept(server_fd, NULL, NULL);  // aceita cliente
        if (client_fd < 0) {
            if (errno == EMFILE || errno == ENFILE) {
                perror("accept: too many fds");
                break;  // sai do loop
            }
            if (errno == EINTR)        continue;   // interrompido por sinal - tente novamente
            if (errno == ECONNABORTED) continue;   // handshake abortado -> next
            if ((errno != EAGAIN) && (errno != EWOULDBLOCK)) {
                perror("accept");
            }
            // No more clients to accept right now
            break;
        }
        int flags = fcntl(client_fd, F_GETFL, 0); // Get current flags
        if (flags != -1) {
            if (fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
                perror("fcntl: set non-blocking");
            }
        }
        // Set close-on-exec flag
        int fdflags = fcntl(client_fd, F_GETFD, 0);  // obtem flags do descritor
        if (fdflags != -1) {
            fcntl(client_fd, F_SETFD, fdflags | FD_CLOEXEC);
        }
        DBG("[Scheduler] New client connected: fd=%d\n", client_fd);  // debug
        // New PCBs do not have a time yet, will be set when we receive a RUN message
        pcb_t *pcb = new_pcb(++PID, client_fd, 0);  // cria um novo PCB com PID incremental e time 0
        enqueue_pcb(command_queue, pcb);
    } while (client_fd > 0);  // continua enquanto aceitar clientes

    // Check queue for new commands in the command queue
    queue_elem_t * elem = command_queue->head;   // itera a partir da cabeça da command_queue
    while (elem != NULL) {
        pcb_t *current_pcb = elem->pcb;   // pcb atual a verificar
        msg_t msg;
        int n = read(current_pcb->sockfd, &msg, sizeof(msg_t)); // tenta ler mensagem
        if (n <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No data available right now, move to next
                elem = elem->next;  // nao ha dados, next
            } else {
                if (n < 0) {
                    perror("read");
                } else {
                    DBG("Connection closed by remote host\n");  // n = 0, conexao fechada pelo cliente
                }
                // Remove from queue
                queue_elem_t *tmp = elem;   // guarda elemento atual para remoçao
                elem = elem->next;  // avança antes de libertar
                free(current_pcb);  // libera o pcb (fechou a conexao)
                free(tmp);  //libera o elemento da fila
            }
            continue;
        }
        // We have received a message
        if (msg.request == PROCESS_REQUEST_RUN) {
            current_pcb->pid = msg.pid; // Set the pid from the message
            current_pcb->time_ms = msg.time_ms; // define o tempo de CPU pedido
            current_pcb->ellapsed_time_ms = 0; // zera tempo já executado
            current_pcb->status = TASK_RUNNING;  // pronto a correr
            enqueue_pcb(ready_queue, current_pcb);  // move pcb para ready_queue
            DBG("Process %d requested RUN for %d ms\n", current_pcb->pid, current_pcb->time_ms);  // debug
        } else if (msg.request == PROCESS_REQUEST_BLOCK) {
            current_pcb->pid = msg.pid; // Set the pid from the message
            current_pcb->time_ms = msg.time_ms;  // define tempo de bloqueio
            current_pcb->status = TASK_BLOCKED;  // marca como bloqueado
            enqueue_pcb(blocked_queue, current_pcb);  // alinha block_queue
            DBG("Process %d requested BLOCK for %d ms\n", current_pcb->pid);
        } else {
            printf("Unexpected message received from client\n");
            continue;  // ignora e continua
        }
        // Remove from command queue
        remove_queue_elem(command_queue, elem);
        queue_elem_t *tmp = elem;  // guarda para liberar
        elem = elem->next;  // avança antes de free
        free(tmp);  // libera o elemento da fila (o PCB foi movido)

        // Send ack message
        msg_t ack_msg = {
            .pid = current_pcb->pid,   // pid do processo a quem responde
            .request = PROCESS_REQUEST_ACK,  // tipo ACK
            .time_ms = current_time_ms  //  timestamp atual
        };
        if (write(current_pcb->sockfd, &ack_msg, sizeof(msg_t)) != sizeof(msg_t)) {
            perror("write");
        }
        DBG("Send ACK message to process %d with time %d\n", current_pcb->pid, current_time_ms);
    }

}

/**
 * @brief Check the blocked queue for messages from clients.
 *
 * This function iterates through the blocked queue, checking each client
 * socket for incoming messages. If a RUN message is received, the corresponding
 * pcb is moved to the command queue and an ACK message is sent back to the client.
 * If a client disconnects or an error occurs, the client is removed from the blocked queue.
 *
 * @param blocked_queue The queue containing PCBs in I/O wait stated (blocked) from CPU
 * @param command_queue The queue where PCBs ready for new instructions will be moved
 * @param current_time_ms The current time in milliseconds
 */
void check_blocked_queue(queue_t * blocked_queue, queue_t * command_queue, uint32_t current_time_ms) {
    // Check all elements of the blocked queue for new messages
    queue_elem_t * elem = blocked_queue->head;  // começa na cabeça da blocked_queue
    while (elem != NULL) {
        pcb_t *pcb = elem->pcb;  // PCB atual a verificar

        // Make sure the time is updated only once per cycle
        if (pcb->last_update_time_ms < current_time_ms) {
            if (pcb->time_ms > TICKS_MS) {
                pcb->time_ms -= TICKS_MS;  // decrementa tempo de bloqueio em um tick
            } else {
                pcb->time_ms = 0;   // garante que não fica negativo
            }
        }

        if (pcb->time_ms == 0) {
            // Send DONE message to the application
            msg_t msg = {
                .pid = pcb->pid,   // pid do processo que terminou o bloqueio
                .request = PROCESS_REQUEST_DONE,   // sinaliza DONE
                .time_ms = current_time_ms  // quando terminou
            };
            if (write(pcb->sockfd, &msg, sizeof(msg_t)) != sizeof(msg_t)) {
                perror("write");
            }
            DBG("Process %d finished BLOCK, sending DONE\n", pcb->pid);
            pcb->status = TASK_COMMAND;  // muda estado para command (aguarda nova instrução)
            pcb->last_update_time_ms = current_time_ms;   // atualiza timestamp de última modificação
            enqueue_pcb(command_queue, pcb);   // move o PCB de volta para a fila de comandos

            // Remove from blocked queue
            remove_queue_elem(blocked_queue, elem);   // remove o elemento da blocked_queue
            queue_elem_t *tmp = elem;   // guarda ponteiro para libertar
            elem = elem->next;  // avança para o próximo, pois vamos liberar o atual
            free(tmp);  // libera o elemento removido
        } else {
            elem = elem->next;  // If not done already, do it now
        }
    }
}

static const char *SCHEDULER_NAMES[] = {
    "FIFO",
    "SJF",
    "RR",
    "MLFQ",
    NULL
};  // array tipos scheduler

typedef enum  {
    NULL_SCHEDULER = -1,
    SCHED_FIFO = 0,
    SCHED_SJF,
    SCHED_RR,
    SCHED_MLFQ
} scheduler_en;   // enumeraçao para tipos scheduler

scheduler_en get_scheduler(const char *name) {   // Percorre o array de nomes de escalonadores até encontrar um igual ao argumento
    for (int i = 0; SCHEDULER_NAMES[i] != NULL; i++) {
        // Compara o nome passado com o nome do escalonador atual
        if (strcmp(name, SCHEDULER_NAMES[i]) == 0) {
            // Retorna o tipo de escalonador correspondente ao índice
            return (scheduler_en)i;
        }
    }
    printf("Scheduler %s not recognized. Available options are:\n", name);
    for (int i = 0; SCHEDULER_NAMES[i] != NULL; i++) {
        // Lista cada opção disponível
        printf(" - %s\n", SCHEDULER_NAMES[i]);
    }
    return NULL_SCHEDULER;
}

int main(int argc, char *argv[]) {
    // Verifica se o número de argumentos está correto (deve ser 2)
    if (argc != 2) {
        printf("Usage: %s <scheduler>\nScheduler options: FIFO", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Parse arguments
    scheduler_en scheduler_type = get_scheduler(argv[1]);   // Analisa o argumento e obtém o tipo de escalonador
    if (scheduler_type == NULL_SCHEDULER) {  // Se o tipo for inválido, encerra o programa com erro
        return EXIT_FAILURE;
    }

    // We set up 3 queues: 1 for the simulator and 2 for scheduling
    // - COMMAND queue: for PCBs that are waiting for (new) instructions from the app
    // - READY queue: for PCBs that are ready to run on the CPU
    // - BLOCKED queue: for PCBs that are blocked waiting for I/O
    queue_t command_queue = {.head = NULL, .tail = NULL};  // Inicializa a fila de comandos vazia
    queue_t ready_queue = {.head = NULL, .tail = NULL};     // Inicializa a fila de prontos vazia
    queue_t blocked_queue = {.head = NULL, .tail = NULL};  // Inicializa a fila de bloqueados vazia

    // We only have a single CPU that is a pointer to the actively running PCB on the CPU
    pcb_t *CPU = NULL;

    int server_fd = setup_server_socket(SOCKET_PATH);  // cria e inicializa o socket do server
    if (server_fd < 0) {
        fprintf(stderr, "Failed to set up server socket\n");
        return 1;
    }

    if (scheduler_type == SCHED_MLFQ) {  // Se o escalonador selecionado for MLFQ

        mq = create_mlfq();  // cria estrutura
        if (!mq) {
            fprintf(stderr, "Failed to create MLFQ\n");
            return 1;
        }
    }

    printf("Scheduler server listening on %s...\n", SOCKET_PATH);
    uint32_t current_time_ms = 0;  // Inicializa o relógio do simulador em milissegundos

    while (1) {
        // Verifica novas conexões e/ou comandos recebidos
        check_new_commands(&command_queue, &blocked_queue, &ready_queue, server_fd, current_time_ms);

        if (current_time_ms%1000 == 0) {  // A cada segundo, imprime o tempo atual
            printf("Current time: %d s\n", current_time_ms/1000);
        }
        // Check the status of the PCBs in the blocked queue
        check_blocked_queue(&blocked_queue, &command_queue, current_time_ms);
        // Tasks from the blocked queue could be moved to the command queue, check again
        usleep(TICKS_MS * 1000/2);
        check_new_commands(&command_queue, &blocked_queue, &ready_queue, server_fd, current_time_ms);

        // Seleciona e executa o algoritmo de escalonamento conforme o tipo escolhido
        switch (scheduler_type) {
            case SCHED_FIFO:
                fifo_scheduler(current_time_ms, &ready_queue, &CPU); // executa o scheduler FIFO
                break;
            case SCHED_SJF:
                sjf_scheduler(current_time_ms, &ready_queue, &CPU);  // executa o scheduler SJF
                break;
            case SCHED_RR:
                rr_scheduler(current_time_ms,&ready_queue,&CPU);  // executa o scheduler RR
                break;
            case SCHED_MLFQ:
                mlfq_scheduler(current_time_ms, mq, &CPU);  // executa o scheduler MLFQ
                break;

            default:
                printf("Unknown scheduler type\n");
                break;
        }

        // Simulate a tick
        usleep(TICKS_MS * 1000/2);
        current_time_ms += TICKS_MS;   // Incrementa o relógio do simulador
    }

    // Unreachable, because of the infinite loop!!!!
    return 0;
}
