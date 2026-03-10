#include "vdx.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

static int parse_csv_line(const char *line, double **out_vals, size_t *out_len) {
    if (!line || !out_vals || !out_len) {
        return -1;
    }
    char *copy = strdup(line);
    if (!copy) {
        return -1;
    }
    size_t cap = 8;
    size_t len = 0;
    double *vals = (double *)malloc(cap * sizeof(double));
    if (!vals) {
        free(copy);
        return -1;
    }

    char *saveptr = NULL;
    char *token = strtok_r(copy, ",", &saveptr);
    while (token) {
        char *endptr = NULL;
        errno = 0;
        double v = strtod(token, &endptr);
        if (endptr == token || errno != 0) {
            free(vals);
            free(copy);
            return -1;
        }
        if (len == cap) {
            size_t new_cap = cap * 2;
            double *new_vals = (double *)realloc(vals, new_cap * sizeof(double));
            if (!new_vals) {
                free(vals);
                free(copy);
                return -1;
            }
            vals = new_vals;
            cap = new_cap;
        }
        vals[len++] = v;
        token = strtok_r(NULL, ",", &saveptr);
    }

    free(copy);
    *out_vals = vals;
    *out_len = len;
    return 0;
}

typedef struct Job {
    size_t index;
    double *values;
    size_t n;
} Job;

typedef struct JobNode {
    Job job;
    struct JobNode *next;
} JobNode;

typedef struct {
    JobNode *head;
    JobNode *tail;
    size_t len;
    size_t max;
    int closed;
    pthread_mutex_t mu;
    pthread_cond_t cv;
    pthread_cond_t cv_not_full;
} JobQueue;

typedef struct {
    double *values;
    char *ready;
    size_t cap;
    size_t next_index;
    size_t total_jobs;
    size_t total_done;
    int jobs_done;
    FILE *out;
    pthread_mutex_t mu;
    pthread_cond_t cv;
} ResultBuffer;

static void jobqueue_init(JobQueue *q, size_t max) {
    q->head = NULL;
    q->tail = NULL;
    q->len = 0;
    q->max = max;
    q->closed = 0;
    pthread_mutex_init(&q->mu, NULL);
    pthread_cond_init(&q->cv, NULL);
    pthread_cond_init(&q->cv_not_full, NULL);
}

static void jobqueue_close(JobQueue *q) {
    pthread_mutex_lock(&q->mu);
    q->closed = 1;
    pthread_cond_broadcast(&q->cv);
    pthread_cond_broadcast(&q->cv_not_full);
    pthread_mutex_unlock(&q->mu);
}

static int jobqueue_push(JobQueue *q, Job job) {
    JobNode *node = (JobNode *)malloc(sizeof(JobNode));
    if (!node) {
        return 0;
    }
    node->job = job;
    node->next = NULL;
    pthread_mutex_lock(&q->mu);
    while (!q->closed && q->max > 0 && q->len >= q->max) {
        pthread_cond_wait(&q->cv_not_full, &q->mu);
    }
    if (q->closed) {
        pthread_mutex_unlock(&q->mu);
        free(node);
        return 0;
    }
    if (q->tail) {
        q->tail->next = node;
    } else {
        q->head = node;
    }
    q->tail = node;
    q->len++;
    pthread_cond_signal(&q->cv);
    pthread_mutex_unlock(&q->mu);
    return 1;
}

static int jobqueue_pop(JobQueue *q, Job *out) {
    pthread_mutex_lock(&q->mu);
    while (!q->head && !q->closed) {
        pthread_cond_wait(&q->cv, &q->mu);
    }
    if (!q->head && q->closed) {
        pthread_mutex_unlock(&q->mu);
        return 0;
    }
    JobNode *node = q->head;
    q->head = node->next;
    if (!q->head) {
        q->tail = NULL;
    }
    q->len--;
    pthread_cond_signal(&q->cv_not_full);
    pthread_mutex_unlock(&q->mu);
    *out = node->job;
    free(node);
    return 1;
}

static void resultbuffer_init(ResultBuffer *rb, FILE *out) {
    rb->values = NULL;
    rb->ready = NULL;
    rb->cap = 0;
    rb->next_index = 0;
    rb->total_jobs = 0;
    rb->total_done = 0;
    rb->jobs_done = 0;
    rb->out = out;
    pthread_mutex_init(&rb->mu, NULL);
    pthread_cond_init(&rb->cv, NULL);
}

static void resultbuffer_free(ResultBuffer *rb) {
    free(rb->values);
    free(rb->ready);
    rb->values = NULL;
    rb->ready = NULL;
    rb->cap = 0;
}

static void resultbuffer_ensure(ResultBuffer *rb, size_t index) {
    if (index < rb->cap) {
        return;
    }
    size_t new_cap = rb->cap == 0 ? 64 : rb->cap;
    while (new_cap <= index) {
        new_cap *= 2;
    }
    double *new_vals = (double *)realloc(rb->values, new_cap * sizeof(double));
    if (!new_vals) {
        return;
    }
    char *new_ready = (char *)realloc(rb->ready, new_cap * sizeof(char));
    if (!new_ready) {
        rb->values = new_vals;
        return;
    }
    for (size_t i = rb->cap; i < new_cap; i++) {
        new_ready[i] = 0;
    }
    rb->values = new_vals;
    rb->ready = new_ready;
    rb->cap = new_cap;
}

typedef struct {
    JobQueue *queue;
    ResultBuffer *results;
} WorkerArgs;

static void *worker_thread(void *arg) {
    WorkerArgs *wa = (WorkerArgs *)arg;
    Job job;
    while (jobqueue_pop(wa->queue, &job)) {
        VDXHistory history = {0};
        VDXWeights weights = {0};
        vdx_history_init(&history, job.n);
        vdx_weights_init(&weights, job.n);
        double result = vdx_vote_numeric(&history,
                                         &weights,
                                         job.values,
                                         job.n,
                                         0.05,
                                         2.0,
                                         "nearest_neighbor",
                                         "history_based_hybrid_voting",
                                         1);
        vdx_history_free(&history);
        vdx_weights_free(&weights);

        pthread_mutex_lock(&wa->results->mu);
        resultbuffer_ensure(wa->results, job.index);
        wa->results->values[job.index] = result;
        wa->results->ready[job.index] = 1;
        wa->results->total_done++;
        pthread_cond_signal(&wa->results->cv);
        pthread_mutex_unlock(&wa->results->mu);

        free(job.values);
    }
    return NULL;
}

static void *writer_thread(void *arg) {
    ResultBuffer *rb = (ResultBuffer *)arg;
    pthread_mutex_lock(&rb->mu);
    for (;;) {
        while (rb->next_index < rb->cap && rb->ready[rb->next_index]) {
            fprintf(rb->out, "%f\n", rb->values[rb->next_index]);
            rb->ready[rb->next_index] = 0;
            rb->next_index++;
        }
        if (rb->jobs_done && rb->next_index == rb->total_jobs) {
            pthread_mutex_unlock(&rb->mu);
            return NULL;
        }
        pthread_cond_wait(&rb->cv, &rb->mu);
    }
}

int main(int argc, char **argv) {
    int num_workers = (int)sysconf(_SC_NPROCESSORS_ONLN);
    const char *input_path = NULL;
    const char *output_path = "output.csv";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-j") == 0 && i + 1 < argc) {
            num_workers = (int)strtol(argv[++i], NULL, 10);
        } else if (!input_path) {
            input_path = argv[i];
        } else if (strcmp(output_path, "output.csv") == 0) {
            output_path = argv[i];
        }
    }

    if (!input_path) {
        fprintf(stderr, "Usage: %s [-j N] <input.csv> [output.csv]\n", argv[0]);
        return 1;
    }
    if (num_workers <= 0) {
        num_workers = 1;
    }

    FILE *in = fopen(input_path, "r");
    if (!in) {
        fprintf(stderr, "Failed to open input: %s\n", input_path);
        return 1;
    }
    FILE *out = fopen(output_path, "w");
    if (!out) {
        fprintf(stderr, "Failed to open output: %s\n", output_path);
        fclose(in);
        return 1;
    }

    JobQueue queue;
    jobqueue_init(&queue, 1024);
    ResultBuffer results;
    resultbuffer_init(&results, out);

    pthread_t writer;
    pthread_create(&writer, NULL, writer_thread, &results);

    pthread_t *threads = (pthread_t *)malloc((size_t)num_workers * sizeof(pthread_t));
    WorkerArgs args = { .queue = &queue, .results = &results };
    for (int i = 0; i < num_workers; i++) {
        pthread_create(&threads[i], NULL, worker_thread, &args);
    }

    char *line = NULL;
    size_t cap = 0;
    ssize_t read = 0;
    size_t idx = 0;
    while ((read = getline(&line, &cap, in)) != -1) {
        if (read == 0) {
            continue;
        }
        double *values = NULL;
        size_t n = 0;
        if (parse_csv_line(line, &values, &n) != 0 || n == 0) {
            free(values);
            continue;
        }
        Job job = { .index = idx++, .values = values, .n = n };
        if (!jobqueue_push(&queue, job)) {
            free(values);
        }
    }
    free(line);
    fclose(in);

    jobqueue_close(&queue);
    for (int i = 0; i < num_workers; i++) {
        pthread_join(threads[i], NULL);
    }
    free(threads);

    pthread_mutex_lock(&results.mu);
    results.total_jobs = idx;
    results.jobs_done = 1;
    pthread_cond_signal(&results.cv);
    pthread_mutex_unlock(&results.mu);

    pthread_join(writer, NULL);
    resultbuffer_free(&results);
    fclose(out);
    return 0;
}
