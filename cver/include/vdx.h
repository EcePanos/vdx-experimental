#ifndef VDX_H
#define VDX_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VDX_MAX_CHOICES 256
#define VDX_MAX_CHOICE_LEN 128

typedef struct {
    double successes;
    double rounds;
} VDXHistoryEntry;

typedef struct {
    VDXHistoryEntry *entries;
    size_t len;
} VDXHistory;

typedef struct {
    double *values;
    size_t len;
} VDXWeights;

void vdx_history_init(VDXHistory *history, size_t n);
void vdx_history_free(VDXHistory *history);
void vdx_weights_init(VDXWeights *weights, size_t n);
void vdx_weights_free(VDXWeights *weights);

double vdx_vote_numeric(VDXHistory *history,
                        VDXWeights *weights,
                        const double *input,
                        size_t n,
                        double error_margin,
                        double scaling_factor,
                        const char *collation,
                        const char *history_algorithm,
                        int bootstrap);

int vdx_vote_alpha(VDXHistory *history,
                   VDXWeights *weights,
                   const char **input,
                   size_t n,
                   const char *history_algorithm,
                   char *out,
                   size_t out_len);

#ifdef __cplusplus
}
#endif

#endif // VDX_H
