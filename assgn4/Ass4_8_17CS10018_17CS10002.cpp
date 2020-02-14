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

#define N 10
#define M 1000000
#define NUM_PROD 1000
#define TIME_SLICE 1

#define RUNNING 1
#define READY 2
#define WAITING 3
#define TERMINATED 0

using namespace std;

struct status {
    char type[10];
	pthread_t tid;
	int current_state;
	int previous_state;
};

queue<int> SHARED_BUFFER;

queue<pthread_t> READY_QUEUE;
queue<pthread_t> PRODUCER_WAITING_QUEUE;
queue<pthread_t> CONSUMER_WAITING_QUEUE;

pthread_t current_t;

pthread_mutex_t lock, lock2;
pthread_t worker[N], scheduler, reporter;
map<pthread_t,struct status> STATUS;

sigset_t sleep_mask;

void *producer(void *param);
void *consumer(void *param);
void *scheduler_handle(void *param);
void *reporter_handle(void *param);

void sleep_handler(int sig);
void wake_handler(int sig);

int main() {
    signal(SIGUSR1, sleep_handler);
	signal(SIGUSR2, wake_handler);

	sigemptyset(&sleep_mask);
	sigaddset(&sleep_mask, SIGUSR1);

    pthread_attr_t attr;

    pthread_attr_init (&attr);

    if (pthread_mutex_init(&lock, NULL) != 0) { 
        printf("\n mutex init has failed\n"); 
        return 1; 
    }
    if (pthread_mutex_init(&lock2, NULL) != 0) { 
        printf("\n mutex init has failed\n"); 
        return 1; 
    }
    struct status temp;
    int p=0,c=0;
    for(int i=0; i < N; i++) {
        srand(2*i*time(NULL));
        int x = rand() % 20;
        if(x < 10) {
            if(pthread_create(&worker[i], &attr, producer, NULL) != 0) {
                perror("thread creation failed");
            }
            memset(temp.type, 0, 10);
            memcpy(temp.type, "PRODUCER", strlen("PRODUCER"));
            p++;
        }
        else {
            if(pthread_create(&worker[i], &attr, consumer, NULL) != 0) {
                perror("thread creation failed");
            }
            memset(temp.type, 0, 10);
            memcpy(temp.type, "CONSUMER", strlen("CONSUMER"));
            c++;
        }
        if(i==0) {
            current_t = worker[i];
            temp.tid = worker[i];
            temp.current_state = RUNNING;
            temp.previous_state = RUNNING;
        }
        else {
            pthread_kill(worker[i], SIGUSR1);
            READY_QUEUE.push(worker[i]);
            temp.tid = worker[i];
            temp.current_state = READY;
            temp.previous_state = READY;
        }
        STATUS.insert({worker[i], temp});
        cout<<"thread created :"<<i+1<<endl;
    }
    cout<<p<<" ; "<<c<<endl;
    pthread_create(&scheduler, &attr, scheduler_handle, NULL);
    pthread_create(&reporter, &attr, reporter_handle, NULL);

    for(int i=0; i < N; i++) {
        pthread_join(worker[i], NULL);
    }
    pthread_join(scheduler, NULL);
    pthread_join(reporter, NULL);
    pthread_mutex_destroy(&lock);
    pthread_mutex_destroy(&lock2);
    
    return 0;
}

void *producer(void *param) {
   
    for(int i=0; i<NUM_PROD;i++) {
        pthread_mutex_lock(&lock);
        if(SHARED_BUFFER.size() < M){
            srand(2*i*time(NULL));
            int x = rand();
            SHARED_BUFFER.push(x);
            if(!CONSUMER_WAITING_QUEUE.empty()) {
                pthread_t t = CONSUMER_WAITING_QUEUE.front();
                CONSUMER_WAITING_QUEUE.pop();
                READY_QUEUE.push(t);
                STATUS[t].current_state = READY;
            }
        }
        else {
            STATUS[pthread_self()].current_state = WAITING;
            PRODUCER_WAITING_QUEUE.push(pthread_self());
        }
        pthread_mutex_unlock(&lock);
    }

    STATUS[pthread_self()].current_state = TERMINATED;
    pthread_exit(0);
}

void *consumer(void *param) {
    while(1) {
        pthread_mutex_lock(&lock2);
        if(!SHARED_BUFFER.empty()) {
            SHARED_BUFFER.pop();
            if(!PRODUCER_WAITING_QUEUE.empty()) {
                pthread_t t = PRODUCER_WAITING_QUEUE.front();
                PRODUCER_WAITING_QUEUE.pop();
                READY_QUEUE.push(t);
                STATUS[t].current_state = READY;
            }
            
        }
        else {
            STATUS[pthread_self()].current_state = WAITING;
            CONSUMER_WAITING_QUEUE.push(pthread_self());
        }
        pthread_mutex_unlock(&lock2);
    }
}

void *scheduler_handle(void *param) {

    sleep(TIME_SLICE);
    while(1) {
        if(STATUS[current_t].current_state == RUNNING) {
            READY_QUEUE.push(current_t);
            STATUS[current_t].current_state = READY;
            pthread_kill(current_t, SIGUSR1);
        }

        else if(STATUS[current_t].current_state == WAITING) {
            pthread_kill(current_t, SIGUSR1);
        }

        if(!READY_QUEUE.empty()){
            current_t = READY_QUEUE.front();
            READY_QUEUE.pop();

            STATUS[current_t].current_state = RUNNING;
            pthread_kill(current_t, SIGUSR2);
            sleep(TIME_SLICE);
        }

        else {
            while(READY_QUEUE.empty()) sleep(0.5);
        }
    }
}

void *reporter_handle(void *param){
    map<pthread_t, struct status>::iterator i;
    while(1){
        sleep(0.1);
        for(i = STATUS.begin(); i!=STATUS.end(); i++) {
            if(i->second.current_state != i->second.previous_state) {
                if(i->second.current_state == TERMINATED){
                    cout<<i->second.type<<" , Thread_ID: "<<i->first<<" :: TERMINATED"<<endl;
                }
                else {
                    cout<<i->second.type<<" , Thread_ID: "<<i->second.tid<<" , Changed from: ";
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
        }
    }
}

void sleep_handler(int sig){
    sigsuspend(&sleep_mask);
}

void wake_handler(int sig) {
    return;
}

