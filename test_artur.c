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

void enqueueClock(Clock clock, Clock *queue, pthread_mutex_t *mutex, pthread_cond_t *condEmpty, pthread_cond_t *condFull, int *counter, long processId, int threadID) {
    pthread_mutex_lock(mutex);

    while (*counter == BUFFER_SIZE) {
        printf("Thread %d from process %ld: Full Queue\n", threadID, processId);
        pthread_cond_wait(condFull, mutex);
    }

    queue[*counter] = clock;
    (*counter)++;
    printf("Thread %d from process %ld enqueued clock {%d, %d, %d} from process %d\n", threadID, processId, clock.p[0], clock.p[1], clock.p[2], clock.p[3]);

    pthread_mutex_unlock(mutex);
    pthread_cond_signal(condEmpty);
}

Clock dequeueClock(Clock *queue, pthread_mutex_t *mutex, pthread_cond_t *condEmpty, pthread_cond_t *condFull, int *counter, long processId, int threadID) {
    pthread_mutex_lock(mutex);

    while (*counter == 0) {
        printf("Thread %d from process %ld: Empty Queue\n", threadID, processId);
        pthread_cond_wait(condEmpty, mutex);
    }

    Clock clock = queue[0];
    for (int i = 0; i < *counter - 1; i++) {
        queue[i] = queue[i + 1];
    }
    (*counter)--;
    printf("Thread %d from process %ld dequeued clock {%d, %d, %d}. Now processing...\n", threadID, processId, clock.p[0], clock.p[1], clock.p[2]);

    pthread_mutex_unlock(mutex);
    pthread_cond_signal(condFull);

    return clock;
}

void processClock(Clock internalClock, Clock receivedClock){
    for (int i = 0; i < 3; i++){
        internalClock.p[i] = (receivedClock.p[i] > internalClock.p[i]) ? receivedClock.p[i] : internalClock.p[i];
    }
}

void Send(int pid_send_to, int pid_sender, long threadId, Clock clock){
    clock.p[3] = pid_sender;
    MPI_Send(&clock.p[0], 4, MPI_INT, pid_send_to, 0, MPI_COMM_WORLD);
    printf("Thread %ld from process %d: sended clock {%d, %d, %d} to process %d\n", threadId, pid_sender, clock.p[0], clock.p[1], clock.p[2],pid_send_to);
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
                MPI_Status status;
                MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
                int count;
                MPI_Get_count(&status, MPI_INT, &count);
                if (count == 4) {
                    MPI_Recv(&temp_clock.p[0], 4, MPI_INT, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    printf("Thread number %ld from process %d received clock {%d, %d, %d} from process %d\n", threadId, processId, temp_clock.p[0], temp_clock.p[1], temp_clock.p[2], temp_clock.p[3]);
                    enqueueClock(temp_clock, msgInQueue, &mutex_msgInQueue, &inQueueEmpty, &inQueueFull, &msgInCount, processId, threadId);
                }
            }
            break;

        case 1: // Processing thread
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
                Send(1, processId, threadId, clock);
                Send(2, processId, threadId, clock);
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
