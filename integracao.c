/* File:
 *    integracao.c
 *
 * Purpose:
 *    Integrar Produtor/Consumidor aos Relógios Vetoriais
 *
 *
 * Compile:  mpicc -g -Wall -o integracao integracao.c -lpthread -lrt
 * Usage:    mpiexec -n 3 ./integracao
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <time.h>
#include <mpi.h>

#define THREAD_NUM 3    // Tamanho do pool de threads
#define BUFFER_SIZE 10   // Númermo máximo de clocks enfileirados

// Vetor para o relógio vetorial
typedef struct Clock {
    int p[5];
} Clock;

struct thread_info {
    long tid;   // Identificador da thread
    long pid;   // Identificador do processo
};

Clock msgInQueue[BUFFER_SIZE];  // Fila de mensagens de entrada
int msgInCount = 0; // Contador para mensagens de entrada

Clock msgOutQueue[BUFFER_SIZE]; // Fila de mensagens de saída
int msgOutCount = 0;    // Contador para mensagens de saída

pthread_mutex_t mutex_msgInQueue;   // Mutex para fila de entrada
pthread_mutex_t mutex_msgOutQueue;  // Mutex para fila de saída

pthread_cond_t inQueueFull; // Condição de fila de entrada cheia
pthread_cond_t inQueueEmpty;    // Condição de fila de entrada vazia

pthread_cond_t outQueueFull;    // Condição de fila de saída cheia
pthread_cond_t outQueueEmpty;   // Condição de fila de saída vazia

// Função para incrementar o relógio do processo
void Event(int pid, Clock* clock){
    clock->p[pid]++;
    printf("* Internal Event - Process: %d, Clock: (%d, %d, %d)\n", pid, clock->p[0], clock->p[1], clock->p[2]);
}

// Função para enviar mensagem para outro processo
void Send(int pid_send_to, Clock* clock, int pid_sender) {
    pthread_mutex_lock(&mutex_msgOutQueue); // Bloqueia acesso à fila de saída

    // Aguarda se a fila de saída estiver cheia
    while (msgOutCount == BUFFER_SIZE) {
        //printf("Process %d / Main Thread: Full msgOutQueue\n", pid_sender);
        pthread_cond_wait(&outQueueFull, &mutex_msgOutQueue);
    }
    clock->p[pid_sender]++; // Incrementa o relógio do processo remetente
    clock->p[3] = pid_sender;   // Define remetente
    clock->p[4] = pid_send_to;  // Define destinatário
    
    // Desloca mensagens na fila
    for (int i = 0; i < msgOutCount - 1; i++) {
        msgOutQueue[i] = msgOutQueue[i + 1];
    }

    // Copia relógio atual para a mensagem
    Clock temp_clock;
    for(int i = 0; i < 5; i++){
        temp_clock.p[i] = clock->p[i];
    }
    
    msgOutQueue[0] = temp_clock;    // Insere mensagem na fila
    msgOutCount++;

    printf("* Send Event - Process: %d send to %d, Clock: (%d, %d, %d)\n", pid_sender, pid_send_to, temp_clock.p[0], temp_clock.p[1], temp_clock.p[2]);

    pthread_mutex_unlock(&mutex_msgOutQueue);   // Libera o acesso à fila de saída
    pthread_cond_signal(&outQueueEmpty);    // Sinaliza que a fila de saída não está mais vazia
}

// Função para processar relógio recebido
void processClock(Clock* internalClock, Clock* receivedClock){
    // Compara os relógios e atualiza o interno
    for (int i = 0; i < 3; i++){    
        internalClock->p[i] = (receivedClock->p[i] > internalClock->p[i]) ? receivedClock->p[i] : internalClock->p[i];
    }
}

// Função para receber mensagem de outro processo
void Receive(int pid_receive_from, int pid_receiver, Clock* clock) {
    pthread_mutex_lock(&mutex_msgInQueue);  // Bloqueia o acesso à fila de entrada 

    // Aguarda se a fila de entrada estiver vazia
    while (msgInCount == 0) {
        //printf("Process %d / Main Thread: Empty msgInQueue\n", pid_receiver);
        pthread_cond_wait(&inQueueEmpty, &mutex_msgInQueue);
    }

    Clock temp_clock = msgInQueue[msgInCount - 1];  // Última mensagem da fila

    msgInCount--;   // Decrementa o contador de mensagens
    clock->p[pid_receiver]++;   // Incrementa o relógio do processo receptor
    printf("* Receiv Event - Process: %d received from %d, Internal Clock = (%d, %d, %d) / External Clock = (%d, %d, %d) => Result = ",
           pid_receiver, temp_clock.p[3], clock->p[0], clock->p[1], clock->p[2],
           temp_clock.p[0], temp_clock.p[1], temp_clock.p[2]);

    processClock(clock, &temp_clock);   // Processa o relógio recebido

    printf("(%d, %d, %d)\n", clock->p[0], clock->p[1], clock->p[2]);

    pthread_mutex_unlock(&mutex_msgInQueue);    // Libera o acesso à fila de entrada
    pthread_cond_signal(&inQueueFull);  // Sinaliza que a fila de entrada não está mais cheia

}


// Função para inicializar a thread de recebimento
void *initReceiverThread(void *args){
    struct thread_info *tinfo = args;
    long tid = tinfo->tid;
    int pid = (int) tinfo->pid;
    Clock received_clock = {{0, 0, 0, 0, 0}};   // Inicializa o relógio recebido
    while(1){

        //printf("Thread %ld Process %d: trying to receive msg\n", tid, pid);

        MPI_Recv(&received_clock.p[0], 5, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        //printf("Thread number %ld from process %d received clock {%d, %d, %d} from process %d\n", tid, pid, received_clock.p[0], received_clock.p[1], received_clock.p[2], received_clock.p[3]);

        pthread_mutex_lock(&mutex_msgInQueue);  // Bloqueia acesso à fila de entrada

        // Aguarda se a fila de entrada estiver cheia
        while (msgInCount == BUFFER_SIZE) {
            // printf("Process %d / Recvr Thread: Full Queue\n", pid);
            pthread_cond_wait(&inQueueFull, &mutex_msgInQueue);
        }

        // Desloca mensagens na fila
        for (int i = 0; i < msgInCount - 1; i++) {
            msgInQueue[i] = msgInQueue[i + 1];
        }
        
        msgInQueue[0] = received_clock; // Insere mensagem na fila
        msgInCount++;

        //printf("Process %d: Clock: (%d, %d, %d) received from %d enqueued in msgInQueue\n",
        //       pid, received_clock.p[0], received_clock.p[1], received_clock.p[2], received_clock.p[3]);

        pthread_mutex_unlock(&mutex_msgInQueue);    // Libera o acesso à fila de entrada
        pthread_cond_signal(&inQueueEmpty); // Sinaliza que a fila de entrada não está mais vazia
    }
    return NULL;
}

// Função para inicializar a thread de envio
void *initSendThread(void *args){
    struct thread_info *tinfo = args;
    long tid = tinfo->tid;
    int pid = (int) tinfo->pid;
    while(1){
        pthread_mutex_lock(&mutex_msgOutQueue); // Bloqueia o acesso à fila de saída

        // Aguarda se a fila de saída estiver vazia
        while (msgOutCount == 0) {
            //printf("Process %d / Sender Thread: Empty msgOutQueue\n", pid);
            pthread_cond_wait(&outQueueEmpty, &mutex_msgOutQueue);
        }

        Clock clock_to_send = msgOutQueue[msgOutCount - 1]; // Última mensagem da fila

        msgOutCount--;

        pthread_mutex_unlock(&mutex_msgOutQueue);   // Libera o acesso à fila de saída
        pthread_cond_signal(&outQueueFull); // Sinaliza que a fila de saída não está mais cheia

        MPI_Send(&clock_to_send.p[0], 5, MPI_INT, clock_to_send.p[4], 0, MPI_COMM_WORLD);

        //printf("Process %d: Clock: (%d, %d, %d) sent to %d with MPI_Send\n",
        //       pid, clock_to_send.p[0], clock_to_send.p[1], clock_to_send.p[2], clock_to_send.p[4]);

    }

    return NULL;
}

// Função principal de execução para cada processo
void *initMainThread(void *args){
    struct thread_info *tinfo = args;
    long threadId = tinfo->tid;
    int pid = (int) tinfo->pid;

    Clock internal_clock = {{0,0,0, pid, 0}};   // Inicializa o relógio interno

    switch(pid){
        case 0:
            Event(pid, &internal_clock);
            Send(1, &internal_clock, pid);
            Receive(1, pid, &internal_clock);
            Send(2, &internal_clock, pid);
            Receive(2, pid, &internal_clock);
            Send(1, &internal_clock, pid);
            Event(pid, &internal_clock);
            break;
        case 1:
            Send(0, &internal_clock, pid);
            Receive(0, pid, &internal_clock);
            Receive(0, pid, &internal_clock);
            break;
        case 2:
            Event(pid, &internal_clock);
            Send(0, &internal_clock, pid);
            Receive(0, pid, &internal_clock);
            break;
        default:
            printf("ERROR: INVALID PROCESS ID\n");
            exit(3);
    }
    return NULL;
}


int main(int argc, char *argv[]) {
    int my_pid; // Id do processo
    pthread_t thread[THREAD_NUM];   // Vetor de threads

    struct thread_info  *tinfo; // Informações da thread

    // Inicializa mutexes
    pthread_mutex_init(&mutex_msgInQueue, NULL);
    pthread_mutex_init(&mutex_msgOutQueue, NULL);

    // Inicializa condições de fila vazia e cheia
    pthread_cond_init(&inQueueEmpty, NULL);
    pthread_cond_init(&inQueueFull, NULL);
    pthread_cond_init(&outQueueEmpty, NULL);
    pthread_cond_init(&outQueueFull, NULL);

    MPI_Init(NULL, NULL);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_pid);

    tinfo = calloc(6, sizeof(*tinfo));  // Aloca memória para informações da thread


    // Loop para criação de threads
    for (long i = 0; i < THREAD_NUM; i++) {
        tinfo[i].tid = i;   // Define o ID da thread
        tinfo[i].pid = my_pid;  // Defina o ID do processo
        switch (i) {
            case 0: // Cria thread principal
                if (pthread_create(&thread[i], NULL, &initMainThread, &tinfo[i]) != 0) {
                    perror("Failed to create the thread");
                }
                break;
            case 1: // Cria thread de envio
                if (pthread_create(&thread[i], NULL, &initSendThread, &tinfo[i]) != 0) {
                    perror("Failed to create the thread");
                }
                break;
            case 2: // Cria thread de recebimento
                if (pthread_create(&thread[i], NULL, &initReceiverThread, &tinfo[i]) != 0) {
                    perror("Failed to create the thread");
                }
                break;
            default:
                printf("ERROR: INVALID THREAD ID\n");
                exit(4);
        }


    }

    for (int i = 0; i < THREAD_NUM; i++) {
        if (pthread_join(thread[i], NULL) != 0) {
            perror("Failed to join the thread");
        }
    }

    pthread_mutex_destroy(&mutex_msgInQueue);
    pthread_cond_destroy(&inQueueEmpty);
    pthread_cond_destroy(&inQueueFull);

    pthread_mutex_destroy(&mutex_msgOutQueue);
    pthread_cond_destroy(&outQueueEmpty);
    pthread_cond_destroy(&outQueueFull);

    MPI_Finalize();

    return 0;
}
