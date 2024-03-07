// PARTE DOS RELÓGIOS VETORIAIS
// ==================================
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



// PARTE DO MODELO PRODUTOR/CONSUMIDOR
// ==================================
int produceRate;
int consumeRate;

typedef struct Clock {
    int p[3];
    long idProducer;
} Clock;

Clock clockQueue[BUFFER_SIZE];
int clockCount = 0;

pthread_mutex_t mutex;

pthread_cond_t condFull;
pthread_cond_t condEmpty;

void consumeClock(Clock *clock, int idConsumer){
    printf("(Consumer Thread %d) Consuming clock {%d, %d, %d} produced by  (Producer Thread %ld)\n", (idConsumer + 1)/2, clock->p[0], clock->p[1], clock->p[2], clock->idProducer);
}

Clock getClock(){
    pthread_mutex_lock(&mutex);

    while (clockCount == 0){
        pthread_cond_wait(&condEmpty, &mutex);
    }

    Clock clock = clockQueue[0];
    int i;
    for (i = 0; i < clockCount - 1; i++){
        clockQueue[i] = clockQueue[i+1];
    }
    clockCount--;

    pthread_mutex_unlock(&mutex);
    pthread_cond_signal(&condFull);

    return clock;
}

void submitClock(Clock clock){
    pthread_mutex_lock(&mutex);

    while (clockCount == BUFFER_SIZE){
        pthread_cond_wait(&condFull, &mutex);
    }

    clockQueue[clockCount] = clock;
    clockCount++;

    pthread_mutex_unlock(&mutex);
    pthread_cond_signal(&condEmpty);
}

/*-------------------------------------------------------------------*/
void *startConsumerThread(void* args) {
    long id = (long) args;
    while (1){
        Clock clock = getClock();
        consumeClock(&clock, id);
        sleep(consumeRate);
    }
    return NULL;
}

/*-------------------------------------------------------------------*/
void *startProducerThread(void* args) {
    long id = (long) args;
    int myTime = 0;
    while (1){
        Clock clock;
        clock.p[0] = 0;
        clock.p[1] = 0;
        clock.p[2] = 0;
        clock.p[id] = myTime;
        clock.idProducer = (long) id/2 + 1;
        submitClock(clock);
        myTime++;
        sleep(produceRate);
    }
    return NULL;
}