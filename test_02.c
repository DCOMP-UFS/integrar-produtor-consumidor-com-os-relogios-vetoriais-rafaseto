#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <mpi.h>

void *receive_thread(void *args) {
    int *rank = (int *)arg;
    int received_value;
    MPI_Status;

    while (1) {
        MPI_Recv(&received_value, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        printf("Thread receptora no processo %d recebeu o valor: %d\n", *rank, received_value); 
    }

    return NULL;
}

int main(int argc, char **argv) {
    int rank size;
        MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    pthread_t thread;
    int thread_arg = rank;
    pthread_create(&thread, NULL, receive_thread, (void *)&thread_arg);

    // Resto do código do programa

    pthread_join(thread, NULL);
    MPI_Finalize();
    return 0;
}