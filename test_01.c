#include <stdio.h>
#include <string.h>
#include <mpi.h>
#include <pthread.h>

typedef struct Clock {
    int p[3];
} Clock;

void Clock_logging(int pid, Clock *clock){
    printf("Process: %d, Clock: (%d, %d, %d)\n", pid, clock->p[0], clock->p[1], clock->p[2]);
}

void Event(int pid, Clock *clock, int logg){
    clock->p[pid]++;
    if (logg) {Clock_logging(pid, clock);}
}

void Send(int pid_send_to, Clock *clock, int pid_sender){
    Event(pid_sender, clock, 0);
    MPI_Send(&clock->p[0], 3, MPI_INT, pid_send_to, 0, MPI_COMM_WORLD);
    Clock_logging(pid_sender, clock);
}

void Receive(int pid_receive_from, Clock *clock, int pid_receiver){
    Clock temp_clock = {{0,0,0}};
    MPI_Status status;
    Event(pid_receiver, clock, 0);
    MPI_Recv(&temp_clock.p[0], 3, MPI_INT, pid_receive_from, 0, MPI_COMM_WORLD, &status);
    for (int i = 0; i < 3; i++){
        clock->p[i] = (temp_clock.p[i] > clock->p[i]) ? temp_clock.p[i] : clock->p[i];
    }
    Clock_logging(pid_receiver, clock);
}

void* sender_thread(void* arg) {
    Clock clock = {{0,0,0}};
    int my_id = *(int*)arg;
    Event(my_id, &clock, 1);
    Send(0, &clock, my_id);
    pthread_exit(NULL);
}

void* receiver_thread(void* arg) {
    Clock clock = {{0,0,0}};
    int my_id = *(int*)arg;
    Receive(0, &clock, my_id);
    pthread_exit(NULL);
}

void* updater_thread(void* arg) {
    Clock clock = {{0,0,0}};
    int my_id = *(int*)arg;
    Event(my_id, &clock, 1);
    pthread_exit(NULL);
}

int main(int argc, char** argv) {
    int my_id, num_processes;
    pthread_t sender, receiver, updater;
    int thread_args[3];

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_id);
    MPI_Comm_size(MPI_COMM_WORLD, &num_processes);

    if (num_processes != 3) {
        printf("Este exemplo requer exatamente 3 processos MPI.\n");
        MPI_Finalize();
        return 1;
    }

    thread_args[0] = my_id;
    thread_args[1] = my_id;
    thread_args[2] = my_id;

    pthread_create(&sender, NULL, sender_thread, &thread_args[0]);
    pthread_create(&receiver, NULL, receiver_thread, &thread_args[1]);
    pthread_create(&updater, NULL, updater_thread, &thread_args[2]);

    pthread_join(sender, NULL);
    pthread_join(receiver, NULL);
    pthread_join(updater, NULL);

    MPI_Finalize();
    return 0;
}