/**
 * Código base para implementação de relógios vetoriais.
 * Meta: implementar a interação entre três processos ilustrada na figura
 * da URL: 
 * 
 * https://people.cs.rutgers.edu/~pxk/417/notes/images/clocks-vector.png
 * 
 * Compilação: mpicc -o rvet rvet.c
 * Execução:   mpiexec -n 3 ./rvet
 */
 
#include <stdio.h>
#include <string.h>  
#include <mpi.h>     
#include <pthread.h>


typedef struct Clock { 
   int p[3];
} Clock;

// outputs to the console the updated change to a given clock upon an event 
void Clock_logging(int pid, Clock *clock){
   printf("Process: %d, Clock: (%d, %d, %d)\n", pid, clock->p[0], clock->p[1], clock->p[2]);
}

// Generates an event on a clock updating its current value by 1 unit
void Event(int pid, Clock *clock, int logg){
   clock->p[pid]++;
   
   // The following condition checks whether logging of the clock is necessary
   // when logg is non-zero (true) i.e., an Event might not necessarily change the current clock
   if (logg) {Clock_logging(pid, clock);}
}

// Given the id of a sender, effectively sends the message to be captured by the receiver
void Send(int pid_send_to, Clock *clock, int pid_sender){
   Event(pid_sender, clock, 0);
   MPI_Send(&clock->p[0], 3, MPI_INT, pid_send_to, 0, MPI_COMM_WORLD);
   Clock_logging(pid_sender, clock);
}

// Given the id of a sender, sets off an event that may or may not be logged
void Receive(int pid_receive_from, Clock *clock, int pid_receiver){
   Clock temp_clock = {{0,0,0}};
   MPI_Status status;
   
   Event(pid_receiver, clock, 0);
   
   MPI_Recv(&temp_clock.p[0], 3, MPI_INT, pid_receive_from, 0, MPI_COMM_WORLD, &status);
   
   //Atualizar o relogio interno comparando com o relogio recebido
   for (int i = 0; i < 3; i++){
      clock->p[i] = (temp_clock.p[i] > clock->p[i]) ? temp_clock.p[i] : clock->p[i];
   }
   Clock_logging(pid_receiver, clock);
}

// Função para enviar thread
void* sender_thread(void* arg) {
    Clock clock = {{0,0,0}};
    int my_id = *(int*)arg;
    
    // Código de envio
    Event(my_id, &clock, 1);
    Send(0, &clock, my_id);
    
    pthread_exit(NULL);
}

// Função para receber thread
void* receiver_thread(void* arg) {
    Clock clock = {{0,0,0}};
    int my_id = *(int*)arg;
    
    // Código de recebimento
    Receive(0, &clock, my_id);
    
    pthread_exit(NULL);
}

// Função para atualizar thread
void* updater_thread(void* arg) {
    Clock clock = {{0,0,0}};
    int my_id = *(int*)arg;
    
    // Código de atualização
    Event(my_id, &clock, 1);
    
    pthread_exit(NULL);
}

// Função principal
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

    // Inicializando as threads com os identificadores dos processos MPI
    thread_args[0] = my_id;
    thread_args[1] = my_id;
    thread_args[2] = my_id;

    pthread_create(&sender, NULL, sender_thread, &thread_args[0]);
    pthread_create(&receiver, NULL, receiver_thread, &thread_args[1]);
    pthread_create(&updater, NULL, updater_thread, &thread_args[2]);

    // Aguardando o término das threads
    pthread_join(sender, NULL);
    pthread_join(receiver, NULL);
    pthread_join(updater, NULL);

    MPI_Finalize();
    return 0;
}