package vdx

import "sync"

type Job struct {
	Index int
	Data  []float64
}

type Result struct {
	Index  int
	Output float64
}

func ProcessJobs(jobs <-chan Job, results chan<- Result, numWorkers int, voteFunc func([]float64) float64) {
	var wg sync.WaitGroup
	wg.Add(numWorkers)
	for range numWorkers {
		go func() {
			defer wg.Done()
			for job := range jobs {
				output := voteFunc(job.Data)
				results <- Result{Index: job.Index, Output: output}
			}
		}()
	}
	go func() {
		wg.Wait()
		close(results)
	}()
}
