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
    if len(sys.argv) >= 3:
        output_file = sys.argv[2]
    else:
        output_file = (input_file.rsplit('/', 1)[0] + '/output.csv') if '/' in input_file else 'output.csv'
    with Pool(cpu_count()) as pool, open(output_file, 'w', newline='') as out_f:
        writer = csv.writer(out_f)
        for result in pool.imap(vote_row, row_generator(input_file), chunksize=100):
            writer.writerow([result])
