package vdx

import (
	"encoding/csv"
	"fmt"
	"os"
	"strconv"
)

func ReadCSVRows(path string) (<-chan Job, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	reader := csv.NewReader(f)
	jobs := make(chan Job)
	go func() {
		defer close(jobs)
		defer f.Close()
		idx := 0
		for {
			record, err := reader.Read()
			if err != nil {
				break
			}
			var data []float64
			for _, s := range record {
				v, _ := strconv.ParseFloat(s, 64)
				data = append(data, v)
			}
			jobs <- Job{Index: idx, Data: data}
			idx++
		}
	}()
	return jobs, nil
}

func WriteResults(path string, results <-chan Result) error {
	f, err := os.Create(path)
	if err != nil {
		return err
	}
	defer f.Close()
	writer := csv.NewWriter(f)
	defer writer.Flush()
	resultMap := make(map[int]float64)
	nextIndex := 0
	for res := range results {
		resultMap[res.Index] = res.Output
		for {
			if val, ok := resultMap[nextIndex]; ok {
				writer.Write([]string{fmt.Sprintf("%f", val)})
				delete(resultMap, nextIndex)
				nextIndex++
			} else {
				break
			}
		}
	}
	return nil
}
