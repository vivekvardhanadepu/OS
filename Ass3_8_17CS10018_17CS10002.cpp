/* GROUP-8 */

#include <iostream>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <string>
#include <signal.h>
#include <chrono>				// for time calculations
#include <assert.h>
#include <time.h>

using namespace std;
using namespace std::chrono;

#define Q_SIZE 100
#define MAX_PROCESS 5000

struct shr_mem* shm_ptr;			// pointer for shared memory
int processor_no;
pid_t pid;
pid_t parent_pid;

typedef struct Job{
	pid_t pid;
	int id;
	int priority;
	int processor_no;
	int compute_time;
	struct Job* next;
}job;

struct semaphore {			// for mutex locking
	int s;
	int cnt;
	pid_t q[MAX_PROCESS];
};

typedef struct shr_mem{
	struct semaphore sem;
	int job_created;
	int job_completed;
	job p_queue[Q_SIZE];
	int job_schd;			// num of jobs present in queue
}shr_mem;

void push(pid_t p) {			// used in semaphore process queue
	shm_ptr->sem.q[shm_ptr->sem.cnt++] = p;
}

void pop() {				// used in semaphore process queue
	pid_t t = shm_ptr->sem.q[1];
	for(int i = 1; i < shm_ptr->sem.cnt-1; i++){
		shm_ptr->sem.q[i] = shm_ptr->sem.q[i+1];
	}
	shm_ptr->sem.q[shm_ptr->sem.cnt-1] = -1;
	shm_ptr->sem.cnt--;
	shm_ptr->sem.q[0] = t;
}

void P(){				// down in semaphore 
	if (shm_ptr->sem.s == 1) { 
        shm_ptr->sem.s = 0;
    } 
    else { 
        push(pid);
        while(shm_ptr->sem.q[0] != pid) sleep(0.01);
    } 
}

void V(){			// up in semaphore
	if (shm_ptr->sem.cnt == 0) { 
        shm_ptr->sem.s = 1;
    } 
    else { 
        pop(); 
    }
}

// signal handler to avoid kill of parent
void sigquit_handler (int sig) {
    assert(sig == SIGQUIT);
    pid_t self = getpid();
    if (parent_pid != self) _exit(0);
}

// insertion of jobs in p_queue
void insert() {
		 if(shm_ptr->job_schd == Q_SIZE) return;
		 
		 srand(pid+time(NULL));		// seed for rand()
         job new_job, q;    
         new_job.pid = pid;
         new_job.processor_no = processor_no;
         new_job.priority = rand()%10 + 1;
         new_job.id = rand()%100000 + 1;
         new_job.compute_time = rand()%4 + 1;
         if (shm_ptr->job_schd == 0) {
			shm_ptr->p_queue[0] = new_job;
         } 
         else {
            int i=0;
            while (i < shm_ptr->job_schd && shm_ptr->p_queue[i].priority <= new_job.priority)
               i++;
			if(i == shm_ptr->job_schd) shm_ptr->p_queue[i] = new_job;
            else{
				q = shm_ptr->p_queue[i];
				shm_ptr->p_queue[i] = new_job;
				for(int j=shm_ptr->job_schd;j>i+1;j--) {
					shm_ptr->p_queue[j] = shm_ptr->p_queue[j-1];
				}
				shm_ptr->p_queue[i+1] = q;
			}
         }
		 cout << "producer pid = " << new_job.pid << ", producer processor no = " << new_job.processor_no << ", priority = " 
         						<< new_job.priority << ", job id = " << new_job.id <<", compute time = " << new_job.compute_time << endl;
    	 fflush(stdout);

         shm_ptr->job_schd++;
         shm_ptr->job_created++;
         
}

// deletion from jobs p_queue
void del() {
	if(shm_ptr->job_schd == 0) return;
	job t=shm_ptr->p_queue[0];
	for(int j=0;j<shm_ptr->job_schd-1;j++) {
		shm_ptr->p_queue[j] = shm_ptr->p_queue[j+1];
	}
	 
	 	
	cout << "consumer pid = " << pid << ", consumer no = " << processor_no << ", producer pid = " << t.pid << ", producer no = " << t.processor_no << ", priority = " 
         						<< t.priority << ", job id = " << t.id <<", compute time = " << t.compute_time << endl;
	fflush(stdout);
	shm_ptr->job_schd--;
	shm_ptr->job_completed++;
}

int main(){

	int nj,np,nc;
	cout << "enter the no of producers:\n";
	cin >> np;
	cout << "enter the no of consumers:\n";
	cin >> nc;
	cout << "enter the no of jobs:\n";
	cin >> nj;

	// initializing shared memory [suffix 2 for job_created and job_completed]
	key_t shm_key = ftok("/dev/random",3);
	
	int shmid = shmget(shm_key, sizeof(shr_mem),0666|IPC_CREAT);
	
	shm_ptr = (shr_mem *)shmat(shmid, 0, 0);
	if(shm_ptr==(shr_mem *)-1)perror("shmat");
	
	// initial values of shared variables
	shm_ptr->job_completed = 0;
	shm_ptr->job_created = 0;
	shm_ptr->job_schd = 0;
	shm_ptr->sem.s = 1;
	shm_ptr->sem.cnt = 0;
	for(int i=0;i<MAX_PROCESS; i++)shm_ptr->sem.q[i] = -1;

	// signal handling
	signal(SIGQUIT, sigquit_handler);
	parent_pid = getpid();

	auto start = high_resolution_clock::now(); 

	// creating producer processes
	for(int i = 0; i<np ; i++){
		switch(fork()){
			case 0:
			{
				pid = getpid();
				processor_no = i+1;
				
				while(1){

					sleep(static_cast <float> (rand()) / static_cast <float> (RAND_MAX/3.0));
					
					if(shm_ptr->job_schd >= Q_SIZE || shm_ptr->job_created >= nj)
						continue;
					else{
						P();		// down lock
						if(shm_ptr->job_created >= nj){
							V();
							exit(0);
						} 
						insert();	// insert in job priority queue
						V();		// up lock
					}
					
				}
			}

			case -1:
			{
				string error = "fork failed for producer " + i ;
				perror(error.c_str());// used in semaphore process queuerror.c_str());
				exit(EXIT_FAILURE);
			}

			default :
					;

		}
	}

	// creating consumer processes
	for(int i = 0; i<nc ; i++){
		switch(fork()){
			case 0:
			{
				pid = getpid();
				processor_no = i+1;
				
				while(1){

					sleep(static_cast <float> (rand()) / static_cast <float> (RAND_MAX/3.0));
					P();		// down lock
					if(shm_ptr->job_completed >= nj){
							V();
							exit(0);
					}
					if(shm_ptr->job_schd > 0){
						int sleep_time = shm_ptr->p_queue[0].compute_time;
						del();		// popping job priority queue
						V();		// up lock
						sleep(sleep_time);		// performing job
					}
					else {
						V();
					}

				}
			}

			case -1:
			{
				string error = "fork failed for consumer " + i ;
				perror(error.c_str());
				exit(EXIT_FAILURE);
			}

			default :
					;
		}
	}

	// parent process' exit on completion of jobs
	while(1){
		sleep(2);
		cout<<"Jobs in P_Queue = "<<shm_ptr->job_schd<<"; Jobs Created = "<<shm_ptr->job_created<<"; Jobs Completed = "<<shm_ptr->job_completed<<endl;
		if(shm_ptr->job_completed == nj){
			auto stop = high_resolution_clock::now();
			auto duration = duration_cast<microseconds>(stop - start);
			shmdt(shm_ptr);
			shmctl(shmid,IPC_RMID,0);
			kill(-parent_pid, SIGQUIT);
			cout << "Time taken is(in microseconds): " << duration.count() << endl;
			break;
		}
	}

	return 0;
}