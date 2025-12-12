# Vote function
def vote_numeric(input, history, weights, algorithm="no_history", collation="average", error=0.05, scaling=2, bootstrapping=False):
    average = 0
    if algorithm == "no_history":
        average = no_history(input, error, bootstrapping)
    elif algorithm == "history_based_weighted_average":
        average, history, weights = history_based_weighted_average(input, history, error, bootstrapping)
    elif algorithm == "module_elimination":
        average, history, weights = module_elimination(input, history, error, bootstrapping)
    elif algorithm == "hybrid":
        average, history, weights = hybrid(input, history, weights, error, scaling, bootstrapping)
    if collation == "average":
        return average, history, weights
    elif collation == "nearest_neighbor":
        return nearest_neighbor(average, input), history, weights

def vote_alpha(input, history, algorithm="no_history"):
    if algorithm == "no_history":
        return no_history_alpha(input)
    elif algorithm == "history_based_majority_voting":
        return history_based_majority_voting(input, history)
    elif algorithm == "module_elimination_majority_voting":
        return module_elimination_majority_voting(input, history)

# Voting algorithms

def history_based_weighted_average(input, history, error=0.05, bootstrapping=False):
    if not check_history(history):
        weights = init_weights(input)
        output = no_history(input, error, bootstrapping=bootstrapping)
        history = calculate_history_standard(input, history, error)
    else:
        weights = calculate_weights_standard(input, history)
        output = weighted_average(input, weights)
        history = calculate_history_standard(input, history, error)
    return output, history, weights

def module_elimination(input, history, error=0.05, bootstrapping=False):
    if not check_history(history):
        weights = init_weights(input)
        output = no_history(input, error, bootstrapping=bootstrapping)
        history = calculate_history_standard(input, history, error)
    else:
        weights = calculate_weights_module_elimination(input, history)
        output = weighted_average(input, weights)
        history = calculate_history_standard(input, history, error)
    return output, history, weights

def hybrid(input, history, weights, error=0.05, scaling=2, bootstrapping=False):
    average = 0
    if not check_history(history):
        weights = init_weights(input)
        average = no_history(input, error, bootstrapping=bootstrapping)
        history = calculate_history_standard(input, history, error)
    else:
        average, history, weights = calculate_history_hybrid(input, history, weights, error, scaling)
        pass
    return average, history, weights

def no_history(input, error=0.05, bootstrapping=False):
    if bootstrapping:
        return majority_voting_bootstrapping(input, error)
    else:
        return average(input)

def no_history_alpha(input, history):
    weights = init_weights(input)
    return weighted_majority_voting(input, weights), history

def history_based_majority_voting(input, history):
    if not check_history(history):
        weights = init_weights(input)
        average = no_history_alpha(input)
        history = calculate_history_standard_alpha(input, history)
    else:
        weights = calculate_weights_standard(input, history)
        average = weighted_majority_voting(input, weights)
        history = calculate_history_standard_alpha(input, history)
    return average, history

def module_elimination_majority_voting(input, history):
    if not check_history(history):
        weights = init_weights(input)
        average = no_history_alpha(input)
        history = calculate_history_standard_alpha(input, history)
    else:
        weights = calculate_weights_module_elimination(input, history)
        average = weighted_majority_voting(input, weights)
        history = calculate_history_standard_alpha(input, history)
    return average, history

# Majority voting bootstrapping

def majority_voting_bootstrapping(input, error):
    groups = []
    for value in input:
        if groups == []:
            groups.append([value])
        else:
            for i in range(len(groups)):
                if value >= (1 - error) * (sum(groups[i])/len(groups[i])) and value <= (1 + error) * (sum(groups[i])/len(groups[i])):
                    groups[i].append(value)
                    break
            else:
                groups.append([value])
    max_length = 0
    max_index = 0
    for i in range(len(groups)):
        if len(groups[i]) > max_length:
            max_length = len(groups[i])
            max_index = i
    return sum(groups[max_index])/len(groups[max_index])

# Collation

def average(input):
    # return the mean of input
    return sum(input)/len(input)

def nearest_neighbor(value, input):
    # return the nearest neighbor of the mean of input
    return min(input, key=lambda x: abs(x-value))

def weighted_average(input, weights):
    # if weights are all zero, return the average
    if sum(weights) == 0:
        return average(input)
    # return the weighted average of input
    return sum(x*y for x, y in zip(input, weights))/sum(weights)

def weighted_majority_voting(input, weights):
    groups = []
    for i in range(len(input)):
        if groups == []:
            groups.append([input[i], weights[i]])
        else:
            for group in groups:
                if input[i] == group[0]:
                    group[1] += weights[i]
                    break
            groups.append([input[i], weights[i]])
    # return the first value of the group with the highest size
    for group in groups:
        if group[1] > sum(weights)/2:
            return group[0]

# History calculation functions

def check_history(history):
    for item in history:
        if item[0] != 0 or item[1] != 0:
            return True
    else:
        return False

def calculate_history_standard(input, history, error=0.05):
    successes = []
    rounds = []
    for item in history:
        successes.append(item[0])
        rounds.append(item[1])
    new_history = []
    for i in range(len(input)):
        s = 0
        for y in range(len(input)):
            if i != y and input[i] >= (1 - error)*input[y] and input[i] <= (1+error)*input[y]:
                s += 1
        if s >= (len(input)-1)/2:
            successes[i] += 1
        rounds[i] += 1
        new_history.append([successes[i], rounds[i]])
    return new_history

def calculate_history_standard_alpha(input, history):
    successes = []
    rounds = []
    for item in history:
        successes.append(item[0])
        rounds.append(item[1])
    new_history = []
    for i in range(len(input)):
        s = 0
        for y in range(len(input)):
            if i != y and input[i] == input[y]:
                s += 1
        if s >= (len(input)-1)/2:
            successes[i] += 1
        rounds[i] += 1
        new_history.append([successes[i], rounds[i]])
    return new_history

def calculate_history_hybrid(input, history, weights, error=0.05, scaling=2):
    output = weighted_average(input, weights)
    successes = []
    rounds = []
    for item in history:
        successes.append(item[0])
        rounds.append(item[1])
    new_history = []
    for i in range(len(input)):
        s = 0
        for y in range(len(input)):
            if i != y and abs(input[i]) >= abs((1-error)*input[y]) and abs(input[i]) <= abs((1+error)*input[y]):
                s += 1
            elif i != y and abs(input[i]) >= abs((1-error*scaling)*input[y]) and abs(input[i]) <= abs((1+error*scaling)*input[y]):
                k = abs(input[i] - input[y])
                s += (scaling/(scaling-1)) * (1 - (k / (scaling * error * abs(input[y]))))
        s_total = s / (len(input) - 1)
        k = abs(input[i] - output)
        e = error*output
        if k <= e:
            successes[i] += 1
        elif k > e and k < scaling * e:
            successes[i] += (scaling/(scaling-1)) * (1 - (k / (scaling * e)))
        new_history.append([successes[i], rounds[i] + 1])
        if successes[i] >= sum(successes)/len(successes):
            weights[i] = s_total
        else:
            weights[i] = 0
    return output, new_history, weights

# Weight calculation functions

def init_weights(input):
    weights = []
    for i in range(len(input)):
        weights.append(1)
    return weights

def calculate_weights_standard(input, history):
    successes = []
    rounds = []
    for item in history:
        successes.append(item[0])
        rounds.append(item[1])
    weights = []
    for i in range(len(input)):
        weights.append((successes[i]/rounds[i])**2)
    return weights

def calculate_weights_module_elimination(input, history):
    successes = []
    rounds = []
    for item in history:
        successes.append(item[0])
        rounds.append(item[1])
    weights = []
    for i in range(len(input)):
        if successes[i] >= sum(successes)/len(successes):
            weights.append((successes[i]/rounds[i])**2)
        else:
            weights.append(0)
    return weights
