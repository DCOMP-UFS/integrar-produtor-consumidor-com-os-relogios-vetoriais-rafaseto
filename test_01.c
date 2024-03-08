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
void* startSenderThread(void* arg) {
    int my_id = *(int*)arg;
    while (1) {
        Clock clock = getClock();
        Event(my_id, &clock, 1);
        Send(0, &clock, my_id);
        usleep(1000000);
    }
    return NULL;
}

// Função da thread para receber relógios
void* startReceiverThread(void* arg) {
    int my_id = *(int*)arg;
    while (1) {
        Clock clock;
        Receive(0, &clock, my_id);
        submitClock(clock);
    }
    return NULL;
}

// Função da thread para atualizar relógios
void* startUpdaterThread(void* arg) {
    int my_id = *(int*)arg;
    while (1) {
        Clock clock = getClock();
        Event(my_id, &clock, 1);
        submitClock(clock);
    }
    return NULL;
}

void callProcess(int my_id) {
    Clock clock = {{0, 0, 0}};

    switch (my_id) {
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
            fprintf(stderr, "The generated process exceeds the number of processes required!!\n");
            printf("Bug found in source code: 404\n");
            break;
    }
}

int main() {
    // Inicializa MPI
    MPI_Init(NULL, NULL);

    // Simula o comportamento de três processos
    for (int i = 0; i < 3; i++) {
        callProcess(i);
    }

    // Finaliza MPI
    MPI_Finalize();

    return 0;
}