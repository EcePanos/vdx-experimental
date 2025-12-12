package main

import (
	"fmt"
	"os"

	"example.com/vdx/pkg/vdx"
)

func main() {
	if len(os.Args) < 2 {
		fmt.Println("Usage: vdxcli <input.csv>")
		os.Exit(1)
	}
	inputFile := os.Args[1]
	outputFile := "output.csv"
	numWorkers := 12

	jobs, err := vdx.ReadCSVRows(inputFile)
	if err != nil {
		fmt.Printf("Error: %v\n", err)
		os.Exit(1)
	}
	results := make(chan vdx.Result, numWorkers*2)
	go vdx.ProcessJobs(jobs, results, numWorkers, func(data []float64) float64 {
		history := make([][]float64, len(data))
		for i := range history {
			history[i] = make([]float64, 2)
		}
		weights := make([]float64, len(data))
		for i := range weights {
			weights[i] = 1.0
		}
		result, _, _ := vdx.VoteNumeric(history, weights, data, 0.05, 2, "nearest_neighbor", "history_based_hybrid_voting", true)
		return result
	})
	if err := vdx.WriteResults(outputFile, results); err != nil {
		fmt.Printf("Error: %v\n", err)
		os.Exit(1)
	}
}
