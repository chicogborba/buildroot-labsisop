#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <errno.h>
#include <sched.h>

#ifndef SCHED_IDLE
#define SCHED_IDLE 5
#endif

char *global_buffer;
int global_buffer_size;
volatile int global_ptr = 0;
sem_t binary_semaphore;

typedef struct {
    int thread_id;
    char assigned_char;
} thread_data_t;

long long *thread_scheduling_counts;

void *thread_function(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    int current_thread_id = data->thread_id;
    char my_char = data->assigned_char;

    while (1) {
        if (sem_wait(&binary_semaphore) != 0) {
            perror("Error acquiring semaphore");
            pthread_exit(NULL);
        }

        if (global_ptr >= global_buffer_size) {
            if (sem_post(&binary_semaphore) != 0) {
                perror("Error releasing semaphore before exit");
            }
            break;
        }

        global_buffer[global_ptr] = my_char;
        global_ptr++;
        thread_scheduling_counts[current_thread_id]++;

        if (sem_post(&binary_semaphore) != 0) {
            perror("Error releasing semaphore");
            pthread_exit(NULL);
        }
    }

    pthread_exit(NULL);
}

int get_scheduler_policy(const char *policy_name) {
    if (strcmp(policy_name, "SCHED_FIFO") == 0) {
        return SCHED_FIFO;
    } else if (strcmp(policy_name, "SCHED_RR") == 0) {
        return SCHED_RR;
    } else if (strcmp(policy_name, "SCHED_IDLE") == 0) {
        return SCHED_IDLE;
    } else if (strcmp(policy_name, "SCHED_OTHER") == 0) {
        return SCHED_OTHER;
    } 
    else {
        fprintf(stderr, "Warning: Unknown scheduling policy '%s'. Using SCHED_OTHER as default.\n", policy_name);
        return SCHED_OTHER;
    }
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <buffer_size> <num_threads> <policy>\n", argv[0]);
        fprintf(stderr, "Policies: SCHED_FIFO, SCHED_RR, SCHED_IDLE, SCHED_OTHER (and potentially SCHED_LOW_IDLE if supported)\n");
        return 1;
    }

    global_buffer_size = atoi(argv[1]);
    int num_threads = atoi(argv[2]);
    const char *policy_name = argv[3];
    int sched_policy = get_scheduler_policy(policy_name);

    if (global_buffer_size <= 0 || num_threads <= 0) {
        fprintf(stderr, "Buffer size and number of threads must be positive integers.\n");
        return 1;
    }

    if (num_threads > 26) {
        fprintf(stderr, "Warning: Maximum 26 threads supported for character assignment (A-Z).\n");
        num_threads = 26;
    }

    global_buffer = (char *)malloc(global_buffer_size + 1); 
    if (global_buffer == NULL) {
        perror("Failed to allocate global buffer");
        return 1;
    }
    memset(global_buffer, '\0', global_buffer_size + 1); 

    thread_scheduling_counts = (long long *)calloc(num_threads, sizeof(long long));
    if (thread_scheduling_counts == NULL) {
        perror("Failed to allocate thread scheduling counts array");
        free(global_buffer);
        return 1;
    }

    if (sem_init(&binary_semaphore, 0, 1) != 0) {
        perror("Failed to initialize semaphore");
        free(global_buffer);
        free(thread_scheduling_counts);
        return 1;
    }

    pthread_t *threads = (pthread_t *)malloc(num_threads * sizeof(pthread_t));
    thread_data_t *thread_data = (thread_data_t *)malloc(num_threads * sizeof(thread_data_t));

    if (threads == NULL || thread_data == NULL) {
        perror("Failed to allocate thread resources");
        free(global_buffer);
        free(thread_scheduling_counts);
        sem_destroy(&binary_semaphore);
        return 1;
    }

    int max_priority = sched_get_priority_max(sched_policy);
    int min_priority = sched_get_priority_min(sched_policy);
    
    int thread_priority = 0; 
    if (sched_policy == SCHED_FIFO || sched_policy == SCHED_RR) {
        thread_priority = (max_priority + min_priority) / 2;
        if (thread_priority == 0 && max_priority > 0) thread_priority = 1;
        if (thread_priority < min_priority) thread_priority = min_priority;
        if (thread_priority > max_priority) thread_priority = max_priority;
    }


    for (int i = 0; i < num_threads; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].assigned_char = 'A' + i;

        pthread_attr_t attr;
        if (pthread_attr_init(&attr) != 0) {
            fprintf(stderr, "Error initializing thread attributes for thread %d: %s\n", i, strerror(errno));
            goto cleanup;
        }

        if (pthread_attr_setschedpolicy(&attr, sched_policy) != 0) {
            fprintf(stderr, "Error setting scheduling policy for thread %d: %s\n", i, strerror(errno));
            pthread_attr_destroy(&attr);
            goto cleanup;
        }

        struct sched_param param;
        param.sched_priority = thread_priority;
        if (pthread_attr_setschedparam(&attr, &param) != 0) {
            fprintf(stderr, "Error setting scheduling parameters for thread %d: %s\n", i, strerror(errno));
            pthread_attr_destroy(&attr);
            goto cleanup;
        }


        if (pthread_create(&threads[i], &attr, thread_function, (void *)&thread_data[i]) != 0) {
            fprintf(stderr, "Error creating thread %d: %s\n", i, strerror(errno));
            pthread_attr_destroy(&attr);
            goto cleanup;
        }

        pthread_attr_destroy(&attr);
    }

    for (int i = 0; i < num_threads; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            fprintf(stderr, "Error joining thread %d: %s\n", i, strerror(errno));
        }
    }

    printf("Raw Buffer Output:\n");
    printf("%s\n\n", global_buffer);

    printf("Post-processed Buffer Output:\n");
    if (global_buffer_size > 0) {
        char last_char = '\0';
        for (int i = 0; i < global_buffer_size; i++) {
            if (global_buffer[i] != last_char) {
                printf("%c", global_buffer[i]);
                last_char = global_buffer[i];
            }
        }
    }
    printf("\n\n");

    printf("Thread Scheduling Counts:\n");
    for (int i = 0; i < num_threads; i++) {
        printf("%c = %lld\n", 'A' + i, thread_scheduling_counts[i]);
    }

cleanup:
    sem_destroy(&binary_semaphore);
    free(global_buffer);
    free(thread_scheduling_counts);
    free(threads);
    free(thread_data);

    return 0;
}