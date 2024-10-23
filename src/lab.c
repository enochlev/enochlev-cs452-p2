#include "lab.h"
#include <pthread.h> 

// Define the structure for the queue
struct queue {
    void **data;
    int capacity;
    int size;
    int front;
    int rear;
    bool shutdown;

    //we need 3 locks
    //1 for a queue itself so it does not lock
    //2 if the queue is full and the producer needs to wait
    //3 if the queue is empty and the consumer needs to wait
    pthread_mutex_t mutex; 
    pthread_cond_t not_full;
    pthread_cond_t not_empty; 
};

queue_t queue_init(int capacity) {
    //https://stackoverflow.com/questions/40205917/using-sizeofvoid-inside-malloc
    queue_t q = (queue_t)malloc(sizeof(struct queue));
    q->data = (void **)malloc(sizeof(void *) * capacity);
    q->capacity = capacity;
    q->size = 0;
    q->front = 0;
    q->rear = -1;
    q->shutdown = false;

    // Initialize the mutex and condition variables
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->not_full, NULL);
    pthread_cond_init(&q->not_empty, NULL);

    return q;
}

void queue_destroy(queue_t q) {
    free(q->data);
    free(q);

    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->not_full);
    pthread_cond_destroy(&q->not_empty);
}

void enqueue(queue_t q, void *data) {
    
    //lock the queue first thing
    pthread_mutex_lock(&q->mutex);

    //check if queue is full to prevent new producers from adding items
    //also check if shutdown has been initiated
    while (q->size >= q->capacity && !q->shutdown) {
        //https://hpc-tutorials.llnl.gov/posix/
        pthread_cond_wait(&q->not_full, &q->mutex);//unlock mutex in a cascade manner also prevent deadlocks
    }

    
    if (q->shutdown) {
        // If shutdown has been initiated, do not accept new items
        pthread_mutex_unlock(&q->mutex);
        return;
    }

    // circular queue aka wrap around
    q->rear = (q->rear + 1) % q->capacity;
    q->data[q->rear] = data;
    q->size++;

    //release the lock
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
}

void *dequeue(queue_t q) {
    //first thing is to lock the queue
    pthread_mutex_lock(&q->mutex);

    //if the consumer is ready but queue is empty, then just simply wait
    while (q->size <= 0 && !q->shutdown) {
        pthread_cond_wait(&q->not_empty, &q->mutex);//unlock mutex in a cascade manner
    }

    if (q->size <= 0 && q->shutdown) {
        // If the queue is empty and shutdown has been initiated, return NULL
        pthread_cond_signal(&q->not_full);
        pthread_mutex_unlock(&q->mutex);
        return NULL;
    }

    //remove the item from the queue
    void *data = q->data[q->front];
    q->front = (q->front + 1) % q->capacity;
    q->size--;

    //signal that the queue is not full
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);

    return data;
}

void queue_shutdown(queue_t q) {
    pthread_mutex_lock(&q->mutex);
    q->shutdown = true;

    // Wake up all waiting threads at once
    // https://pages.cs.wisc.edu/~remzi/OSTEP/threads-cv.pdf
    pthread_cond_broadcast(&q->not_full);
    pthread_cond_broadcast(&q->not_empty);

    pthread_mutex_unlock(&q->mutex);
}

bool is_empty(queue_t q) {
    bool empty = (q->size == 0);
    return empty;
}

bool is_shutdown(queue_t q) {
    pthread_mutex_lock(&q->mutex);
    bool shutdown = q->shutdown;
    pthread_mutex_unlock(&q->mutex);
    return shutdown;
}