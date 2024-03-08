#include <stdio.h>
#include <string.h>
#include <mpi.h>
#include <pthread.h>
#include <unistd.h>

#define BUFFER_SIZE 10

// Definição da estrutura Clock para representar um relógio
typedef struct Clock {
    int p[3];            // Vetor de três inteiros para representar o estado do relógio
    long idProducer;     // Identificador do produtor do relógio
} Clock;

// Buffer para armazenar os relógios entre Receive e Update
Clock clockQueueReceiveUpdate[BUFFER_SIZE];
int clockCountReceiveUpdate = 0;

// Buffer para armazenar os relógios entre Update e Send
Clock clockQueueUpdateSend[BUFFER_SIZE];
int clockCountUpdateSend = 0;

pthread_mutex_t mutexReceiveUpdate;
pthread_cond_t condFullReceiveUpdate;
pthread_cond_t condEmptyReceiveUpdate;

pthread_mutex_t mutexUpdateSend;
pthread_cond_t condFullUpdateSend;
pthread_cond_t condEmptyUpdateSend;

// Função para obter um relógio do buffer entre Receive e Update
Clock getClockReceiveUpdate(){
    pthread_mutex_lock(&mutexReceiveUpdate);  

    while (clockCountReceiveUpdate == 0){
        pthread_cond_wait(&condEmptyReceiveUpdate, &mutexReceiveUpdate);
    }

    Clock clock = clockQueueReceiveUpdate[0];  
    int i;
    for (i = 0; i < clockCountReceiveUpdate - 1; i++){
        clockQueueReceiveUpdate[i] = clockQueueReceiveUpdate[i+1];  
    }
    clockCountReceiveUpdate--;  

    pthread_mutex_unlock(&mutexReceiveUpdate);
    pthread_cond_signal(&condFullReceiveUpdate);

    return clock;  
}

// Função para submeter um relógio ao buffer entre Receive e Update
void submitClockReceiveUpdate(Clock clock){
    pthread_mutex_lock(&mutexReceiveUpdate);

    while (clockCountReceiveUpdate == BUFFER_SIZE){
        pthread_cond_wait(&condFullReceiveUpdate, &mutexReceiveUpdate);
    }

    clockQueueReceiveUpdate[clockCountReceiveUpdate] = clock;
    clockCountReceiveUpdate++;

    pthread_mutex_unlock(&mutexReceiveUpdate);
    pthread_cond_signal(&condEmptyReceiveUpdate);
}

// Função para obter um relógio do buffer entre Update e Send
Clock getClockUpdateSend(){
    pthread_mutex_lock(&mutexUpdateSend);

    while (clockCountUpdateSend == 0){
        pthread_cond_wait(&condEmptyUpdateSend, &mutexUpdateSend);
    }

    Clock clock = clockQueueUpdateSend[0];
    int i;
    for (i = 0; i < clockCountUpdateSend - 1; i++){
        clockQueueUpdateSend[i] = clockQueueUpdateSend[i+1];
    }
    clockCountUpdateSend--;

    pthread_mutex_unlock(&mutexUpdateSend);
    pthread_cond_signal(&condFullUpdateSend);

    return clock;
}

// Função para submeter um relógio ao buffer entre Update e Send
void submitClockUpdateSend(Clock clock){
    pthread_mutex_lock(&mutexUpdateSend);

    while (clockCountUpdateSend == BUFFER_SIZE){
        pthread_cond_wait(&condFullUpdateSend, &mutexUpdateSend);
    }

    clockQueueUpdateSend[clockCountUpdateSend] = clock;
    clockCountUpdateSend++;

    pthread_mutex_unlock(&mutexUpdateSend);
    pthread_cond_signal(&condEmptyUpdateSend);
}

// Função para receber um relógio de outro processo via MPI e atualizar o relógio local
void Receive(int pid_receive_from, Clock *clock, int pid_receiver){
    Clock temp_clock = {{0,0,0}};  
    MPI_Status status;
    MPI_Recv(&temp_clock.p[0], 3, MPI_INT, pid_receive_from, 0, MPI_COMM_WORLD, &status);  
    for (int i = 0; i < 3; i++){
        clock->p[i] = (temp_clock.p[i] > clock->p[i]) ? temp_clock.p[i] : clock->p[i];  
    }
    printf("Process %d received clock {%d, %d, %d} from process %d\n", pid_receiver, clock->p[0], clock->p[1], clock->p[2], pid_receive_from);
    submitClockReceiveUpdate(*clock); // Submete o relógio ao buffer entre Receive e Update
}

// Função para atualizar o relógio e submetê-lo ao buffer entre Update e Send
void Update(int pid_updater, Clock *clock, int pid_receiver){
    Clock updatedClock = getClockReceiveUpdate(); // Obtém um relógio do buffer entre Receive e Update
    updatedClock.p[pid_updater]++; // Incrementa o relógio do processo atual
    printf("Process %d updated clock to {%d, %d, %d}\n", pid_updater, updatedClock.p[0], updatedClock.p[1], updatedClock.p[2]);
    submitClockUpdateSend(updatedClock); // Submete o relógio ao buffer entre Update e Send
}

// Função para enviar um relógio para outro processo via MPI e consumir o relógio produzido
void Send(int pid_send_to, Clock *clock, int pid_sender){
    MPI_Send(&clock->p[0], 3, MPI_INT, pid_send_to, 0, MPI_COMM_WORLD);  
    printf("Process %d sent clock {%d, %d, %d} to process %d\n", pid_sender, clock->p[0], clock->p[1], clock->p[2], pid_send_to);
    Clock consumedClock = getClockUpdateSend(); // Obtém um relógio do buffer entre Update e Send para consumo
    printf("Process %d received clock {%d, %d, %d} back from buffer\n", pid_sender, consumedClock.p[0], consumedClock.p[1], consumedClock.p[2]);
}

void callProcess(int my_id) {
    Clock clock = {{0, 0, 0}};

    switch (my_id) {
        case 0:
            Update(my_id, &clock, 1);
            Send(1, &clock, my_id);
            Receive(1, &clock, my_id);
            Send(2, &clock, my_id);
            Receive(2, &clock, my_id);
            Send(1, &clock, my_id);
            Update(my_id, &clock, 1);
            break;

        case 1:
            Send(0, &clock, my_id);
            Receive(0, &clock, my_id);
            Update(my_id, &clock, 1); 
            break;

        case 2:
            Update(my_id, &clock, 1);
            Send(0, &clock, my_id);
            Receive(0, &clock, my_id);
            break;

        default:
            fprintf(stderr, "The generated process exceeds the number of processes required!!\n");
            printf("Bug found in source code: 404\n");
            break;
    }
}

int main() {
    // Inicialização MPI
    MPI_Init(NULL, NULL);

    // Inicialização das variáveis pthread
    pthread_mutex_init(&mutexReceiveUpdate, NULL);
    pthread_mutex_init(&mutexUpdateSend, NULL);
    pthread_cond_init(&condFullReceiveUpdate, NULL);
    pthread_cond_init(&condEmptyReceiveUpdate, NULL);
    pthread_cond_init(&condFullUpdateSend, NULL);
    pthread_cond_init(&condEmptyUpdateSend, NULL);

    // Número total de processos
    int total_processes;
    MPI_Comm_size(MPI_COMM_WORLD, &total_processes);

    // ID do processo atual
    int my_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

    // Chamadas de processos para teste
    callProcess(my_rank);

    // Finalização MPI
    MPI_Finalize();

    // Destruir variáveis pthread
    pthread_mutex_destroy(&mutexReceiveUpdate);
    pthread_mutex_destroy(&mutexUpdateSend);
    pthread_cond_destroy(&condFullReceiveUpdate);
    pthread_cond_destroy(&condEmptyReceiveUpdate);
    pthread_cond_destroy(&condFullUpdateSend);
    pthread_cond_destroy(&condEmptyUpdateSend);

    return 0;
}