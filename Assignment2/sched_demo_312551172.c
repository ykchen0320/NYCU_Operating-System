#define _GNU_SOURCE
#include <getopt.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
pthread_barrier_t barrier;
float t;
typedef struct {
  pthread_t thread_id;
  int thread_num;
  int sched_policy;
  int sched_priority;
} thread_info_t;
void *thread_function(void *arg) {
  thread_info_t *thread_info = (thread_info_t *)arg;
  /* 1. Wait until all threads are ready */
  pthread_barrier_wait(&barrier);
  /* 2. Do the task */
  for (int i = 0; i < 3; i++) {
    printf("Thread %d is running\n", thread_info->thread_num);
    clock_t start_time = clock();
    clock_t wait_time = t * CLOCKS_PER_SEC;
    while ((clock() - start_time) < wait_time) {
    }
  }
  /* 3. Exit the function  */
  pthread_exit(NULL);
}
int main(int argc, char *argv[]) {
  // 1. Parse program arguments
  int opt, n;
  char *temp_s, *temp_p;
  while ((opt = getopt(argc, argv, "n:t:s:p:")) != -1) {
    switch (opt) {
      case 'n':
        n = atoi(optarg);
        break;
      case 't':
        t = atof(optarg);
        break;
      case 's':
        temp_s = optarg;
        break;
      case 'p':
        temp_p = optarg;
        break;
      default:
        printf("error opterr: %d\n", opterr);
        break;
    }
  }
  char *s[n];
  int p[n];
  char *token = strtok(temp_s, ",");
  for (int i = 0; i < n; i++) {
    s[i] = token;
    token = strtok(NULL, ",");
  }
  token = strtok(temp_p, ",");
  for (int i = 0; i < n; i++) {
    p[i] = atoi(token);
    token = strtok(NULL, ",");
  }
  // 2. Create <num_threads> worker threads
  thread_info_t thr[n];
  for (int i = 0; i < n; i++) {
    thr[i].thread_num = i;
    if (!strcmp(s[i], "FIFO")) {
      thr[i].sched_policy = SCHED_FIFO;
    } else {
      thr[i].sched_policy = SCHED_OTHER;
    }
    thr[i].sched_priority = p[i];
  }
  // 3. Set CPU affinity
  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(0, &mask);
  pthread_barrier_init(&barrier, NULL, n);
  for (int i = 0; i < n; i++) {
    // 4. Set the attributes to each thread
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    if ((pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &mask)) != 0) {
      perror("attr setaffinity:");
    }
    if ((pthread_attr_setschedpolicy(&attr, thr[i].sched_policy)) != 0) {
      perror("attr_setschedpolicy:");
    }
    struct sched_param param;
    param.sched_priority = thr[i].sched_priority;
    pthread_attr_setschedparam(&attr, &param);
    if ((pthread_create(&thr[i].thread_id, &attr, thread_function, &thr[i])) !=
        0) {
      perror("pthread create");
    }
    pthread_attr_destroy(&attr);
  }
  // 5. Start all threads at once
  // 6. Wait for all threads to finish
  for (int i = 0; i < n; i++) {
    if (pthread_join(thr[i].thread_id, NULL)) {
      perror("pthread join");
    }
  }
  pthread_barrier_destroy(&barrier);
  return 0;
}