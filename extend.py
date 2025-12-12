# Take the input CSV file and extend it N times by duplicating its rows.
import sys
import csv

if __name__ == "__main__":
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    n = int(sys.argv[3])

    rows = []
    with open(input_file, 'r') as f:
        reader = csv.reader(f)
        for row in reader:
            rows.append(row)

    with open(output_file, 'w', newline='') as f:
        writer = csv.writer(f)
        for _ in range(n):
            for row in rows:
                writer.writerow(row)
