/*
 * sumsq.c
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

volatile bool *available_array;
volatile struct Node *root = NULL;
volatile int queuesize;

pthread_mutex_t queue_lock;
pthread_mutex_t aggregate_lock;
pthread_cond_t queue_condition = PTHREAD_COND_INITIALIZER;
volatile bool running = 1;

// function prototypes
void calculate_square(long number);

/*
 * update global aggregate variables given a number
 */
void calculate_square(long number)
{

    // calculate the square
    long the_square = number * number;

    // ok that was not so hard, but let's pretend it was
    // simulate how hard it is to square this number!

    sleep(number);
    pthread_mutex_lock(&aggregate_lock);
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
    pthread_mutex_unlock(&aggregate_lock);
}

void queuePush(int d)
{
    struct Node *newnode = (struct Node*)malloc(sizeof(struct Node));
    newnode->next = NULL;
    newnode->data = d;

    if(root==NULL)
    {
        root = newnode;
    }
    else
    {
        struct Node *i = root;
        for(;i->next!=NULL;i=i->next);
        i->next = newnode;
    }
    queuesize++;
}

int queuePeek()
{
    return root->data;
}

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

void threadLoop(int t_id)
{
    while(running)
    {
        pthread_mutex_lock(&queue_lock);
        if(root!=NULL)
        {
            int num = queuePop();
            pthread_mutex_unlock(&queue_lock);
            //printf("Thread: %d\nJob: %d\n\n",t_id,num);
            calculate_square(num);

            continue;
        }
        pthread_mutex_unlock(&queue_lock);
    }
}

void thread(int t_id)
{
    while(running)  //when a thread is active, it is either currently processing data, or waiting in pthread_cond_wait
    {
        pthread_mutex_lock(&queue_lock);
        if(root!=NULL)  //if the queue is NOT empty, immediately process request
        {
            printf("\nthread %d queue_notempty\n",t_id);
            available_array[t_id]=0;
            int num = queuePop();
            pthread_mutex_unlock(&queue_lock);
            calculate_square(num);
            printf("Thread: %d finished Job: %d\n\n",t_id,num);
        }
        else    //if queue IS empty, wait until it isnt, unlocking queue_lock in the meantime
        {
            printf("\nthread %d queue_empty, waiting...\n",t_id);
            available_array[t_id] = 1;
            while(root==NULL)
                pthread_cond_wait(&queue_condition,&queue_lock);
            if(running==0)  //check if still running
                continue;
            available_array[t_id]=0;
            printf("thread %d queue_empty, finished waiting\n",t_id);
            int num = queuePop();
            pthread_mutex_unlock(&queue_lock);
            calculate_square(num);
            printf("Thread: %d finished Job: %d\n\n",t_id,num);
        }
    }

}


int main(int argc, char* argv[])
{
    //initialize locks
    if (pthread_mutex_init(&queue_lock, NULL) != 0)
    {
        printf("\n mutex init has failed\n");
        return 1;
    }

    if (pthread_mutex_init(&aggregate_lock, NULL) != 0)
    {
        printf("\n mutex init has failed\n");
        return 1;
    }





    // check and parse command line options
    if (argc != 3) {
    printf("Usage: sumsq <infile> <thread number>\n");
    exit(EXIT_FAILURE);
    }
    char *fn = argv[1];
    int thread_num = atoi(argv[2]);
    pthread_t threads[thread_num];
    int active_thread_num=-1;   //start at -1, first thread is zero

    available_array = malloc(sizeof(bool)*thread_num);
    for(int i=0;i<thread_num;i++)
        available_array[i] = 0;

    // load numbers and add them to the queue
    FILE* fin = fopen(fn, "r");
    char action;
    long num;


    while (fscanf(fin, "%c %ld\n", &action, &num) == 2) {
    if (action == 'p') {            // process, do some work
        pthread_mutex_lock(&queue_lock);
        printf("ADD_TO_QUEUE: %d\n",num);
        queuePush(num);     //push job onto queue
        bool availableflag=0;
        for(int i=0;i<thread_num;i++)
        {
            if(available_array[i])
            {
                printf("%d ",available_array[i]);
                printf("available\n");
                available_array[i]=0;
                pthread_mutex_unlock(&queue_lock);
                pthread_cond_signal(&queue_condition);
                availableflag=1;
                break;
            }
        }

        if(active_thread_num<thread_num-1 && !availableflag)
        {
            printf("create new thread\n");
            pthread_mutex_unlock(&queue_lock);  //the position of this may be wrong
            active_thread_num++;
            pthread_create(&threads[active_thread_num], NULL, &thread, active_thread_num);
        }
        pthread_mutex_unlock(&queue_lock);

    } else if (action == 'w') {     // wait, nothing new happening
        printf("WAIT\n");
        sleep(num);
        printf("ENDWAIT\n");
    } else {
        printf("ERROR: Unrecognized action: '%c'\n", action);
        exit(EXIT_FAILURE);
    }
    }
    printf("Finished\n");
    printf("active threads: %d\n",active_thread_num);

    bool f=0;
    while(f==0)    //wait until queue is empty
    {
        pthread_mutex_lock(&queue_lock);
        //printf("loop\n");
        int counter=-1;
        for(int i=0;i<active_thread_num+1;i++)   //loop over thread availability array to determine if any are active
        {
            if(available_array[i]==1 && queuesize==0)
            {
                //printf("%d ",available_array[i]);
                counter++;
            }
        }
        //printf("counter: %d",counter);
        //printf("\n");
        if(counter==active_thread_num)
        {
            f=1;
        }
        else
        {
            f=0;
        }

        pthread_cond_signal(&queue_condition);
        pthread_cond_signal(&queue_condition);
        pthread_mutex_unlock(&queue_lock);
    }
    printf("END WAIT\nqueue size %d\n",queuesize);
    running=0;  //end loops in threads


    for(int i=0;i<=active_thread_num;i++)    //join threads
    {
        pthread_cancel(threads[i]);
        printf("join\n");
    }

        //printf("s");

    fclose(fin);

    // print results
    printf("%ld %ld %ld %ld\n", sum, odd, min, max);

    // clean up and return
    return (EXIT_SUCCESS);
}