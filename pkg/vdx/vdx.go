package vdx

import (
	"math"
)

// Type Definitions

type History [][]float64
type Weights []float64

// Utility Functions

func sum(nums []float64) float64 {
	var total float64
	for _, num := range nums {
		total += num
	}
	return total
}

// Collation Functions

func average(nums ...float64) float64 {
	if len(nums) == 0 {
		return 0
	}
	var sum float64
	for _, num := range nums {
		sum += num
	}
	return sum / float64(len(nums))
}

func nearest_neighbor(nums []float64, target float64) float64 {
	if len(nums) == 0 {
		return 0
	}
	nearest := nums[0]
	smallestDiff := math.Abs(nearest - target)
	for _, num := range nums[1:] {
		diff := math.Abs(num - target)
		if diff < smallestDiff {
			smallestDiff = diff
			nearest = num
		}
	}
	return nearest
}

func weighted_average(nums []float64, weights Weights) float64 {
	if len(nums) == 0 || len(nums) != len(weights) {
		return 0
	}
	var weightedSum, totalWeight float64
	for i, num := range nums {
		weightedSum += num * weights[i]
		totalWeight += weights[i]
	}
	if totalWeight == 0 {
		return 0
	}
	return weightedSum / totalWeight
}

func weighted_majority_voting(choices []string, weights Weights) string {
	if len(choices) == 0 || len(choices) != len(weights) {
		return ""
	}
	voteCount := make(map[string]float64)
	for i, choice := range choices {
		voteCount[choice] += weights[i]
	}
	var winner string
	var maxWeight float64
	for choice, weight := range voteCount {
		if weight > maxWeight {
			maxWeight = weight
			winner = choice
		}
	}
	return winner
}

// History calculation functions

// history is a 2D slice where each sub-slice represents a module
// for each module we keep track of how many times it was included in the winning result
// and how many times it was considered (included in the candidate set)

func NewHistory(num_modules int) History {
	history := make(History, num_modules)
	for i := range history {
		history[i] = []float64{0, 0} // [successes, rounds]
	}
	return history
}

// check whether all numbers in the slice are zero, that means we are in the bootstrapping phase
func check_history_exists(history History) bool {
	for _, moduleHistory := range history {
		if moduleHistory[0] > 0 || moduleHistory[1] > 0 {
			return true
		}
	}
	return false
}

// Standard history update function
// for each module, we calculate if it was included in the winning result
// if yes, we increment the first element of its history by 1
// in any case, we increment the second element of its history by 1
func update_history_standard(history History, input_data []float64, error_margin float64) History {
	successes := make([]float64, len(history))
	rounds := make([]float64, len(history))
	// initialize the successes and rounds slices
	for i := range history {
		successes[i] = history[i][0]
		rounds[i] = history[i][1]
	}
	// initialize the new history
	new_history := NewHistory(len(history))
	for i := range input_data {
		s := float64(0)
		for y := range input_data {
			if i != y && input_data[i] >= (1-error_margin)*input_data[y] && input_data[i] <= (1+error_margin)*input_data[y] {
				s++
			}
		}
		if s > float64(len(input_data)-1)/2 {
			successes[i]++
		}
		rounds[i]++
		new_history[i] = []float64{successes[i], rounds[i]}
	}
	return new_history
}

// Standard history update for alpha modules
// Since we cannot compute error margins for alpha modules, we consider a comparison successful
// if the module's output matches any other module's output exactly
func update_history_alpha(history History, input_data []string) History {
	successes := make([]float64, len(history))
	rounds := make([]float64, len(history))
	// initialize the successes and rounds slices
	for i := range history {
		successes[i] = history[i][0]
		rounds[i] = history[i][1]
	}
	// initialize the new history
	new_history := NewHistory(len(history))
	for i := range input_data {
		s := float64(0)
		for y := range input_data {
			if i != y && input_data[i] == input_data[y] {
				s++
			}
		}
		if s > float64(0) {
			successes[i]++
		}
		rounds[i]++
		new_history[i] = []float64{successes[i], rounds[i]}
	}
	return new_history
}

// History update function for the hybrid method
// here we must calculate the winning value first
// and then we can calculate a partial success, if a module is not successful
// but its output is within error * scaling factor of the winning value
// in that case the s value is not an integer, but a value between 0 and 1
func update_history_hybrid(history History, weights Weights, input_data []float64, error_margin float64, scaling_factor float64) (float64, History, Weights) {
	winning_value := weighted_average(input_data, weights)
	successes := make([]float64, len(history))
	rounds := make([]float64, len(history))
	// initialize the successes and rounds slices
	for i := range history {
		successes[i] = float64(history[i][0])
		rounds[i] = history[i][1]
	}
	// initialize the new history
	new_history := NewHistory(len(history))
	for i := range input_data {
		s := float64(0)
		for y := range input_data {
			if i != y && input_data[i] >= (1-error_margin)*input_data[y] && input_data[i] <= (1+error_margin)*input_data[y] {
				s++
			} else if i != y && input_data[i] >= (1-error_margin*scaling_factor)*input_data[y] && input_data[i] <= (1+error_margin*scaling_factor)*input_data[y] {
				k := math.Abs(input_data[i] - input_data[y])
				s += (scaling_factor / (scaling_factor - 1)) * (1 - (k / (error_margin * scaling_factor * input_data[y])))
			}
		}
		s_total := float64(s / float64(len(input_data)-1))
		k := math.Abs(input_data[i] - winning_value)
		e := error_margin * winning_value
		if k <= e {
			successes[i]++
		} else if k > e && k <= e*scaling_factor {
			successes[i] += (scaling_factor / (scaling_factor - 1)) * (1 - (k / (e * scaling_factor)))
		}
		new_history[i] = []float64{float64(successes[i]), rounds[i] + 1}
		if successes[i] > sum(successes)/float64(len(successes)) {
			weights[i] = s_total
		} else {
			weights[i] = 0
		}
	}
	return winning_value, new_history, weights
}

// Weight calculation functions

// Initialize weights as 1
func NewWeights(num_modules int) Weights {
	weights := make(Weights, num_modules)
	for i := range weights {
		weights[i] = 1.0
	}
	return weights
}

// Standard weight calculation function
// the weight of each module is the square of its success rate
func calculate_weights_standard(history History) Weights {
	successes := make([]float64, len(history))
	rounds := make([]float64, len(history))
	weights := make(Weights, len(history))
	for i := range history {
		successes[i] = history[i][0]
		rounds[i] = history[i][1]
		weights[i] = (successes[i] / rounds[i]) * (successes[i] / rounds[i])
	}
	return weights
}

// Weight calculation with module elimination
// if a module's success rate is below the average success rate across all modules, its weight is set to zero
func calculate_weights_elimination(history History) Weights {
	successes := make([]float64, len(history))
	rounds := make([]float64, len(history))
	weights := make(Weights, len(history))
	var totalSuccessRate float64
	for i := range history {
		successes[i] = history[i][0]
		rounds[i] = history[i][1]
		successRate := successes[i] / rounds[i]
		totalSuccessRate += successRate
	}
	averageSuccessRate := totalSuccessRate / float64(len(history))
	for i := range history {
		successRate := successes[i] / rounds[i]
		if successRate < averageSuccessRate {
			weights[i] = 0
		} else {
			weights[i] = successRate * successRate
		}
	}
	return weights
}

// Clustering-based bootstrapping
// We use a grouping method to cluster modules.
// A module is assigned to a group if its output is within the error margin of the group's centroid.
// The centroid of the largest group is selected as the winning value.
func clustering_bootstrap(input_data []float64, error_margin float64) float64 {
	groups := make([][]float64, 0)
	for _, dataPoint := range input_data {
		assigned := false
		for i, group := range groups {
			centroid := average(group...)
			if dataPoint >= (1-error_margin)*centroid && dataPoint <= (1+error_margin)*centroid {
				groups[i] = append(groups[i], dataPoint)
				assigned = true
				break
			}
		}
		if !assigned {
			groups = append(groups, []float64{dataPoint})
		}
	}
	largestGroup := groups[0]
	for _, group := range groups {
		if len(group) > len(largestGroup) {
			largestGroup = group
		}
	}
	return average(largestGroup...)
}

// Voting algorithms
// These algorithms combine the steps above into a single function for ease of use

// No history voting
// This is the fallback method when no history exists
// It can optionally use clustering-based bootstrapping
func no_history_voting(input_data []float64, error_margin float64, use_clustering bool) float64 {
	if use_clustering {
		return clustering_bootstrap(input_data, error_margin)
	}
	return average(input_data...)
}

// History-based weighted average
func history_based_weighted_average(history History, input_data []float64, error_margin float64, bootstrap bool) (float64, History, Weights) {
	// check if the history exists
	if !check_history_exists(history) {
		// initialize weights
		weights := NewWeights(len(history))
		// since the weights are still equal, fall back to the no history voting
		result := no_history_voting(input_data, error_margin, bootstrap)
		new_history := update_history_standard(history, input_data, error_margin)
		return result, new_history, weights
	}
	// calculate weights based on history
	weights := calculate_weights_standard(history)
	// calculate the weighted average
	result := weighted_average(input_data, weights)
	// update history
	new_history := update_history_standard(history, input_data, error_margin)
	return result, new_history, weights
}

// History-based weighted average with module elimination
func history_based_weighted_average_elimination(history History, input_data []float64, error_margin float64, bootstrap bool) (float64, History, Weights) {
	// check if the history exists
	if !check_history_exists(history) {
		// initialize weights
		weights := NewWeights(len(history))
		// since the weights are still equal, fall back to the no history voting
		result := no_history_voting(input_data, error_margin, bootstrap)
		new_history := update_history_standard(history, input_data, error_margin)
		return result, new_history, weights
	}
	// calculate weights based on history with elimination
	weights := calculate_weights_elimination(history)
	// calculate the weighted average
	result := weighted_average(input_data, weights)
	// update history
	new_history := update_history_standard(history, input_data, error_margin)
	return result, new_history, weights
}

// History-based hybrid voting
func history_based_hybrid_voting(history History, weights Weights, input_data []float64, error_margin float64, scaling_factor float64, bootstrap bool) (float64, History, Weights) {
	// check if the history exists
	if !check_history_exists(history) {
		// since the weights are still equal, fall back to the no history voting
		result := no_history_voting(input_data, error_margin, bootstrap)
		new_history := update_history_standard(history, input_data, error_margin)
		new_weights := calculate_weights_standard(new_history)
		return result, new_history, new_weights
	}
	// calculate the weighted average with history-based hybrid method
	result, new_history, new_weights := update_history_hybrid(history, weights, input_data, error_margin, scaling_factor)
	return result, new_history, new_weights
}

// Alpha module voting

// No history voting for alpha modules
// this also initializes the weights and history for alpha modules
func no_history_voting_alpha(input_data []string, history History) (string, History) {
	// initialize history
	new_history := NewHistory(len(history))
	// perform majority voting
	result := weighted_majority_voting(input_data, NewWeights(len(history)))
	return result, new_history
}

// History-based weighted majority voting for alpha modules
func history_based_weighted_majority_voting(history History, input_data []string) (string, History, Weights) {
	// check if the history exists
	if !check_history_exists(history) {
		// initialize weights
		weights := NewWeights(len(history))
		// since the weights are still equal, fall back to the no history voting
		result, new_history := no_history_voting_alpha(input_data, history)
		return result, new_history, weights
	}
	// calculate weights based on history
	weights := calculate_weights_standard(history)
	// perform weighted majority voting
	result := weighted_majority_voting(input_data, weights)
	// update history
	new_history := update_history_alpha(history, input_data)
	return result, new_history, weights
}

// History-based weighted majority voting with module elimination for alpha modules
func history_based_weighted_majority_voting_elimination(history History, input_data []string) (string, History, Weights) {
	// check if the history exists
	if !check_history_exists(history) {
		// initialize weights
		weights := NewWeights(len(history))
		// since the weights are still equal, fall back to the no history voting
		result, new_history := no_history_voting_alpha(input_data, history)
		return result, new_history, weights
	}
	// calculate weights based on history with elimination
	weights := calculate_weights_elimination(history)
	// perform weighted majority voting
	result := weighted_majority_voting(input_data, weights)
	// update history
	new_history := update_history_alpha(history, input_data)
	return result, new_history, weights
}

// Public API Functions

// Numeric voting function
func VoteNumeric(history History, weights Weights, input_data []float64, error_margin float64, scaling_factor float64, collation string, history_algorithm string, bootstrap bool) (float64, History, Weights) {
	var result float64
	var new_history History
	var new_weights Weights

	switch history_algorithm {
	case "no_history":
		result = no_history_voting(input_data, error_margin, bootstrap)
	case "history_based_weighted_average":
		result, new_history, new_weights = history_based_weighted_average(history, input_data, error_margin, bootstrap)
	case "history_based_weighted_average_elimination":
		result, new_history, new_weights = history_based_weighted_average_elimination(history, input_data, error_margin, bootstrap)
	case "history_based_hybrid_voting":
		result, new_history, new_weights = history_based_hybrid_voting(history, weights, input_data, error_margin, scaling_factor, bootstrap)
	default:
		result = no_history_voting(input_data, error_margin, bootstrap)
	}

	switch collation {
	case "average":
		return result, new_history, new_weights
	case "nearest_neighbor":
		nearest := nearest_neighbor(input_data, result)
		return nearest, new_history, new_weights
	default:
		return result, new_history, new_weights
	}
}

// Alpha voting function
func VoteAlpha(history History, input_data []string, history_algorithm string) (string, History, Weights) {
	var result string
	var new_history History
	var new_weights Weights

	switch history_algorithm {
	case "no_history":
		result, new_history = no_history_voting_alpha(input_data, history)
	case "history_based_weighted_majority_voting":
		result, new_history, new_weights = history_based_weighted_majority_voting(history, input_data)
	case "history_based_weighted_majority_voting_elimination":
		result, new_history, new_weights = history_based_weighted_majority_voting_elimination(history, input_data)
	default:
		result, new_history = no_history_voting_alpha(input_data, history)
	}

	return result, new_history, new_weights
}
