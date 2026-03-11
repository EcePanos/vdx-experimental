use std::env;
use std::fs::File;
use std::io::{BufWriter, Write};
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Arc;
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
    let (results_tx, results_rx) = channel::unbounded::<ResultItem>();

    let total_jobs = Arc::new(AtomicUsize::new(0));
    let total_jobs_writer = Arc::clone(&total_jobs);

    let writer_handle = thread::spawn(move || {
        let mut writer = BufWriter::new(output_file);
        let mut buffer: Vec<Option<f64>> = Vec::new();
        let mut next_index = 0usize;

        for item in results_rx.iter() {
            if item.index >= buffer.len() {
                buffer.resize(item.index + 1, None);
            }
            buffer[item.index] = Some(item.output);
            while next_index < buffer.len() {
                if let Some(val) = buffer[next_index].take() {
                    let _ = writeln!(writer, "{:.6}", val);
                    next_index += 1;
                } else {
                    break;
                }
            }
        }

        let total = total_jobs_writer.load(Ordering::Acquire);
        while next_index < total {
            if next_index >= buffer.len() {
                buffer.resize(next_index + 1, None);
            }
            if let Some(val) = buffer[next_index].take() {
                let _ = writeln!(writer, "{:.6}", val);
                next_index += 1;
            } else {
                break;
            }
        }
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

    total_jobs.store(idx, Ordering::Release);
    drop(jobs_tx);

    for worker in workers {
        let _ = worker.join();
    }
    let _ = writer_handle.join();
}
