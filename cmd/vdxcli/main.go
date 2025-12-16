package main

import (
	"flag"
	"fmt"
	"os"
	"runtime"

	"example.com/vdx/pkg/vdx"
)

func main() {
	var numWorkers int
	flag.IntVar(&numWorkers, "j", runtime.NumCPU(), "number of parallel jobs")
	flag.Parse()
	if flag.NArg() < 1 {
		fmt.Println("Usage: vdxcli [-j N] <input.csv>")
		os.Exit(1)
	}
	inputFile := "/data/" + flag.Arg(0)
	outputFile := "/data/output.csv"

	jobs, err := vdx.ReadCSVRows(inputFile)
	if err != nil {
		fmt.Printf("Error: %v\n", err)
		os.Exit(1)
	}
	results := make(chan vdx.Result, numWorkers*2)
	go vdx.ProcessJobs(jobs, results, numWorkers, func(data []float64) float64 {
		history := vdx.NewHistory(len(data))
		weights := vdx.NewWeights(len(data))
		result, _, _ := vdx.VoteNumeric(history, weights, data, 0.05, 2, "nearest_neighbor", "history_based_hybrid_voting", true)
		return result
	})
	if err := vdx.WriteResults(outputFile, results); err != nil {
		fmt.Printf("Error: %v\n", err)
		os.Exit(1)
	}
}
