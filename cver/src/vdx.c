#include <stdio.h>
#indlude <strings.h>
#include "vdx.h"

#define MAX_CHOICES 100
#define MAX_CHOICE_LEN 64

// history struct
// history is a 2D array. Each row contains the history of one module
// each column contains the number of rounds the module has participated in
// and the number of wins (wins can be decimal)
typedef struct {
    int rounds;
    float wins;
} History;

// sum function for an array of floats
float sum(float* arr, int size) {
    float total = 0.0f;
    for (int i = 0; i < size; i++) {
        total += arr[i];
    }
    return total;
}

// average function for an array of floats
float average(float* arr, int size) {
    if (size == 0) return 0.0f;
    return sum(arr, size) / size;
}

// nearest neighbor function
// returns the closest value in arr to the target
float nearest_neighbor(float* arr, int size, float target) {
    if (size == 0) return 0.0f; // or some error value
    float nearest = arr[0];
    float min_diff = fabsf(target - nearest);
    for (int i = 1; i < size; i++) {
        float diff = fabsf(target - arr[i]);
        if (diff < min_diff) {
            min_diff = diff;
            nearest = arr[i];
        }
    }
    return nearest;
}

// weighted average function
float weighted_average(float* arr, float* weights, int size) {
    float weighted_sum = 0.0f;
    float total_weight = 0.0f;
    for (int i = 0; i < size; i++) {
        weighted_sum += arr[i] * weights[i];
        total_weight += weights[i];
    }
    if (total_weight == 0.0f) return 0.0f;
    return weighted_sum / total_weight;
}

// weighted majority function for an array of strings
void weighted_majority_voting(const char choices[][MAX_CHOICE_LEN], const double *weights, int n, char *result) {
    if (n == 0) {
        result[0] = '\0';
        return;
    }

    double voteCount[MAX_CHOICES] = {0};
    char uniqueChoices[MAX_CHOICES][MAX_CHOICE_LEN];
    int uniqueCount = 0;

    // Build unique choices and accumulate weights
    for (int i = 0; i < n; ++i) {
        int found = -1;
        for (int j = 0; j < uniqueCount; ++j) {
            if (strcmp(choices[i], uniqueChoices[j]) == 0) {
                found = j;
                break;
            }
        }
        if (found == -1) {
            strcpy(uniqueChoices[uniqueCount], choices[i]);
            voteCount[uniqueCount] = weights[i];
            uniqueCount++;
        } else {
            voteCount[found] += weights[i];
        }
    }

    // Find the winner
    int winnerIdx = 0;
    double maxWeight = voteCount[0];
    for (int i = 1; i < uniqueCount; ++i) {
        if (voteCount[i] > maxWeight) {
            maxWeight = voteCount[i];
            winnerIdx = i;
        }
    }
    strcpy(result, uniqueChoices[winnerIdx]);
}

// function to create an emty history for n modules
History* create_empty_history(int n) {
    History* history = (History*)malloc(n * sizeof(History));
    for (int i = 0; i < n; i++) {
        history[i].rounds = 0;
        history[i].wins = 0.0f;
    }
    return history;
}

// function to check if all values in a history array are zero
int is_history_empty(History* history, int n) {
    for (int i = 0; i < n; i++) {
        if (history[i].rounds != 0 || history[i].wins != 0.0f) {
            return 0; // not empty
        }
    }
    return 1; // empty
}
