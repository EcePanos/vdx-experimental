from vdx import vote_numeric
import sys
import csv
from multiprocessing import Pool, cpu_count

def vote_row(row):
    history = [[0 for _ in range(2)] for _ in range(len(row))]
    weights = [1 for _ in range(len(row))]
    return vote_numeric(row, history, weights, "hybrid", "nearest_neighbor", bootstrapping=True)[0]

def row_generator(input_file):
    with open(input_file, 'r') as f:
        reader = csv.reader(f)
        for row in reader:
            yield [float(x) for x in row]

if __name__ == "__main__":
    input_file = sys.argv[1]
    with Pool(cpu_count()) as pool, open('output.csv', 'w', newline='') as out_f:
        writer = csv.writer(out_f)
        for result in pool.imap(vote_row, row_generator(input_file), chunksize=100):
            writer.writerow([result])
