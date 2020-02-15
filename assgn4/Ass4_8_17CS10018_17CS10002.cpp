/* Round robin scheduler */

#include <iostream>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <queue>
#include <map>

#define N 20													// no of jobs
#define M 10000													// Max shared_buffer size
#define NUM_PROD 1000											// No of integers to be generated
#define TIME_SLICE 0.5

// states of workers
#define RUNNING 1
#define READY 2
#define WAITING 3
#define TERMINATED 0

// job status flags
#define DONE  1
#define NOT_DONE 0
#define COMPLETE 2
#define START -1

using namespace std;

struct status {
    char type;													// type of worker
	pthread_t tid;
	int current_state;
	int previous_state;
    int job_done_flag;
};

queue<int> SHARED_BUFFER;										// the arrray of randomly generated numbers

queue<pthread_t> READY_QUEUE;									// Ready jobs' queue of scheduler
queue<pthread_t> PRODUCER_WAITING_QUEUE;						// Waiting[as the shared buffer is full] producers
queue<pthread_t> CONSUMER_WAITING_QUEUE;						// Waiting[as the shared buffer is empty] consumers

pthread_t current_t;											// tid of current thread

pthread_mutex_t lock, lock2, state_lock;
pthread_t worker[N], scheduler, reporter;
map<pthread_t,struct status> STATUS;

int term_threads = 0, num_p = 0, num_c = 0;						// terminated threads until now, no of producers,[\n] 
																				// no of consumers respectively
sigset_t sleep_mask;											

void *producer(void *param);
void *consumer(void *param);
void *scheduler_handle(void *param);
void *reporter_handle(void *param);

void sleep_handler(int sig);
void wake_handler(int sig);

int main() {

	sigemptyset(&sleep_mask);
	sigaddset(&sleep_mask, SIGUSR1);

    signal(SIGUSR1, sleep_handler);
	signal(SIGUSR2, wake_handler);

    pthread_attr_t attr;

    pthread_attr_init (&attr);

    if (pthread_mutex_init(&lock, NULL) != 0) { 				// for producer
        printf("\n mutex init has failed\n"); 
        return 1; 
    }

    if (pthread_mutex_init(&lock2, NULL) != 0) { 				// for consumer
        printf("\n mutex init has failed\n"); 
        return 1; 
    }
    
    if (pthread_mutex_init(&state_lock, NULL) != 0) { 			// for scheduler to change states before reporter prints
        printf("\n mutex init has failed\n"); 
        return 1; 
    }
    struct status temp;

    pthread_create(&scheduler, &attr, scheduler_handle, NULL);
    pthread_create(&reporter, &attr, reporter_handle, NULL);

    for(int i=0; i < N; i++) {									// Creating producers and consumers randomly
        srand(2*i*time(NULL));
        int x = rand() % 20;
        if(x < 10) {
            if(pthread_create(&worker[i], &attr, producer, NULL) != 0) {
                perror("thread creation failed");
            }
            temp.type = 'P';
            num_p++;
        }
        else {
            if(pthread_create(&worker[i], &attr, consumer, NULL) != 0) {
                perror("thread creation failed");
            }
            temp.type = 'C';
            num_c++;
        }

        if(i==0) {												// First worker created
            current_t = worker[i];
            temp.tid = worker[i];
            temp.current_state = RUNNING;
            temp.previous_state = RUNNING;
            temp.job_done_flag = START;
        }
        else {
            pthread_kill(worker[i], SIGUSR1);					// making them sleep and pushed to ready queue, waiting for their chance
            READY_QUEUE.push(worker[i]);
            temp.tid = worker[i];
            temp.current_state = READY;
            temp.previous_state = READY;
            temp.job_done_flag = START;
        }
        STATUS.insert({worker[i], temp});						// inserting into status map
    }
    
    for(int i=0; i < N; i++) {
        pthread_join(worker[i], NULL);
    }
    pthread_join(scheduler, NULL);
    pthread_join(reporter, NULL);
    pthread_mutex_destroy(&lock);
    pthread_mutex_destroy(&lock2);
    pthread_mutex_destroy(&state_lock);
    
    cout<<"\nNumber of Producers : "<<num_p<<" ; Number of Consumers :  "<<num_c<<endl;
    cout<<"JOBS DONE\nEXITING....\n";
    return 0;
}

void *producer(void *param) {
    
    for(int i=0; i<NUM_PROD;i++) {
        pthread_mutex_lock(&lock);
        if(SHARED_BUFFER.size() < M){							// checking for shared buffer fullness
            int x = rand()%1000;
            SHARED_BUFFER.push(x);
            STATUS[pthread_self()].job_done_flag = DONE;
        }
        else {
            STATUS[pthread_self()].job_done_flag = NOT_DONE;
        }
        pthread_mutex_unlock(&lock);
    }
    pthread_mutex_lock(&lock);
    STATUS[pthread_self()].job_done_flag = COMPLETE;			// pushed NUM_PROD numbers. So, its job done
    term_threads++;
    pthread_mutex_unlock(&lock);
    pthread_kill(scheduler, SIGUSR2);							// killing thread
    pthread_exit(0);
}

void *consumer(void *param) {
    while(1) {
        pthread_mutex_lock(&lock2);
        if(!SHARED_BUFFER.empty()) {							// checking for buffer emptyness
            SHARED_BUFFER.pop();
            STATUS[pthread_self()].job_done_flag = DONE;
        }
        else {
            STATUS[pthread_self()].job_done_flag = NOT_DONE;
        }

        if(SHARED_BUFFER.empty() && term_threads >= num_p) {	// All producers are terminated and buffer empty. Hence nothing to do
            STATUS[pthread_self()].job_done_flag = COMPLETE;
            term_threads++;
            pthread_mutex_unlock(&lock2);
            pthread_kill(scheduler, SIGUSR2);
            pthread_exit(0);
        }
        pthread_mutex_unlock(&lock2);
    }
}

void *scheduler_handle(void *param) {

    sleep(TIME_SLICE);
    while(1) {
        if(term_threads == N) pthread_exit(0);
     
        pthread_mutex_lock(&state_lock);

        if(SHARED_BUFFER.empty() && term_threads >= num_p) {	// producers terminated and buffer empty
            while(!CONSUMER_WAITING_QUEUE.empty()) {			// any consumers waiting? If so, pop
                pthread_t t = CONSUMER_WAITING_QUEUE.front();
                CONSUMER_WAITING_QUEUE.pop();
                READY_QUEUE.push(t);
                STATUS[t].current_state = READY;
            }

            while(!PRODUCER_WAITING_QUEUE.empty()) {			// amy producers waiting? If so, pop
                pthread_t t = PRODUCER_WAITING_QUEUE.front();
                PRODUCER_WAITING_QUEUE.pop();
                READY_QUEUE.push(t);
                STATUS[t].current_state = READY;
            }
        }

        if(STATUS[current_t].current_state == RUNNING) {
            if(STATUS[current_t].type == 'P') {					// producer
                if(STATUS[current_t].job_done_flag){			
                    if(STATUS[current_t].job_done_flag != START 
                    				&& !CONSUMER_WAITING_QUEUE.empty()) { // pulling from consumer waiting queue to consume
                        pthread_t t = CONSUMER_WAITING_QUEUE.front();
                        CONSUMER_WAITING_QUEUE.pop();
                        READY_QUEUE.push(t);
                        STATUS[t].current_state = READY;
                    }

                    if(STATUS[current_t].job_done_flag == COMPLETE) {	// if it's job's done
                        STATUS[current_t].current_state = TERMINATED;
                    }
                    else {
                        READY_QUEUE.push(current_t);
                        STATUS[current_t].current_state = READY;
                        pthread_kill(current_t, SIGUSR1);
                    }
                    
                }

                else {											// if its job isn't done
                    STATUS[current_t].current_state = WAITING;
                    PRODUCER_WAITING_QUEUE.push(current_t);
                    pthread_kill(current_t, SIGUSR1);
                }
            }

            else if(STATUS[current_t].type == 'C') {			// consumer
                if(STATUS[current_t].job_done_flag){
                    if(STATUS[current_t].job_done_flag != START && !PRODUCER_WAITING_QUEUE.empty()) {
                        pthread_t t = PRODUCER_WAITING_QUEUE.front();
                        PRODUCER_WAITING_QUEUE.pop();
                        READY_QUEUE.push(t);
                        STATUS[t].current_state = READY;
                    }

                    if(STATUS[current_t].job_done_flag == COMPLETE) {
                        STATUS[current_t].current_state = TERMINATED;
                    }

                    else {
                        READY_QUEUE.push(current_t);
                        STATUS[current_t].current_state = READY;
                        pthread_kill(current_t, SIGUSR1);
                    }

                }

                else {
                    STATUS[current_t].current_state = WAITING;
                    CONSUMER_WAITING_QUEUE.push(current_t);
                    pthread_kill(current_t, SIGUSR1);
                }
            }
            STATUS[current_t].job_done_flag = START;
        }

        if(!READY_QUEUE.empty()){								// if ready queue empty
            current_t = READY_QUEUE.front();
            READY_QUEUE.pop();

            STATUS[current_t].current_state = RUNNING;
            pthread_kill(current_t, SIGUSR2);
        }

        pthread_mutex_unlock(&state_lock);
        sleep(TIME_SLICE);
    }
}

void *reporter_handle(void *param){
    map<pthread_t, struct status>::iterator i;
    while(1){
        sleep(0.1);
        for(i = STATUS.begin(); i!=STATUS.end(); i++) {
            pthread_mutex_lock(&state_lock);
            if(i->second.current_state != i->second.previous_state) {				// context switch
                if(i->second.type == 'P') cout<<"(PRODUCER)";
                else if(i->second.type == 'C') cout<<"(CONSUMER)";
                cout<<" :: Thread_ID: "<<i->first;

                if(i->second.current_state == TERMINATED){
                    cout<<" :: TERMINATED"<<endl;
                }
                else {
                    cout<<" , Changed from: ";
                    if(i->second.previous_state == READY)cout<<"READY  ";
                    else if(i->second.previous_state == WAITING)cout<<"WAITING";
                    else if(i->second.previous_state == RUNNING)cout<<"RUNNING";
                    cout<<" to: ";
                    if(i->second.current_state == READY)cout<<"  READY";
                    else if(i->second.current_state == WAITING)cout<<"WAITING";
                    else if(i->second.current_state == RUNNING)cout<<"RUNNING";
                            
                    cout<<" and numbers in BUFFER ="<<SHARED_BUFFER.size()<<endl;
                }
                i->second.previous_state = i->second.current_state;
            }
            pthread_mutex_unlock(&state_lock);
        }
        if(term_threads == N) {														// all threads terminated. So, exit
            pthread_exit(0);
        }
    }
}

void sleep_handler(int sig){
    sigsuspend(&sleep_mask);
}

void wake_handler(int sig) {
    return;
}

