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
// /*
// Compacts the generation of processes
// Defines each process according to its ranking

void Call_process(int my_id){
    Clock clock = {{0,0,0}};

    switch(my_id){
        case 0:
            Event(my_id, &clock, 1);
            Send(1, &clock, my_id);
            Receive(1, &clock, my_id);
            Send(2, &clock, my_id);
            Receive(2, &clock, my_id);
            Send(1, &clock, my_id);
            Event(my_id, &clock, 1);
            break;

        case 1:
            Send(0, &clock, my_id);
            Receive(0, &clock, my_id);
            Receive(0, &clock, my_id);
            break;

        case 2:
            Event(my_id, &clock, 1);
            Send(0, &clock, my_id);
            Receive(0, &clock, my_id);
            break;

        default:
            perror("The generated process exceeds the number of processes required!!\n");
            printf("Bug found in source code: 404");
            break;


    }
}
// */ REMOVE ===============================================
/*
// Representa o processo de rank 0
void process0(){
   Clock clock = {{0,0,0}};

   int my_id = 0;

   Event(my_id, &clock, 1);
   Send(1, &clock, my_id);
   Receive(1, &clock, my_id);
   Send(2, &clock, my_id);
   Receive(2, &clock, my_id);
   Send(1, &clock, my_id);
   Event(my_id, &clock, 1);

}

// Representa o processo de rank 1
void process1(){
   Clock clock = {{0,0,0}};
   int my_id = 1;

   Send(0, &clock, my_id);
   Receive(0, &clock, my_id);
   Receive(0, &clock, my_id);
}

// Representa o processo de rank 2
void process2(){
   Clock clock = {{0,0,0}};
   int my_id = 2;

   Event(my_id, &clock, 1);
   Send(0, &clock, my_id);
   Receive(0, &clock, my_id);
}
*/

int main(void) {
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
}  /* main */