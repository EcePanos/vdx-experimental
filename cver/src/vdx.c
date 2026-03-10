#include "vdx.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static double sum_array(const double *arr, size_t n) {
    double total = 0.0;
    for (size_t i = 0; i < n; i++) {
        total += arr[i];
    }
    return total;
}

static double average_array(const double *arr, size_t n) {
    if (n == 0) {
        return 0.0;
    }
    return sum_array(arr, n) / (double)n;
}

static double nearest_neighbor(const double *arr, size_t n, double target) {
    if (n == 0) {
        return 0.0;
    }
    double nearest = arr[0];
    double min_diff = fabs(target - nearest);
    for (size_t i = 1; i < n; i++) {
        double diff = fabs(target - arr[i]);
        if (diff < min_diff) {
            min_diff = diff;
            nearest = arr[i];
        }
    }
    return nearest;
}

static double weighted_average(const double *arr, const double *weights, size_t n) {
    if (n == 0) {
        return 0.0;
    }
    double weighted_sum = 0.0;
    double total_weight = 0.0;
    for (size_t i = 0; i < n; i++) {
        weighted_sum += arr[i] * weights[i];
        total_weight += weights[i];
    }
    if (total_weight == 0.0) {
        return 0.0;
    }
    return weighted_sum / total_weight;
}

static int weighted_majority_voting(const char **choices,
                                    const double *weights,
                                    size_t n,
                                    char *result,
                                    size_t result_len) {
    if (n == 0 || result_len == 0) {
        return -1;
    }

    double vote_count[VDX_MAX_CHOICES];
    const char *unique_choices[VDX_MAX_CHOICES];
    size_t unique_count = 0;

    for (size_t i = 0; i < n; i++) {
        size_t found = unique_count;
        for (size_t j = 0; j < unique_count; j++) {
            if (strcmp(choices[i], unique_choices[j]) == 0) {
                found = j;
                break;
            }
        }
        if (found == unique_count) {
            if (unique_count >= VDX_MAX_CHOICES) {
                return -1;
            }
            unique_choices[unique_count] = choices[i];
            vote_count[unique_count] = weights[i];
            unique_count++;
        } else {
            vote_count[found] += weights[i];
        }
    }

    size_t winner_idx = 0;
    double max_weight = vote_count[0];
    for (size_t i = 1; i < unique_count; i++) {
        if (vote_count[i] > max_weight) {
            max_weight = vote_count[i];
            winner_idx = i;
        }
    }

    if (snprintf(result, result_len, "%s", unique_choices[winner_idx]) < 0) {
        return -1;
    }
    return 0;
}

void vdx_history_init(VDXHistory *history, size_t n) {
    if (!history) {
        return;
    }
    vdx_history_free(history);
    history->entries = (VDXHistoryEntry *)calloc(n, sizeof(VDXHistoryEntry));
    history->len = history->entries ? n : 0;
}

void vdx_history_free(VDXHistory *history) {
    if (!history) {
        return;
    }
    free(history->entries);
    history->entries = NULL;
    history->len = 0;
}

void vdx_weights_init(VDXWeights *weights, size_t n) {
    if (!weights) {
        return;
    }
    vdx_weights_free(weights);
    weights->values = (double *)malloc(n * sizeof(double));
    if (!weights->values) {
        weights->len = 0;
        return;
    }
    weights->len = n;
    for (size_t i = 0; i < n; i++) {
        weights->values[i] = 1.0;
    }
}

void vdx_weights_free(VDXWeights *weights) {
    if (!weights) {
        return;
    }
    free(weights->values);
    weights->values = NULL;
    weights->len = 0;
}

static int check_history_exists(const VDXHistory *history) {
    if (!history || !history->entries) {
        return 0;
    }
    for (size_t i = 0; i < history->len; i++) {
        if (history->entries[i].successes > 0.0 || history->entries[i].rounds > 0.0) {
            return 1;
        }
    }
    return 0;
}

static void ensure_history(VDXHistory *history, size_t n) {
    if (!history) {
        return;
    }
    if (!history->entries || history->len != n) {
        vdx_history_init(history, n);
    }
}

static void ensure_weights(VDXWeights *weights, size_t n) {
    if (!weights) {
        return;
    }
    if (!weights->values || weights->len != n) {
        vdx_weights_init(weights, n);
    }
}

static void update_history_standard(const double *input,
                                    size_t n,
                                    double error_margin,
                                    const VDXHistory *history,
                                    VDXHistory *out_history) {
    vdx_history_init(out_history, n);
    if (!out_history->entries) {
        return;
    }
    for (size_t i = 0; i < n; i++) {
        double s = 0.0;
        for (size_t y = 0; y < n; y++) {
            if (i == y) {
                continue;
            }
            if (input[i] >= (1 - error_margin) * input[y] &&
                input[i] <= (1 + error_margin) * input[y]) {
                s += 1.0;
            }
        }
        double successes = history->entries[i].successes;
        double rounds = history->entries[i].rounds;
        if (s > (double)(n - 1) / 2.0) {
            successes += 1.0;
        }
        rounds += 1.0;
        out_history->entries[i].successes = successes;
        out_history->entries[i].rounds = rounds;
    }
}

static void update_history_alpha(const char **input,
                                 size_t n,
                                 const VDXHistory *history,
                                 VDXHistory *out_history) {
    vdx_history_init(out_history, n);
    if (!out_history->entries) {
        return;
    }
    for (size_t i = 0; i < n; i++) {
        double s = 0.0;
        for (size_t y = 0; y < n; y++) {
            if (i == y) {
                continue;
            }
            if (strcmp(input[i], input[y]) == 0) {
                s += 1.0;
            }
        }
        double successes = history->entries[i].successes;
        double rounds = history->entries[i].rounds;
        if (s > 0.0) {
            successes += 1.0;
        }
        rounds += 1.0;
        out_history->entries[i].successes = successes;
        out_history->entries[i].rounds = rounds;
    }
}

static void update_history_hybrid(const double *input,
                                  size_t n,
                                  double error_margin,
                                  double scaling_factor,
                                  VDXHistory *history,
                                  VDXWeights *weights,
                                  double *winning_value) {
    *winning_value = weighted_average(input, weights->values, n);

    VDXHistory new_history = {0};
    vdx_history_init(&new_history, n);
    if (!new_history.entries) {
        return;
    }

    for (size_t i = 0; i < n; i++) {
        double s = 0.0;
        for (size_t y = 0; y < n; y++) {
            if (i == y) {
                continue;
            }
            if (input[i] >= (1 - error_margin) * input[y] &&
                input[i] <= (1 + error_margin) * input[y]) {
                s += 1.0;
            } else if (input[i] >= (1 - error_margin * scaling_factor) * input[y] &&
                       input[i] <= (1 + error_margin * scaling_factor) * input[y]) {
                double k = fabs(input[i] - input[y]);
                s += (scaling_factor / (scaling_factor - 1)) *
                     (1 - (k / (error_margin * scaling_factor * input[y])));
            }
        }
        double s_total = s / (double)(n - 1);
        double k = fabs(input[i] - *winning_value);
        double e = error_margin * (*winning_value);
        double successes = history->entries[i].successes;
        double rounds = history->entries[i].rounds;
        if (k <= e) {
            successes += 1.0;
        } else if (k > e && k <= e * scaling_factor) {
            successes += (scaling_factor / (scaling_factor - 1)) *
                         (1 - (k / (e * scaling_factor)));
        }
        new_history.entries[i].successes = successes;
        new_history.entries[i].rounds = rounds + 1.0;

        if (successes > history->entries[i].successes) {
            weights->values[i] = s_total;
        } else {
            weights->values[i] = 0.0;
        }
    }

    vdx_history_free(history);
    *history = new_history;
}

static void calculate_weights_standard(const VDXHistory *history, VDXWeights *weights) {
    for (size_t i = 0; i < history->len; i++) {
        double successes = history->entries[i].successes;
        double rounds = history->entries[i].rounds;
        if (rounds == 0.0) {
            weights->values[i] = 0.0;
        } else {
            double rate = successes / rounds;
            weights->values[i] = rate * rate;
        }
    }
}

static void calculate_weights_elimination(const VDXHistory *history, VDXWeights *weights) {
    double total_success_rate = 0.0;
    for (size_t i = 0; i < history->len; i++) {
        double rounds = history->entries[i].rounds;
        double success_rate = rounds == 0.0 ? 0.0 : history->entries[i].successes / rounds;
        total_success_rate += success_rate;
    }
    double average_success_rate = total_success_rate / (double)history->len;
    for (size_t i = 0; i < history->len; i++) {
        double rounds = history->entries[i].rounds;
        double success_rate = rounds == 0.0 ? 0.0 : history->entries[i].successes / rounds;
        if (success_rate < average_success_rate) {
            weights->values[i] = 0.0;
        } else {
            weights->values[i] = success_rate * success_rate;
        }
    }
}

typedef struct {
    double *values;
    size_t len;
    size_t cap;
} Group;

static void group_push(Group *g, double value) {
    if (g->len == g->cap) {
        size_t new_cap = g->cap == 0 ? 4 : g->cap * 2;
        double *new_vals = (double *)realloc(g->values, new_cap * sizeof(double));
        if (!new_vals) {
            return;
        }
        g->values = new_vals;
        g->cap = new_cap;
    }
    g->values[g->len++] = value;
}

static void group_free(Group *g) {
    free(g->values);
    g->values = NULL;
    g->len = 0;
    g->cap = 0;
}

static double clustering_bootstrap(const double *input, size_t n, double error_margin) {
    if (n == 0) {
        return 0.0;
    }

    Group *groups = NULL;
    size_t group_count = 0;
    size_t group_cap = 0;

    for (size_t i = 0; i < n; i++) {
        int assigned = 0;
        for (size_t g = 0; g < group_count; g++) {
            double centroid = average_array(groups[g].values, groups[g].len);
            if (input[i] >= (1 - error_margin) * centroid &&
                input[i] <= (1 + error_margin) * centroid) {
                group_push(&groups[g], input[i]);
                assigned = 1;
                break;
            }
        }
        if (!assigned) {
            if (group_count == group_cap) {
                size_t new_cap = group_cap == 0 ? 4 : group_cap * 2;
                Group *new_groups = (Group *)realloc(groups, new_cap * sizeof(Group));
                if (!new_groups) {
                    break;
                }
                for (size_t k = group_cap; k < new_cap; k++) {
                    new_groups[k].values = NULL;
                    new_groups[k].len = 0;
                    new_groups[k].cap = 0;
                }
                groups = new_groups;
                group_cap = new_cap;
            }
            group_push(&groups[group_count], input[i]);
            group_count++;
        }
    }

    size_t largest_idx = 0;
    for (size_t g = 1; g < group_count; g++) {
        if (groups[g].len > groups[largest_idx].len) {
            largest_idx = g;
        }
    }
    double result = average_array(groups[largest_idx].values, groups[largest_idx].len);

    for (size_t g = 0; g < group_count; g++) {
        group_free(&groups[g]);
    }
    free(groups);
    return result;
}

static double no_history_voting(const double *input, size_t n, double error_margin, int bootstrap) {
    if (bootstrap) {
        return clustering_bootstrap(input, n, error_margin);
    }
    return average_array(input, n);
}

static void history_based_weighted_average(VDXHistory *history,
                                           VDXWeights *weights,
                                           const double *input,
                                           size_t n,
                                           double error_margin,
                                           int bootstrap,
                                           double *result) {
    if (!check_history_exists(history)) {
        vdx_weights_init(weights, n);
        *result = no_history_voting(input, n, error_margin, bootstrap);
        VDXHistory new_history = {0};
        update_history_standard(input, n, error_margin, history, &new_history);
        vdx_history_free(history);
        *history = new_history;
        return;
    }
    calculate_weights_standard(history, weights);
    *result = weighted_average(input, weights->values, n);
    VDXHistory new_history = {0};
    update_history_standard(input, n, error_margin, history, &new_history);
    vdx_history_free(history);
    *history = new_history;
}

static void history_based_weighted_average_elimination(VDXHistory *history,
                                                       VDXWeights *weights,
                                                       const double *input,
                                                       size_t n,
                                                       double error_margin,
                                                       int bootstrap,
                                                       double *result) {
    if (!check_history_exists(history)) {
        vdx_weights_init(weights, n);
        *result = no_history_voting(input, n, error_margin, bootstrap);
        VDXHistory new_history = {0};
        update_history_standard(input, n, error_margin, history, &new_history);
        vdx_history_free(history);
        *history = new_history;
        return;
    }
    calculate_weights_elimination(history, weights);
    *result = weighted_average(input, weights->values, n);
    VDXHistory new_history = {0};
    update_history_standard(input, n, error_margin, history, &new_history);
    vdx_history_free(history);
    *history = new_history;
}

static void history_based_hybrid_voting(VDXHistory *history,
                                        VDXWeights *weights,
                                        const double *input,
                                        size_t n,
                                        double error_margin,
                                        double scaling_factor,
                                        int bootstrap,
                                        double *result) {
    if (!check_history_exists(history)) {
        *result = no_history_voting(input, n, error_margin, bootstrap);
        VDXHistory new_history = {0};
        update_history_standard(input, n, error_margin, history, &new_history);
        vdx_history_free(history);
        *history = new_history;
        calculate_weights_standard(history, weights);
        return;
    }
    update_history_hybrid(input, n, error_margin, scaling_factor, history, weights, result);
}

static int no_history_voting_alpha(VDXHistory *history,
                                   VDXWeights *weights,
                                   const char **input,
                                   size_t n,
                                   char *out,
                                   size_t out_len) {
    vdx_history_init(history, n);
    vdx_weights_init(weights, n);
    return weighted_majority_voting(input, weights->values, n, out, out_len);
}

static int history_based_weighted_majority_voting(VDXHistory *history,
                                                  VDXWeights *weights,
                                                  const char **input,
                                                  size_t n,
                                                  char *out,
                                                  size_t out_len) {
    if (!check_history_exists(history)) {
        return no_history_voting_alpha(history, weights, input, n, out, out_len);
    }
    calculate_weights_standard(history, weights);
    int rc = weighted_majority_voting(input, weights->values, n, out, out_len);
    VDXHistory new_history = {0};
    update_history_alpha(input, n, history, &new_history);
    vdx_history_free(history);
    *history = new_history;
    return rc;
}

static int history_based_weighted_majority_voting_elimination(VDXHistory *history,
                                                              VDXWeights *weights,
                                                              const char **input,
                                                              size_t n,
                                                              char *out,
                                                              size_t out_len) {
    if (!check_history_exists(history)) {
        return no_history_voting_alpha(history, weights, input, n, out, out_len);
    }
    calculate_weights_elimination(history, weights);
    int rc = weighted_majority_voting(input, weights->values, n, out, out_len);
    VDXHistory new_history = {0};
    update_history_alpha(input, n, history, &new_history);
    vdx_history_free(history);
    *history = new_history;
    return rc;
}

double vdx_vote_numeric(VDXHistory *history,
                        VDXWeights *weights,
                        const double *input,
                        size_t n,
                        double error_margin,
                        double scaling_factor,
                        const char *collation,
                        const char *history_algorithm,
                        int bootstrap) {
    double result = 0.0;
    ensure_history(history, n);
    ensure_weights(weights, n);

    if (history_algorithm && strcmp(history_algorithm, "no_history") == 0) {
        result = no_history_voting(input, n, error_margin, bootstrap);
    } else if (history_algorithm &&
               strcmp(history_algorithm, "history_based_weighted_average") == 0) {
        history_based_weighted_average(history, weights, input, n, error_margin, bootstrap, &result);
    } else if (history_algorithm &&
               strcmp(history_algorithm, "history_based_weighted_average_elimination") == 0) {
        history_based_weighted_average_elimination(history, weights, input, n, error_margin, bootstrap, &result);
    } else if (history_algorithm &&
               strcmp(history_algorithm, "history_based_hybrid_voting") == 0) {
        history_based_hybrid_voting(history, weights, input, n, error_margin, scaling_factor, bootstrap, &result);
    } else {
        result = no_history_voting(input, n, error_margin, bootstrap);
    }

    if (collation && strcmp(collation, "nearest_neighbor") == 0) {
        return nearest_neighbor(input, n, result);
    }
    return result;
}

int vdx_vote_alpha(VDXHistory *history,
                   VDXWeights *weights,
                   const char **input,
                   size_t n,
                   const char *history_algorithm,
                   char *out,
                   size_t out_len) {
    ensure_history(history, n);
    ensure_weights(weights, n);
    if (history_algorithm && strcmp(history_algorithm, "no_history") == 0) {
        return no_history_voting_alpha(history, weights, input, n, out, out_len);
    }
    if (history_algorithm &&
        strcmp(history_algorithm, "history_based_weighted_majority_voting") == 0) {
        return history_based_weighted_majority_voting(history, weights, input, n, out, out_len);
    }
    if (history_algorithm &&
        strcmp(history_algorithm, "history_based_weighted_majority_voting_elimination") == 0) {
        return history_based_weighted_majority_voting_elimination(history, weights, input, n, out, out_len);
    }
    return no_history_voting_alpha(history, weights, input, n, out, out_len);
}
