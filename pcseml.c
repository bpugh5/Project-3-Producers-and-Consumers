#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <fcntl.h>
#include "eventbuf.h"

int producers;
int consumers;
int producer_events;
int max_events;
sem_t *items_sem;
sem_t *events_sem;
sem_t *mutex_sem;
struct eventbuf *event_buffer;

sem_t *sem_open_temp(const char *name, int value)
{
    sem_t *sem;

    // Create the semaphore
    if ((sem = sem_open(name, O_CREAT, 0600, value)) == SEM_FAILED)
        return SEM_FAILED;

    // Unlink it so it will go away after this process exits
    if (sem_unlink(name) == -1) {
        sem_close(sem);
        return SEM_FAILED;
    }

    return sem;
}

void *consumer_run(void *consumer_id) {
    int *id = consumer_id;
    while (1) {
        sem_wait(items_sem);
        sem_wait(mutex_sem);

        if (eventbuf_empty(event_buffer) == 1) {
            sem_post(mutex_sem);
            break;
        }
        
        int event_number = eventbuf_get(event_buffer);

        printf("C%d: got event %d\n", *id, event_number);

        sem_post(mutex_sem);
        sem_post(events_sem);
    }

    printf("C%d: exiting\n", *id);

    return NULL;

}

void *producer_run(void *producer_id) {
    int* id = producer_id;
    
    for (int j = 0; j < producer_events; j++) {
        int event_number = (*id * 100) + j;
        sem_wait(events_sem);
        sem_wait(mutex_sem);

        printf("P%d: adding event %d\n", *id, event_number);
        
        eventbuf_add(event_buffer, event_number);
        
        sem_post(mutex_sem);
        sem_post(items_sem);
    }

    printf("P%d: exiting\n", *id);

    return NULL;

}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "usage: producers consumers producer_events max_events\n");
    }

    producers = atoi(argv[1]);
    consumers = atoi(argv[2]);
    producer_events = atoi(argv[3]);
    max_events = atoi(argv[4]);

    events_sem = sem_open_temp("events_sem", max_events);
    items_sem = sem_open_temp("items_sem", 0);
    mutex_sem = sem_open_temp("mutex_sem", 1);

    event_buffer = eventbuf_create();

    int *producer_thread_id = calloc(producers, sizeof *producer_thread_id);
    int *consumer_thread_id = calloc(consumers, sizeof *consumer_thread_id);

    pthread_t *producer_thread = calloc(producers, sizeof *producer_thread);
    pthread_t *consumer_thread = calloc(consumers, sizeof *consumer_thread);

    for (int i = 0; i < producers; i++) {
        producer_thread_id[i] = i;
        pthread_create(producer_thread + i, NULL, producer_run, producer_thread_id + i);
    }
    
    for (int i = 0; i < consumers; i++) {
        consumer_thread_id[i] = i;
        pthread_create(consumer_thread + i, NULL, consumer_run, consumer_thread_id + i);
    }

    for (int i = 0; i < producers; i++) {
        pthread_join(producer_thread[i], NULL);
    }

    for (int i = 0; i < consumers; i++) {
        sem_post(items_sem);
    }

    for (int i = 0; i < consumers; i++) {
        pthread_join(consumer_thread[i], NULL);
    }

    eventbuf_free(event_buffer);
}