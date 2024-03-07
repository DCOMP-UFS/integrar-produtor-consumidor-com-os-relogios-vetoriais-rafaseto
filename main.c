/* File:
 *    clock.c
 *
 * Purpose:
 *    Implementação de uma fila de clocks
 *
 *
 * Compile:  gcc -g -Wall -o clock clock.c -lpthread -lrt
 * Usage:    ./clock PRODUCE_RATE CONSUME_RATE
 * PRODUCE_RATE and CONSUME_RATE optional
 * default: PRODUCE_RATE = 2; CONSUME_RATE = 1;
 */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <time.h>
#include <string.h>
#include <mpi.h>

#define THREAD_NUM 3    // Tamanho do pool de threads
#define BUFFER_SIZE 5 // Númermo máximo de tarefas enfileiradas

int receiveRate;
int sendRate;

typedef struct Clock {
    int p[3];
    long idProducer;
} Clock;

Clock msgInQueue[BUFFER_SIZE];
int msgInCount = 0;
Clock msgOutQueue[BUFFER_SIZE];
int msgOutCount = 0;

pthread_mutex_t mutex_msgInQueue;
pthread_mutex_t mutex_msgOutQueue;

pthread_cond_t inQueueFull;
pthread_cond_t inQueueEmpty;

pthread_cond_t outQueueFull;
pthread_cond_t outQueueEmpty;

void consumeClock(Clock *clock, int idConsumer){
    printf("(Consumer Thread %d) Consuming clock {%d, %d, %d} produced by (Producer Thread %ld)\n", (idConsumer + 1)/2, clock->p[0], clock->p[1], clock->p[2], clock->idProducer);
}

Clock dequeueClock(Clock *queue,pthread_mutex_t *mutex,pthread_cond_t condEmpty,pthread_cond_t condFull, int counter){
    pthread_mutex_lock(mutex);

    while (counter == 0){
        printf("Empty clockQueue\n");
        pthread_cond_wait(&condEmpty, mutex);
    }

    Clock clock = queue[0];
    int i;
    for (i = 0; i < counter - 1; i++){
        queue[i] = queue[i+1];
    }
    counter--;

    pthread_mutex_unlock(mutex);
    pthread_cond_signal(&condFull);

    return clock;
}

void enqueueClock(Clock clock, Clock *queue,pthread_mutex_t *mutex, pthread_cond_t condEmpty,pthread_cond_t condFull, int counter){
    pthread_mutex_lock(mutex);

    while (counter == BUFFER_SIZE){
        printf("full clockQueue\n");
        pthread_cond_wait(&condFull, mutex);
    }

    queue[counter] = clock;
    counter++;

    pthread_mutex_unlock(mutex);
    pthread_cond_signal(&condEmpty);
}

/*-------------------------------------------------------------------*/
void *startMsgOutThread(void* args) {
    long id = (long) args;
    while (1){
        Clock clock = getClock();
        consumeClock(&clock, id);
        sleep(1/consumeRate);
    }
    return NULL;
}

/*-------------------------------------------------------------------*/
void *startMsgInThread(void* args) {
    long id = (long) args;
    while (1){
        Clock clock;
        clock.p[0] = 0;
        clock.p[1] = 0;
        clock.p[2] = 0;
        clock.p[idNorm-1] = myTime;
        clock.idProducer = idNorm;
        submitClock(clock);
        myTime++;
        sleep(1/produceRate);
    }
    return NULL;
}
/*-------------------------------------------------------------------*/
void *startClockThread(void* args) {
    long id = (long) args;
    int myTime = 0;
    long idNorm = (long) id/2 + 1;
    while (1){
        Clock clock;
        clock.p[0] = 0;
        clock.p[1] = 0;
        clock.p[2] = 0;
        clock.p[idNorm-1] = myTime;
        clock.idProducer = idNorm;
        submitClock(clock);
        myTime++;
        sleep(1/produceRate);
    }
    return NULL;
}

/*--------------------------------------------------------------------*/
int main(int argc, char* argv[]) {
    int my_rank;

    MPI_Init(NULL, NULL);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

    // REMOVE commented processes ===============================================

    if (my_rank == 0) {
        // process0();
        Call_process(0);
    } else if (my_rank == 1) {
        // process1();
        Call_process(1);
    } else if (my_rank == 2) {
        // process2();
        Call_process(2);
    }

    /* Finaliza MPI */
    MPI_Finalize();

    return 0;

    if (argc == 1){
        produceRate = 2;
        consumeRate = 1;
    }
    else{
        produceRate = atoi(argv[1]);
        consumeRate = atoi(argv[2]);
    }

    printf("Implemented Producer-Consumer model started with\n");
    printf("PRODUCE_RATE = %d; CONSUME_RATE = %d;\n", produceRate, consumeRate);
    // Iniciar semáforo
    pthread_mutex_init(&mutex, NULL);

    pthread_cond_init(&condEmpty, NULL);
    pthread_cond_init(&condFull, NULL);

    //
    pthread_t thread[THREAD_NUM];
    long i;
    for (i = 0; i < THREAD_NUM; i++){
        if (i % 2 == 0){
            if (pthread_create(&thread[i], NULL, &startProducerThread, (void*) i) != 0) {
                perror("Failed to create the thread");
            }
        }
        else{
            if (pthread_create(&thread[i], NULL, &startConsumerThread, (void*) i) != 0) {
                perror("Failed to create the thread");
            }
        }
    }

    for (i = 0; i < THREAD_NUM; i++){
        if (pthread_join(thread[i], NULL) != 0) {
            perror("Failed to join the thread");
        }
    }

    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&condEmpty);
    pthread_cond_destroy(&condFull);
    return 0;
}  /* main */
