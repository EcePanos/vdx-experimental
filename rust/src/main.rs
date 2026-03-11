use std::env;
use std::fs::File;
use std::io::{BufWriter, Write};
use std::thread;

use crossbeam_channel as channel;

use vdx::{new_history, new_weights, vote_numeric};

struct Job {
    index: usize,
    data: Vec<f64>,
}

struct ResultItem {
    index: usize,
    output: f64,
}

fn parse_args() -> Result<(usize, String, String), String> {
    let mut num_workers = std::thread::available_parallelism()
        .map(|n| n.get())
        .unwrap_or(1);
    let mut input = None;
    let mut output = None;
    let mut args = env::args().skip(1);
    while let Some(arg) = args.next() {
        if arg == "-j" {
            let Some(val) = args.next() else {
                return Err("Missing value for -j".to_string());
            };
            num_workers = val.parse::<usize>().unwrap_or(1);
        } else if input.is_none() {
            input = Some(arg);
        } else if output.is_none() {
            output = Some(arg);
        }
    }
    let input = input.ok_or_else(|| "Usage: vdxcli [-j N] <input.csv> [output.csv]".to_string())?;
    let data_dir = env::var("VDX_DATA_DIR").unwrap_or_else(|_| "/data".to_string());
    let input = if std::path::Path::new(&input).is_absolute() {
        input
    } else {
        format!("{}/{}", data_dir.trim_end_matches('/'), input)
    };
    let output = output.unwrap_or_else(|| format!("{}/output.csv", data_dir.trim_end_matches('/')));
    if num_workers == 0 {
        num_workers = 1;
    }
    Ok((num_workers, input, output))
}

fn main() {
    let (num_workers, input_path, output_path) = match parse_args() {
        Ok(v) => v,
        Err(msg) => {
            eprintln!("{}", msg);
            std::process::exit(1);
        }
    };

    let input_file = match File::open(&input_path) {
        Ok(f) => f,
        Err(err) => {
            eprintln!("Failed to open input: {}", err);
            std::process::exit(1);
        }
    };
    let output_file = match File::create(&output_path) {
        Ok(f) => f,
        Err(err) => {
            eprintln!("Failed to open output: {}", err);
            std::process::exit(1);
        }
    };

    let queue_cap = std::cmp::max(64, num_workers * 8);
    let (jobs_tx, jobs_rx) = channel::bounded::<Job>(queue_cap);
    let (results_tx, results_rx) = channel::bounded::<ResultItem>(queue_cap);

    let writer_handle = thread::spawn(move || {
        let mut writer = BufWriter::new(output_file);
        let window_cap = queue_cap + num_workers + 1;
        let mut window: Vec<Option<f64>> = vec![None; window_cap];
        let mut base_index = 0usize;
        let mut next_index = 0usize;
        let mut overflow: std::collections::BTreeMap<usize, f64> = std::collections::BTreeMap::new();

        let mut try_flush = |writer: &mut BufWriter<File>,
                             window: &mut [Option<f64>],
                             overflow: &mut std::collections::BTreeMap<usize, f64>,
                             base_index: &mut usize,
                             next_index: &mut usize| {
            loop {
                if *next_index < *base_index + window.len() {
                    let slot = *next_index - *base_index;
                    if let Some(val) = window[slot].take() {
                        let _ = writeln!(writer, "{:.6}", val);
                        *next_index += 1;
                        if *next_index - *base_index >= window.len() {
                            *base_index = *next_index;
                        }
                        continue;
                    }
                }
                if let Some(val) = overflow.remove(next_index) {
                    let _ = writeln!(writer, "{:.6}", val);
                    *next_index += 1;
                    continue;
                }
                break;
            }
        };

        for item in results_rx.iter() {
            if item.index < base_index {
                continue;
            }
            if item.index < base_index + window.len() {
                let slot = item.index - base_index;
                window[slot] = Some(item.output);
            } else {
                overflow.insert(item.index, item.output);
            }
            try_flush(&mut writer, &mut window, &mut overflow, &mut base_index, &mut next_index);
        }

        try_flush(&mut writer, &mut window, &mut overflow, &mut base_index, &mut next_index);
        let _ = writer.flush();
    });

    let mut workers = Vec::with_capacity(num_workers);
    for _ in 0..num_workers {
        let rx = jobs_rx.clone();
        let tx = results_tx.clone();
        workers.push(thread::spawn(move || {
            for job in rx.iter() {
                let history = new_history(job.data.len());
                let weights = new_weights(job.data.len());
                let (result, _, _) = vote_numeric(
                    history,
                    weights,
                    &job.data,
                    0.05,
                    2.0,
                    "nearest_neighbor",
                    "history_based_hybrid_voting",
                    true,
                );
                let _ = tx.send(ResultItem {
                    index: job.index,
                    output: result,
                });
            }
        }));
    }
    drop(results_tx);
    drop(jobs_rx);

    let mut rdr = csv::ReaderBuilder::new()
        .has_headers(false)
        .from_reader(input_file);

    let mut idx = 0usize;
    for record in rdr.records() {
        let record = match record {
            Ok(r) => r,
            Err(_) => continue,
        };
        if record.is_empty() {
            continue;
        }
        let mut data = Vec::with_capacity(record.len());
        for field in record.iter() {
            let val = field.trim().parse::<f64>().unwrap_or(0.0);
            data.push(val);
        }
        if jobs_tx.send(Job { index: idx, data }).is_err() {
            break;
        }
        idx += 1;
    }

    drop(jobs_tx);

    for worker in workers {
        let _ = worker.join();
    }
    let _ = writer_handle.join();
}
