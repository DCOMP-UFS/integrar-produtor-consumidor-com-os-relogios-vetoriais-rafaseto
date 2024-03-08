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

Clock clockQueue[BUFFER_SIZE];  // Buffer para armazenar os relógios produzidos
int clockCount = 0;              // Contador para o número de relógios presentes no buffer

pthread_mutex_t mutex;           // Mutex para garantir exclusão mútua no acesso ao buffer
pthread_cond_t condFull;         // Variável de condição para indicar que o buffer está cheio
pthread_cond_t condEmpty;        // Variável de condição para indicar que o buffer está vazio

// Função para imprimir o estado do relógio
void Clock_logging(int pid, Clock *clock){
    printf("Process: %d, Clock: (%d, %d, %d)\n", pid, clock->p[0], clock->p[1], clock->p[2]);
}

// Função para simular um evento incrementando o relógio de um processo
void Event(int pid, Clock *clock, int logg){
    clock->p[pid]++;  // Incrementa o relógio do processo especificado
    if (logg) {Clock_logging(pid, clock);}  // Se logg for verdadeiro, imprime o estado do relógio
}

// Função para enviar um relógio para outro processo via MPI
void Send(int pid_send_to, Clock *clock, int pid_sender){
    Event(pid_sender, clock, 0);  // Incrementa o relógio do processo remetente
    MPI_Send(&clock->p[0], 3, MPI_INT, pid_send_to, 0, MPI_COMM_WORLD);  // Envia o relógio via MPI
    Clock_logging(pid_sender, clock);  // Imprime o estado do relógio após o envio
}

// Função para receber um relógio de outro processo via MPI
void Receive(int pid_receive_from, Clock *clock, int pid_receiver){
    Clock temp_clock = {{0,0,0}};  // Cria um relógio temporário com todos os valores inicializados para zero
    MPI_Status status;
    Event(pid_receiver, clock, 0);  // Incrementa o relógio do processo receptor
    MPI_Recv(&temp_clock.p[0], 3, MPI_INT, pid_receive_from, 0, MPI_COMM_WORLD, &status);  // Recebe o relógio via MPI
    for (int i = 0; i < 3; i++){
        clock->p[i] = (temp_clock.p[i] > clock->p[i]) ? temp_clock.p[i] : clock->p[i];  // Atualiza o relógio local com os valores recebidos
    }
    Clock_logging(pid_receiver, clock);  // Imprime o estado do relógio após a recepção
}

// Função para consumir (imprimir) um relógio produzido
void consumeClock(Clock *clock, int idConsumer){
    printf("(Consumer Thread %d) Consuming clock {%d, %d, %d} produced by  (Producer Thread %ld)\n", (idConsumer + 1)/2, clock->p[0], clock->p[1], clock->p[2], clock->idProducer);
}

// Função para obter um relógio do buffer
Clock getClock(){
    pthread_mutex_lock(&mutex);  // Bloqueia o mutex para garantir exclusão mútua

    while (clockCount == 0){
        pthread_cond_wait(&condEmpty, &mutex);  // Aguarda até que o buffer não esteja vazio
    }

    Clock clock = clockQueue[0];  // Obtém o relógio do buffer
    int i;
    for (i = 0; i < clockCount - 1; i++){
        clockQueue[i] = clockQueue[i+1];  // Move os relógios restantes no buffer para frente
    }
    clockCount--;  // Atualiza o contador de relógios no buffer

    pthread_mutex_unlock(&mutex);  // Libera o mutex
    pthread_cond_signal(&condFull);  // Sinaliza que o buffer não está mais cheio

    return clock;  // Retorna o relógio obtido
}

// Função para submeter um relógio ao buffer
void submitClock(Clock clock){
    pthread_mutex_lock(&mutex);  // Bloqueia o mutex para garantir exclusão mútua

    while (clockCount == BUFFER_SIZE){
        pthread_cond_wait(&condFull, &mutex);  // Aguarda até que o buffer não esteja cheio
    }

    clockQueue[clockCount] = clock;  // Adiciona o relógio ao buffer
    clockCount++;  // Atualiza o contador de relógios no buffer

    pthread_mutex_unlock(&mutex);  // Libera o mutex
    pthread_cond_signal(&condEmpty);  // Sinaliza que o buffer não está mais vazio
}

// Função da thread para enviar relógios
void* sender_thread(void* arg) {
    Clock clock;
    int my_id = *(int*)arg;

    while (1) {
        clock = getClock();  // Obtém um relógio do buffer
        Event(my_id, &clock, 1);  // Incrementa o relógio com o ID da thread
        Send(0, &clock, my_id);  // Envia o relógio para o processo 0
        // Aguardar um pouco para simular um processo de envio
        usleep(1000);  // Aguarda por um curto período de tempo
    }
}

// Função da thread para receber relógios
void* receiver_thread(void* arg) {
    Clock clock;
    int my_id = *(int*)arg;

    while (1) {
        Receive(0, &clock, my_id);  // Recebe um relógio do processo 0
        submitClock(clock);  // Submete o relógio ao buffer
    }
}

// Função da thread para atualizar relógios
void* updater_thread(void* arg) {
    Clock clock;
    int my_id = *(int*)arg;

    while (1) {
        clock = getClock();  // Obtém um relógio do buffer
        Event(my_id, &clock, 1);  // Incrementa o relógio com o ID da thread
        submitClock(clock);  // Submete o relógio ao buffer
    }
}

int main(int argc, char** argv) {
    int my_id, num_processes;
    pthread_t sender, receiver, updater;
    int thread_args[3];

    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&condFull, NULL);
    pthread_cond_init(&condEmpty, NULL);

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