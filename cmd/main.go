package main

import (
	"encoding/csv"
	"fmt"
	"os"
	"strconv"
	"sync"

	vdx "example.com/vdx/pkg/vdx"
)

func main() {
	if len(os.Args) < 2 {
		fmt.Println("Usage: program <input.csv>")
		os.Exit(1)
	}
	inputFile := os.Args[1]

	f, err := os.Open(inputFile)
	if err != nil {
		fmt.Printf("Error opening file: %v\n", err)
		os.Exit(1)
	}
	defer f.Close()
	reader := csv.NewReader(f)

	numWorkers := 12

	type Job struct {
		Index int
		Data  []float64
	}

	type Result struct {
		Index  int
		Output float64
	}

	jobsChan := make(chan Job, numWorkers*2)
	resultsChan := make(chan Result, numWorkers*2)

	// Start worker goroutines
	var wg sync.WaitGroup
	wg.Add(numWorkers)
	for w := 0; w < numWorkers; w++ {
		go func() {
			defer wg.Done()
			for job := range jobsChan {
				history := make([][]float64, len(job.Data))
				for i := range history {
					history[i] = make([]float64, 2)
				}
				weights := make([]float64, len(job.Data))
				for i := range weights {
					weights[i] = 1.0
				}
				result, _, _ := vdx.VoteNumeric(history, weights, job.Data, 0.05, 2, "nearest_neighbor", "history_based_hybrid_voting", true)
				resultsChan <- Result{Index: job.Index, Output: result}
			}
		}()
	}

	// Start a goroutine to close resultsChan after all workers are done
	go func() {
		wg.Wait()
		close(resultsChan)
	}()

	// Open output file and writer
	outFile, _ := os.Create("output.csv")
	defer outFile.Close()
	writer := csv.NewWriter(outFile)
	defer writer.Flush()

	// Read and dispatch jobs row by row, and write results as they come in
	var rowIndex int
	var resultWg sync.WaitGroup
	resultWg.Add(1)
	go func() {
		defer resultWg.Done()
		resultsMap := make(map[int]float64)
		nextIndex := 0
		for res := range resultsChan {
			resultsMap[res.Index] = res.Output
			// Write results in order as soon as possible
			for {
				if val, ok := resultsMap[nextIndex]; ok {
					writer.Write([]string{fmt.Sprintf("%f", val)})
					delete(resultsMap, nextIndex)
					nextIndex++
				} else {
					break
				}
			}
		}
	}()

	for {
		record, err := reader.Read()
		if err != nil {
			break
		}
		var data []float64
		for _, strVal := range record {
			val, _ := strconv.ParseFloat(strVal, 64)
			data = append(data, val)
		}
		jobsChan <- Job{Index: rowIndex, Data: data}
		rowIndex++
	}
	close(jobsChan)
	resultWg.Wait()
}
