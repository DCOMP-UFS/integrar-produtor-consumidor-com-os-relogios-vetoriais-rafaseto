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
#include <string.h>
#include <mpi.h>
#include <limits.h>

#define THREAD_NUM 3    // Tamanho do pool de threads
#define BUFFER_SIZE 5   // Númermo máximo de tarefas enfileiradas

typedef struct Clock {
    int p[4];
} Clock;

struct thread_info {
           long threadId;
           long processId;
       };
       
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

void Clock_logging(int pid, Clock *clock){
   printf("Process: %d, Clock: (%d, %d, %d)\n", pid, clock->p[0], clock->p[1], clock->p[2]);
}

void Event(int pid, Clock *clock, int logg){
   clock->p[pid]++;
   
   // The following condition checks whether logging of the clock is necessary
   // when logg is non-zero (true) i.e., an Event might not necessarily change the current clock
   if (logg) {Clock_logging(pid, clock);}
}

void Send(int pid_send_to, Clock clock) {
    pthread_mutex_lock(mutex);

    while (*msgOutCount == BUFFER_SIZE) {
        printf("Full Queue\n");
        pthread_cond_wait(outQueueFull, mutex_msgOutQueue);
    }
    clock->p[pid]++;
    for (int i = 0; i < *counter - 1; i++) {
        queue[i] = queue[i + 1];
    }
    msgOutQueue[0] = clock;
    msgOutCount++;
    
    pthread_mutex_unlock(mutex_msgOutQueue);
    pthread_cond_signal(outQueueEmpty);
}

void processClock(Clock internalClock, Clock receivedClock){
    for (int i = 0; i < 3; i++){
        internalClock.p[i] = (receivedClock.p[i] > internalClock.p[i]) ? receivedClock.p[i] : internalClock.p[i];
    }
}

void Receive(int receive_from, int pid, Clock clock) {
    pthread_mutex_lock(mutex_msgInQueue);

    while (*counter == BUFFER_SIZE) {
        printf("Empty Queue\n", );
        pthread_cond_wait(inQueueEmpty, mutex_msgInQueue);
    }

    Clock temp_clock = msgInQueue[msgInCount];
    msgInQueue[msgInCount] = NULL;
    //for (int i = 0; i < *counter - 1; i++) {
    //    queue[i] = queue[i + 1];
    //}
    msgInCount--;
    clock->p[pid]++;
    processClock(clock, temp_clock);
    

    pthread_mutex_unlock(mutex_msgInQueue);
    pthread_cond_signal(inQueueFull);

}



void SendMPI(int pid_send_to, int pid_sender, long threadId, Clock clock){
    MPI_Request request;
    clock.p[3] = pid_sender;
    //MPI_Send(&clock.p[0], 4, MPI_INT, pid_send_to, 0, MPI_COMM_WORLD);
    MPI_Isend(&clock.p[0], 4, MPI_INT, pid_send_to, 0, MPI_COMM_WORLD, &request);
    
    printf("Thread %ld from process %d: sended clock {%d, %d, %d} to process %d\n", threadId, pid_sender, clock.p[0], clock.p[1], clock.p[2],pid_send_to);
    //MPI_Wait(&request, MPI_STATUS_IGNORE);
}

void *initReceiverThread(void *args){
    
    
}
void *initSendThread(void *args){
    struct thread_info *tinfo = args;
    long threadId = tinfo->threadId;
    int processId = (int) tinfo->processId;
    
    while (1){
        Clock clock;
        clock.p[0] = 0;
        clock.p[1] = 0;
        clock.p[2] = 0;
        clock.p[3] = threadId;
        clock.p[4] = processId;
        Send(threadId, clock);

    }
    return NULL;
}

void *initMainThread(void *args){
    int pid = (int) args;
    switch(pid){
        case 0:
            Event(pid);
            Send()
            break;
        case 1:
            break;
        case 2:
            break;
    }
}

void *process(void *args) {
    
    
    struct thread_info *tinfo = args;
    long threadId = tinfo->threadId;
    int processId = (int) tinfo->processId;
    printf("started thread %ld from process %d\n", threadId, processId);
    Clock clock = {{0, 0, 0, processId}};
    Clock temp_clock = {{0, 0, 0, 0}};

    switch (threadId) {
        case 0: // Receiving thread
            while (1) {
                printf("Thread %ld Process %d: trying to receive msg\n", threadId, processId);
                MPI_Status *status;
                int flag;
                MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, MPI_STATUS_IGNORE);
                //int count;
                //MPI_Get_count(&status, MPI_INT, &count);
                if (flag) {
                    MPI_Recv(&temp_clock.p[0], 4, MPI_INT, status->MPI_SOURCE, status->MPI_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    printf("Thread number %ld from process %d received clock {%d, %d, %d} from process %d\n", threadId, processId, temp_clock.p[0], temp_clock.p[1], temp_clock.p[2], temp_clock.p[3]);
                    enqueueClock(temp_clock, msgInQueue, &mutex_msgInQueue, &inQueueEmpty, &inQueueFull, &msgInCount, processId, threadId);
                }
                sleep(1);
            }
            break;

        case 1: // Processing thread
            switch(processId){
                case 0:
                    clock.p[processId]++;
                    enqueueClock(clock, msgOutQueue, &mutex_msgOutQueue, &outQueueEmpty, &outQueueFull, &msgOutCount, processId, threadId);
                    break;
                case 1:
                    clock.p[processId]++;
                    enqueueClock(clock, msgOutQueue, &mutex_msgOutQueue, &outQueueEmpty, &outQueueFull, &msgOutCount, processId, threadId);
                    break;
                case 2:
                    clock.p[processId]++;
                    enqueueClock(clock, msgOutQueue, &mutex_msgOutQueue, &outQueueEmpty, &outQueueFull, &msgOutCount, processId, threadId);
                    break;
            }
            while (1) {
                temp_clock = dequeueClock(msgInQueue, &mutex_msgInQueue, &inQueueEmpty, &inQueueFull, &msgInCount, processId, threadId);
                clock.p[processId]++;
                printf("Thread %ld Process %d internal clock update: {%d, %d, %d}. source: external clock processing\n", threadId, processId, clock.p[0], clock.p[1], clock.p[2]);
                processClock(clock, temp_clock);
                clock.p[processId]++;
                enqueueClock(clock, msgOutQueue, &mutex_msgOutQueue, &outQueueEmpty, &outQueueFull, &msgOutCount, processId, threadId);
            }
            break;

        case 2: // Sending thread
            while (1) {
                clock = dequeueClock(msgOutQueue, &mutex_msgOutQueue, &outQueueEmpty, &outQueueFull, &msgOutCount, processId, threadId);
                switch(processId){
                    case 0:
                        Send(1, processId, threadId, clock);
                        Send(2, processId, threadId, clock);
                        Send(1, processId, threadId, clock);
                        break;
                    case 1:
                        Send(0, processId, threadId, clock);
                        break;
                    case 2:
                        Send(0, processId, threadId, clock);
                        break;
                    default:
                }
                
            }
            break;
    }
    return NULL;
}



int main(int argc, char *argv[]) {
    int my_rank;
    pthread_t thread[THREAD_NUM];
    struct thread_info  *tinfo;

    pthread_mutex_init(&mutex_msgInQueue, NULL);
    pthread_mutex_init(&mutex_msgOutQueue, NULL);

    pthread_cond_init(&inQueueEmpty, NULL);
    pthread_cond_init(&inQueueFull, NULL);
    pthread_cond_init(&outQueueEmpty, NULL);
    pthread_cond_init(&outQueueFull, NULL);

    MPI_Init(NULL, NULL);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    
    tinfo = calloc(6, sizeof(*tinfo));
               
    for (long i = 0; i < THREAD_NUM; i++) {
        tinfo[i].threadId = i;
        tinfo[i].processId = my_rank;
        if (pthread_create(&thread[i], NULL, &process, &tinfo[i]) != 0) {
            perror("Failed to create the thread");
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
#include <string.h>
#include <mpi.h>
#include <limits.h>

#define THREAD_NUM 3    // Tamanho do pool de threads
#define BUFFER_SIZE 5   // Númermo máximo de tarefas enfileiradas

typedef struct Clock {
    int p[4];
} Clock;

struct thread_info {
           long threadId;
           long processId;
       };
       
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

void Clock_logging(int pid, Clock *clock){
   printf("Process: %d, Clock: (%d, %d, %d)\n", pid, clock->p[0], clock->p[1], clock->p[2]);
}

void Event(int pid, Clock *clock, int logg){
   clock->p[pid]++;
   
   // The following condition checks whether logging of the clock is necessary
   // when logg is non-zero (true) i.e., an Event might not necessarily change the current clock
   if (logg) {Clock_logging(pid, clock);}
}

void Send(int pid_send_to, Clock clock) {
    pthread_mutex_lock(mutex);

    while (*msgOutCount == BUFFER_SIZE) {
        printf("Full Queue\n");
        pthread_cond_wait(outQueueFull, mutex_msgOutQueue);
    }
    clock->p[pid]++;
    for (int i = 0; i < *counter - 1; i++) {
        queue[i] = queue[i + 1];
    }
    msgOutQueue[0] = clock;
    msgOutCount++;
    
    pthread_mutex_unlock(mutex_msgOutQueue);
    pthread_cond_signal(outQueueEmpty);
}

void processClock(Clock internalClock, Clock receivedClock){
    for (int i = 0; i < 3; i++){
        internalClock.p[i] = (receivedClock.p[i] > internalClock.p[i]) ? receivedClock.p[i] : internalClock.p[i];
    }
}

void Receive(int receive_from, int pid, Clock clock) {
    pthread_mutex_lock(mutex_msgInQueue);

    while (*counter == BUFFER_SIZE) {
        printf("Empty Queue\n", );
        pthread_cond_wait(inQueueEmpty, mutex_msgInQueue);
    }

    Clock temp_clock = msgInQueue[msgInCount];
    msgInQueue[msgInCount] = NULL;
    //for (int i = 0; i < *counter - 1; i++) {
    //    queue[i] = queue[i + 1];
    //}
    msgInCount--;
    clock->p[pid]++;
    processClock(clock, temp_clock);
    

    pthread_mutex_unlock(mutex_msgInQueue);
    pthread_cond_signal(inQueueFull);

}



void SendMPI(int pid_send_to, int pid_sender, long threadId, Clock clock){
    MPI_Request request;
    clock.p[3] = pid_sender;
    //MPI_Send(&clock.p[0], 4, MPI_INT, pid_send_to, 0, MPI_COMM_WORLD);
    MPI_Isend(&clock.p[0], 4, MPI_INT, pid_send_to, 0, MPI_COMM_WORLD, &request);
    
    printf("Thread %ld from process %d: sended clock {%d, %d, %d} to process %d\n", threadId, pid_sender, clock.p[0], clock.p[1], clock.p[2],pid_send_to);
    //MPI_Wait(&request, MPI_STATUS_IGNORE);
}

void *initReceiverThread(void *args){
    
    
}
void *initSendThread(void *args){
    
}

void *initMainThread(void *args){
    int pid = (int) args;
    switch(pid){
        case 0:
            Event(pid);
            Send()
            break;
        case 1:
            break;
        case 2:
            break;
    }
}

void *process(void *args) {
    
    
    struct thread_info *tinfo = args;
    long threadId = tinfo->threadId;
    int processId = (int) tinfo->processId;
    printf("started thread %ld from process %d\n", threadId, processId);
    Clock clock = {{0, 0, 0, processId}};
    Clock temp_clock = {{0, 0, 0, 0}};

    switch (threadId) {
        case 0: // Receiving thread
            while (1) {
                printf("Thread %ld Process %d: trying to receive msg\n", threadId, processId);
                MPI_Status *status;
                int flag;
                MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, MPI_STATUS_IGNORE);
                //int count;
                //MPI_Get_count(&status, MPI_INT, &count);
                if (flag) {
                    MPI_Recv(&temp_clock.p[0], 4, MPI_INT, status->MPI_SOURCE, status->MPI_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    printf("Thread number %ld from process %d received clock {%d, %d, %d} from process %d\n", threadId, processId, temp_clock.p[0], temp_clock.p[1], temp_clock.p[2], temp_clock.p[3]);
                    enqueueClock(temp_clock, msgInQueue, &mutex_msgInQueue, &inQueueEmpty, &inQueueFull, &msgInCount, processId, threadId);
                }
                sleep(1);
            }
            break;

        case 1: // Processing thread
            switch(processId){
                case 0:
                    clock.p[processId]++;
                    enqueueClock(clock, msgOutQueue, &mutex_msgOutQueue, &outQueueEmpty, &outQueueFull, &msgOutCount, processId, threadId);
                    break;
                case 1:
                    clock.p[processId]++;
                    enqueueClock(clock, msgOutQueue, &mutex_msgOutQueue, &outQueueEmpty, &outQueueFull, &msgOutCount, processId, threadId);
                    break;
                case 2:
                    clock.p[processId]++;
                    enqueueClock(clock, msgOutQueue, &mutex_msgOutQueue, &outQueueEmpty, &outQueueFull, &msgOutCount, processId, threadId);
                    break;
            }
            while (1) {
                temp_clock = dequeueClock(msgInQueue, &mutex_msgInQueue, &inQueueEmpty, &inQueueFull, &msgInCount, processId, threadId);
                clock.p[processId]++;
                printf("Thread %ld Process %d internal clock update: {%d, %d, %d}. source: external clock processing\n", threadId, processId, clock.p[0], clock.p[1], clock.p[2]);
                processClock(clock, temp_clock);
                clock.p[processId]++;
                enqueueClock(clock, msgOutQueue, &mutex_msgOutQueue, &outQueueEmpty, &outQueueFull, &msgOutCount, processId, threadId);
            }
            break;

        case 2: // Sending thread
            while (1) {
                clock = dequeueClock(msgOutQueue, &mutex_msgOutQueue, &outQueueEmpty, &outQueueFull, &msgOutCount, processId, threadId);
                switch(processId){
                    case 0:
                        Send(1, processId, threadId, clock);
                        Send(2, processId, threadId, clock);
                        Send(1, processId, threadId, clock);
                        break;
                    case 1:
                        Send(0, processId, threadId, clock);
                        break;
                    case 2:
                        Send(0, processId, threadId, clock);
                        break;
                    default:
                }
                
            }
            break;
    }
    return NULL;
}



int main(int argc, char *argv[]) {
    int my_rank;
    pthread_t thread[THREAD_NUM];
    struct thread_info  *tinfo;

    pthread_mutex_init(&mutex_msgInQueue, NULL);
    pthread_mutex_init(&mutex_msgOutQueue, NULL);

    pthread_cond_init(&inQueueEmpty, NULL);
    pthread_cond_init(&inQueueFull, NULL);
    pthread_cond_init(&outQueueEmpty, NULL);
    pthread_cond_init(&outQueueFull, NULL);

    MPI_Init(NULL, NULL);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    
    tinfo = calloc(6, sizeof(*tinfo));
               
    for (long i = 0; i < THREAD_NUM; i++) {
        tinfo[i].threadId = i;
        tinfo[i].processId = my_rank;
        if (pthread_create(&thread[i], NULL, &process, &tinfo[i]) != 0) {
            perror("Failed to create the thread");
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
