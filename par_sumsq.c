/*
 * par_sumsq.c
 *
 * CS 446.646 Project 1 (Pthreads)
 *
 * Compile with --std=c99
 */

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

//Node struct for queue
struct Node
{
    int data;
    struct Node *next;
};

// aggregate variables
long sum = 0;
long odd = 0;
long min = INT_MAX;
long max = INT_MIN;
bool done = false;

//thread availability and count
volatile int available_threads = 0;
long active_thread_num=-1;   //start at -1, first thread is zero

//queue variables
struct Node *root = NULL;
volatile int queuesize=0;

//mutex locks
pthread_mutex_t queue_lock;
pthread_mutex_t aggregate_lock;

//other variables
pthread_cond_t queue_condition = PTHREAD_COND_INITIALIZER;
volatile bool running = 1;

// function prototypes
void calculate_square(long number);
void queuePush(int d);
int queuePeek();
int queuePop();
void *thread();

//main function
int main(int argc, char* argv[])
{
    //initialize mutexes and abort if failed
    if (pthread_mutex_init(&queue_lock, NULL) != 0)
    {
        printf("\n queue mutex init failed\n");
        return 1;
    }

    if (pthread_mutex_init(&aggregate_lock, NULL) != 0)
    {
        printf("\n aggregate mutex init failed\n");
        return 1;
    }

    // check and parse command line options
    if (argc != 3) {
    printf("Usage: sumsq <infile> <thread number>\n");
    exit(EXIT_FAILURE);
    }
    
    // get file name and thread number from command line
    char *fn = argv[1];
    int total_thread_num = atoi(argv[2]);
    pthread_t threads[total_thread_num];

    // load numbers and add them to the queue
    FILE* fin = fopen(fn, "r");
    char action;
    long num;


    while (fscanf(fin, "%c %ld\n", &action, &num) == 2) {
    if (action == 'p') {            // process, do some work
        pthread_mutex_lock(&queue_lock);
        queuePush(num);     //push job onto queue
        bool availableflag=0;

        if(available_threads>0)	//check if any thread is currently available (sleeping), wake it up if there is
        {
            available_threads--;
            pthread_cond_signal(&queue_condition);	//wake up a sleeping thread
            pthread_mutex_unlock(&queue_lock);	//this line might need to be swapped with signal
            availableflag=1;
        }
        else  if(active_thread_num<total_thread_num-1 && !availableflag)	//if no threads currently available, check if we can create a new thread for this job
        {
            pthread_mutex_unlock(&queue_lock);  //the position of this may be wrong
            active_thread_num++;
            pthread_create(&threads[active_thread_num], NULL, &thread, NULL);
        }
        else	//can't process job immediately, so just unlock queue mutex
        {
            pthread_mutex_unlock(&queue_lock);
        }
        

    } else if (action == 'w') {     // wait, nothing new happening
        sleep(num);
    } else {
        printf("ERROR: Unrecognized action: '%c'\n", action);
        exit(EXIT_FAILURE);
    }
    }
    
    bool f=0;
    while(f==0)    //wait until queue is empty and all threads are finished
    {
        pthread_mutex_lock(&queue_lock);
        if(available_threads==active_thread_num+1 && queuesize==0)	//check if available threads is equal to total active threads and queue is empty
        {
            f=1;
        }
        pthread_mutex_unlock(&queue_lock);
    }

    running=0;  //end loops in threads, sleeping threads will exit when signaled


    pthread_mutex_lock(&queue_lock);
    pthread_cond_broadcast(&queue_condition);	//wake up all sleeping threads with broadcast, they exit since running is 0
    pthread_mutex_unlock(&queue_lock);

    
    for(int i=0;i<=active_thread_num;i++)
    {
    	pthread_join(threads[i],NULL);	//join all threads
    }

    // print results
    printf("%ld %ld %ld %ld\n", sum, odd, min, max);

    // clean up and return
    fclose(fin);
    return (0);
}


//update global aggregate variables given a number
void calculate_square(long number)
{

    // calculate the square
    long the_square = number * number;

    // ok that was not so hard, but let's pretend it was
    // simulate how hard it is to square this number!

    sleep(number);
    pthread_mutex_lock(&aggregate_lock);	//obtain aggregate lock since we're changing variables
    // let's add this to our (global) sum
    sum += the_square;

    // now we also tabulate some (meaningless) statistics
    if (number % 2 == 1) {
    // how many of our numbers were odd?
    odd++;
    }

    // what was the smallest one we had to deal with?
    if (number < min) {
    min = number;
    }

    // and what was the biggest one?
    if (number > max) {
    max = number;
    }
    pthread_mutex_unlock(&aggregate_lock);	//release aggregate lock since all changes are finished
}

//push value d onto queue
void queuePush(int d)
{
    struct Node *newnode = (struct Node*)malloc(sizeof(struct Node));	//create a new node
    newnode->next = NULL;
    newnode->data = d;
    if(root==NULL)	//case if queue is empty
    {
        root = newnode;
    }
    else	//if queue is not empty, traverse to last node and insert at end
    {
        struct Node *i = root;
        for(;i->next!=NULL;i=i->next);
        i->next = newnode;
    }
    queuesize++;
}

//returns data at front of queue
int queuePeek()
{
    return root->data;
}

//remove first element of queue and return it
int queuePop()
{
   if(root->next==NULL)
   {
       int data = root->data;
       free(root);
       root = NULL;
       queuesize--;
       return data;
   }
   else
   {
       int data = root->data;
       struct Node *nextnode = root->next;
       free(root);
       root = nextnode;
       queuesize--;
       return data;
   }
}

//function passed to threads
void *thread()
{
    //when a thread is active, it is either currently processing data, or asleep in pthread_cond_wait

    while(running)	//loop until running is set to false by main thread
    {
        pthread_mutex_lock(&queue_lock);	//aquire queue mutex since we are manipulating the queue

        if(root!=NULL)  //if the queue is NOT empty, immediately process request
        {
            int num = queuePop();
            pthread_mutex_unlock(&queue_lock);	//queue manipulation done, unlock queue
            calculate_square(num);
        }
        else    //if queue IS empty, wait until it isnt, unlocking queue_lock in the meantime
        {
            available_threads++;	//thread is now available since there is no data to process
            while(root==NULL && running)
                pthread_cond_wait(&queue_condition,&queue_lock);	//put thread to sleep until main() signals it
            if(running==0)  //after signal, check if still running, if not, don't process anything and exit loop
            {
                break;
            }
            int num = queuePop();
            pthread_mutex_unlock(&queue_lock);	//queue manipulation done, unlock
            calculate_square(num);
        }
    }
    active_thread_num--;    //thread is no longer active
    pthread_mutex_unlock(&queue_lock);  //unlock mutex before exiting
    pthread_exit(NULL);
    return NULL;
}
