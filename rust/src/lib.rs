use std::collections::HashMap;

pub type History = Vec<[f64; 2]>;
pub type Weights = Vec<f64>;

pub fn new_history(num_modules: usize) -> History {
    vec![[0.0, 0.0]; num_modules]
}

pub fn new_weights(num_modules: usize) -> Weights {
    vec![1.0; num_modules]
}

fn sum(nums: &[f64]) -> f64 {
    nums.iter().copied().sum()
}

fn average(nums: &[f64]) -> f64 {
    if nums.is_empty() {
        return 0.0;
    }
    sum(nums) / nums.len() as f64
}

fn nearest_neighbor(nums: &[f64], target: f64) -> f64 {
    if nums.is_empty() {
        return 0.0;
    }
    let mut nearest = nums[0];
    let mut smallest_diff = (nearest - target).abs();
    for &num in &nums[1..] {
        let diff = (num - target).abs();
        if diff < smallest_diff {
            smallest_diff = diff;
            nearest = num;
        }
    }
    nearest
}

fn weighted_average(nums: &[f64], weights: &Weights) -> f64 {
    if nums.is_empty() || nums.len() != weights.len() {
        return 0.0;
    }
    let mut weighted_sum = 0.0;
    let mut total_weight = 0.0;
    for (num, weight) in nums.iter().copied().zip(weights.iter().copied()) {
        weighted_sum += num * weight;
        total_weight += weight;
    }
    if total_weight == 0.0 {
        return 0.0;
    }
    weighted_sum / total_weight
}

fn weighted_majority_voting(choices: &[String], weights: &Weights) -> String {
    if choices.is_empty() || choices.len() != weights.len() {
        return String::new();
    }
    let mut vote_count: HashMap<&str, f64> = HashMap::new();
    for (choice, weight) in choices.iter().zip(weights.iter()) {
        *vote_count.entry(choice.as_str()).or_insert(0.0) += *weight;
    }
    let mut winner = "";
    let mut max_weight = f64::MIN;
    for (choice, weight) in vote_count {
        if weight > max_weight {
            max_weight = weight;
            winner = choice;
        }
    }
    winner.to_string()
}

fn check_history_exists(history: &History) -> bool {
    history
        .iter()
        .any(|entry| entry[0] > 0.0 || entry[1] > 0.0)
}

fn update_history_standard(history: &History, input_data: &[f64], error_margin: f64) -> History {
    let mut new_history = new_history(history.len());
    for i in 0..input_data.len() {
        let mut s = 0.0;
        for y in 0..input_data.len() {
            if i == y {
                continue;
            }
            if input_data[i] >= (1.0 - error_margin) * input_data[y]
                && input_data[i] <= (1.0 + error_margin) * input_data[y]
            {
                s += 1.0;
            }
        }
        let mut successes = history[i][0];
        let rounds = history[i][1];
        if s > (input_data.len() as f64 - 1.0) / 2.0 {
            successes += 1.0;
        }
        new_history[i][0] = successes;
        new_history[i][1] = rounds + 1.0;
    }
    new_history
}

fn update_history_alpha(history: &History, input_data: &[String]) -> History {
    let mut new_history = new_history(history.len());
    for i in 0..input_data.len() {
        let mut s = 0.0;
        for y in 0..input_data.len() {
            if i == y {
                continue;
            }
            if input_data[i] == input_data[y] {
                s += 1.0;
            }
        }
        let mut successes = history[i][0];
        let rounds = history[i][1];
        if s > 0.0 {
            successes += 1.0;
        }
        new_history[i][0] = successes;
        new_history[i][1] = rounds + 1.0;
    }
    new_history
}

fn update_history_hybrid(
    history: &History,
    mut weights: Weights,
    input_data: &[f64],
    error_margin: f64,
    scaling_factor: f64,
) -> (f64, History, Weights) {
    let winning_value = weighted_average(input_data, &weights);
    let mut new_history = new_history(history.len());
    for i in 0..input_data.len() {
        let mut s = 0.0;
        for y in 0..input_data.len() {
            if i == y {
                continue;
            }
            if input_data[i] >= (1.0 - error_margin) * input_data[y]
                && input_data[i] <= (1.0 + error_margin) * input_data[y]
            {
                s += 1.0;
            } else if input_data[i] >= (1.0 - error_margin * scaling_factor) * input_data[y]
                && input_data[i] <= (1.0 + error_margin * scaling_factor) * input_data[y]
            {
                let k = (input_data[i] - input_data[y]).abs();
                s += (scaling_factor / (scaling_factor - 1.0))
                    * (1.0 - (k / (error_margin * scaling_factor * input_data[y])));
            }
        }
        let s_total = s / (input_data.len() as f64 - 1.0);
        let k = (input_data[i] - winning_value).abs();
        let e = error_margin * winning_value;
        let mut successes = history[i][0];
        if k <= e {
            successes += 1.0;
        } else if k > e && k <= e * scaling_factor {
            successes += (scaling_factor / (scaling_factor - 1.0))
                * (1.0 - (k / (e * scaling_factor)));
        }
        new_history[i][0] = successes;
        new_history[i][1] = history[i][1] + 1.0;
        if successes > history[i][0] {
            weights[i] = s_total;
        } else {
            weights[i] = 0.0;
        }
    }
    (winning_value, new_history, weights)
}

fn calculate_weights_standard(history: &History) -> Weights {
    let mut weights = vec![0.0; history.len()];
    for (i, entry) in history.iter().enumerate() {
        let successes = entry[0];
        let rounds = entry[1];
        if rounds != 0.0 {
            let rate = successes / rounds;
            weights[i] = rate * rate;
        }
    }
    weights
}

fn calculate_weights_elimination(history: &History) -> Weights {
    let mut total_success_rate = 0.0;
    for entry in history {
        let rounds = entry[1];
        let success_rate = if rounds == 0.0 { 0.0 } else { entry[0] / rounds };
        total_success_rate += success_rate;
    }
    let average_success_rate = total_success_rate / history.len() as f64;
    let mut weights = vec![0.0; history.len()];
    for (i, entry) in history.iter().enumerate() {
        let rounds = entry[1];
        let success_rate = if rounds == 0.0 { 0.0 } else { entry[0] / rounds };
        if success_rate < average_success_rate {
            weights[i] = 0.0;
        } else {
            weights[i] = success_rate * success_rate;
        }
    }
    weights
}

fn clustering_bootstrap(input_data: &[f64], error_margin: f64) -> f64 {
    if input_data.is_empty() {
        return 0.0;
    }
    let mut groups: Vec<Vec<f64>> = Vec::new();
    for &data_point in input_data {
        let mut assigned = false;
        for group in groups.iter_mut() {
            let centroid = average(group);
            if data_point >= (1.0 - error_margin) * centroid
                && data_point <= (1.0 + error_margin) * centroid
            {
                group.push(data_point);
                assigned = true;
                break;
            }
        }
        if !assigned {
            groups.push(vec![data_point]);
        }
    }
    let mut largest_group = &groups[0];
    for group in &groups[1..] {
        if group.len() > largest_group.len() {
            largest_group = group;
        }
    }
    average(largest_group)
}

fn no_history_voting(input_data: &[f64], error_margin: f64, use_clustering: bool) -> f64 {
    if use_clustering {
        return clustering_bootstrap(input_data, error_margin);
    }
    average(input_data)
}

fn history_based_weighted_average(
    history: History,
    input_data: &[f64],
    error_margin: f64,
    bootstrap: bool,
) -> (f64, History, Weights) {
    if !check_history_exists(&history) {
        let weights = new_weights(history.len());
        let result = no_history_voting(input_data, error_margin, bootstrap);
        let new_history = update_history_standard(&history, input_data, error_margin);
        return (result, new_history, weights);
    }
    let weights = calculate_weights_standard(&history);
    let result = weighted_average(input_data, &weights);
    let new_history = update_history_standard(&history, input_data, error_margin);
    (result, new_history, weights)
}

fn history_based_weighted_average_elimination(
    history: History,
    input_data: &[f64],
    error_margin: f64,
    bootstrap: bool,
) -> (f64, History, Weights) {
    if !check_history_exists(&history) {
        let weights = new_weights(history.len());
        let result = no_history_voting(input_data, error_margin, bootstrap);
        let new_history = update_history_standard(&history, input_data, error_margin);
        return (result, new_history, weights);
    }
    let weights = calculate_weights_elimination(&history);
    let result = weighted_average(input_data, &weights);
    let new_history = update_history_standard(&history, input_data, error_margin);
    (result, new_history, weights)
}

fn history_based_hybrid_voting(
    history: History,
    weights: Weights,
    input_data: &[f64],
    error_margin: f64,
    scaling_factor: f64,
    bootstrap: bool,
) -> (f64, History, Weights) {
    if !check_history_exists(&history) {
        let result = no_history_voting(input_data, error_margin, bootstrap);
        let new_history = update_history_standard(&history, input_data, error_margin);
        let new_weights = calculate_weights_standard(&new_history);
        return (result, new_history, new_weights);
    }
    update_history_hybrid(&history, weights, input_data, error_margin, scaling_factor)
}

fn no_history_voting_alpha(input_data: &[String], history: History) -> (String, History) {
    let new_history = new_history(history.len());
    let result = weighted_majority_voting(input_data, &new_weights(history.len()));
    (result, new_history)
}

fn history_based_weighted_majority_voting(
    history: History,
    input_data: &[String],
) -> (String, History, Weights) {
    if !check_history_exists(&history) {
        let weights = new_weights(history.len());
        let (result, new_history) = no_history_voting_alpha(input_data, history);
        return (result, new_history, weights);
    }
    let weights = calculate_weights_standard(&history);
    let result = weighted_majority_voting(input_data, &weights);
    let new_history = update_history_alpha(&history, input_data);
    (result, new_history, weights)
}

fn history_based_weighted_majority_voting_elimination(
    history: History,
    input_data: &[String],
) -> (String, History, Weights) {
    if !check_history_exists(&history) {
        let weights = new_weights(history.len());
        let (result, new_history) = no_history_voting_alpha(input_data, history);
        return (result, new_history, weights);
    }
    let weights = calculate_weights_elimination(&history);
    let result = weighted_majority_voting(input_data, &weights);
    let new_history = update_history_alpha(&history, input_data);
    (result, new_history, weights)
}

pub fn vote_numeric(
    history: History,
    weights: Weights,
    input_data: &[f64],
    error_margin: f64,
    scaling_factor: f64,
    collation: &str,
    history_algorithm: &str,
    bootstrap: bool,
) -> (f64, History, Weights) {
    let (result, new_history, new_weights) = match history_algorithm {
        "no_history" => (no_history_voting(input_data, error_margin, bootstrap), history, weights),
        "history_based_weighted_average" => {
            history_based_weighted_average(history, input_data, error_margin, bootstrap)
        }
        "history_based_weighted_average_elimination" => history_based_weighted_average_elimination(
            history,
            input_data,
            error_margin,
            bootstrap,
        ),
        "history_based_hybrid_voting" => history_based_hybrid_voting(
            history,
            weights,
            input_data,
            error_margin,
            scaling_factor,
            bootstrap,
        ),
        _ => (no_history_voting(input_data, error_margin, bootstrap), history, weights),
    };

    match collation {
        "average" => (result, new_history, new_weights),
        "nearest_neighbor" => (nearest_neighbor(input_data, result), new_history, new_weights),
        _ => (result, new_history, new_weights),
    }
}

pub fn vote_alpha(
    history: History,
    input_data: &[String],
    history_algorithm: &str,
) -> (String, History, Weights) {
    match history_algorithm {
        "no_history" => {
            let (result, new_history) = no_history_voting_alpha(input_data, history);
            (result, new_history, Vec::new())
        }
        "history_based_weighted_majority_voting" => {
            history_based_weighted_majority_voting(history, input_data)
        }
        "history_based_weighted_majority_voting_elimination" => {
            history_based_weighted_majority_voting_elimination(history, input_data)
        }
        _ => {
            let (result, new_history) = no_history_voting_alpha(input_data, history);
            (result, new_history, Vec::new())
        }
    }
}
