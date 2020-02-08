/* Round robin scheduler */

#include <iostream>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <queue>

#define N 300
#define M 1000000
#define NUM_PROD 1000
#define SIGUSR1 101
#define SIGUSR2 102

using namespace std;

queue<int> shared_buffer;
pthread_mutex_t lock;
pthread_t worker[N], scheduler, reporter;

void *producer(void *param);
void *consumer(void *param);
void *scheduler_handle(void *param);
void *reporter_handle(void *param);

void sleep_handler(int sig);
void wake_handler(int sig);

int main() {
    pthread_attr_t attr;

    pthread_attr_init (&attr); // get default attributes
    //pthread_join(tid, NULL); //wait for the thread to exit
    signal(SIGUSR1, sleep_handler);
    signal(SIGUSR2, wake_handler);
    
    if (pthread_mutex_init(&lock, NULL) != 0) { 
        printf("\n mutex init has failed\n"); 
        return 1; 
    }

    for(int i=0; i < N; i++) {
        srand(2*i*time(NULL));
        int x = rand() % 20;
        if(x < 10) pthread_create(&worker[i], &attr, producer, NULL);
        else pthread_create(&worker[i], &attr, consumer, NULL);
    }

    pthread_create(&scheduler, &attr, scheduler_handle, NULL);
    pthread_create(&reporter, &attr, reporter_handle, NULL);

    for(int i=0; i < N; i++) {
        pthread_join(worker[i], NULL);
    }
    pthread_mutex_destroy(&lock);
    
}

void *producer(void *param) {
    while(1){
        for(int i=0; i<NUM_PROD;i++) {
            pthread_mutex_lock(&lock);
            if(shared_buffer.size() < M){
                srand(2*i*time(NULL));
                int x = rand();
                shared_buffer.push(x);
                pthread_mutex_lock(&lock);
            }
            else {
                pthread_mutex_unlock(&lock);
                while(shared_buffer.size() >= M) sleep(0.001);
                pthread_mutex_lock(&lock);
                srand(2*i*time(NULL));
                int x = rand();
                shared_buffer.push(x);
                pthread_mutex_unlock(&lock);
            }
        }
    }
}

void *consumer(void *param) {
    while(1) {
        pthread_mutex_lock(&lock);
        if(!shared_buffer.empty()) {
            shared_buffer.pop();
            pthread_mutex_unlock(&lock);
        }
        else {
            pthread_mutex_unlock(&lock);
            while(shared_buffer.empty()) sleep(0.001);
        }
    }
}

void sleep_handler(int sig){
    sleep();
}

void wake_handler(int sig) {
    //();
}