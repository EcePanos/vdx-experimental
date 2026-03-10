#!/usr/bin/env python3
import csv
import os
import subprocess
import sys
import time


IMAGES = ["vdx_c", "vdx_go", "vdx_python"]


def run(cmd, check=True):
    return subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, check=check)


def docker_ok():
    try:
        run(["docker", "info"], check=True)
        return True
    except Exception:
        return False


def image_size_bytes(image):
    r = run(["docker", "image", "inspect", image, "--format", "{{.Size}}"], check=True)
    return int(r.stdout.strip())


def bytes_to_mb(b):
    return round(b / 1024 / 1024, 2)


def parse_mem_to_bytes(s):
    s = s.strip()
    if not s:
        return 0
    if s.endswith("KiB"):
        return int(float(s[:-3]) * 1024)
    if s.endswith("MiB"):
        return int(float(s[:-3]) * 1024 * 1024)
    if s.endswith("GiB"):
        return int(float(s[:-3]) * 1024 * 1024 * 1024)
    if s.endswith("B"):
        return int(float(s[:-1]))
    return 0


def docker_run(image, input_dir, input_base):
    name = f"bench_{image}_{int(time.time() * 1000)}"
    cmd = [
        "docker",
        "run",
        "-d",
        "--rm",
        "--name",
        name,
        "-v",
        f"{input_dir}:/data",
        image,
        f"/data/{input_base}",
        "/data/output.csv",
    ]
    r = run(cmd, check=True)
    return name


def docker_running(name):
    r = run(["docker", "ps", "-q", "-f", f"name=^{name}$"], check=False)
    return bool(r.stdout.strip())


def docker_wait(name):
    run(["docker", "wait", name], check=False)


def docker_pid(name):
    r = run(["docker", "inspect", "--format", "{{.State.Pid}}", name], check=False)
    try:
        return int(r.stdout.strip())
    except Exception:
        return 0


def docker_id(name):
    r = run(["docker", "inspect", "--format", "{{.Id}}", name], check=False)
    cid = r.stdout.strip()
    return cid if cid else ""


def cgroup_path_from_pid(pid):
    try:
        with open(f"/proc/{pid}/cgroup", "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                # cgroup v2: 0::/path
                if line.startswith("0::"):
                    return line.split("0::", 1)[1]
                # cgroup v1: 1:name:/path
                parts = line.split(":", 2)
                if len(parts) == 3:
                    return parts[2]
    except Exception:
        return ""
    return ""


def cgroup_peak_bytes(container_id, pid):
    candidates = []
    if pid > 0:
        cg_path = cgroup_path_from_pid(pid)
        if cg_path:
            candidates.append(os.path.join("/sys/fs/cgroup", cg_path.lstrip("/")))
    if container_id:
        cid = container_id
        candidates.extend(
            [
                f"/sys/fs/cgroup/system.slice/docker-{cid}.scope",
                f"/sys/fs/cgroup/system.slice/docker-{cid[:12]}.scope",
                f"/sys/fs/cgroup/docker/{cid}",
                f"/sys/fs/cgroup/docker/{cid[:12]}",
                f"/sys/fs/cgroup/docker-{cid}.scope",
                f"/sys/fs/cgroup/docker-{cid[:12]}.scope",
                f"/sys/fs/cgroup/{cid}",
                f"/sys/fs/cgroup/{cid[:12]}",
            ]
        )
    for base in candidates:
        peak_path = os.path.join(base, "memory.peak")
        max_path = os.path.join(base, "memory.max_usage_in_bytes")
        try:
            if os.path.isfile(peak_path):
                with open(peak_path, "r", encoding="utf-8") as f:
                    return int(f.read().strip() or "0")
            if os.path.isfile(max_path):
                with open(max_path, "r", encoding="utf-8") as f:
                    return int(f.read().strip() or "0")
        except Exception:
            continue
    return 0


def proc_rss_bytes(pid):
    try:
        with open(f"/proc/{pid}/status", "r", encoding="utf-8") as f:
            for line in f:
                if line.startswith("VmRSS:"):
                    parts = line.split()
                    if len(parts) >= 2:
                        return int(parts[1]) * 1024
    except Exception:
        return 0
    return 0


def docker_stats_mem(name):
    r = run(["docker", "stats", "--no-stream", "--format", "{{.MemUsage}}", name], check=False)
    if not r.stdout.strip():
        return 0
    mem = r.stdout.split("/", 1)[0].strip()
    return parse_mem_to_bytes(mem)


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <input.csv> [runs] [min_runtime_sec]", file=sys.stderr)
        return 1
    input_path = sys.argv[1]
    runs = int(sys.argv[2]) if len(sys.argv) >= 3 else 3
    min_runtime = float(sys.argv[3]) if len(sys.argv) >= 4 else 0.0

    if not os.path.isfile(input_path):
        print(f"input file not found: {input_path}", file=sys.stderr)
        return 1
    if not docker_ok():
        print("docker daemon not available", file=sys.stderr)
        return 1

    input_dir = os.path.abspath(os.path.dirname(input_path))
    input_base = os.path.basename(input_path)

    writer = csv.writer(sys.stdout)
    writer.writerow(["image", "run", "seconds", "peak_mem_mb", "image_size_mb"])

    for image in IMAGES:
        try:
            size_mb = bytes_to_mb(image_size_bytes(image))
        except Exception:
            print(f"missing image: {image} (build or tag it first)", file=sys.stderr)
            return 1
        for run_idx in range(1, runs + 1):
            total_duration = 0.0
            peak = 0
            iterations = 0
            while total_duration < max(min_runtime, 0.0) or iterations == 0:
                iterations += 1
                try:
                    name = docker_run(image, input_dir, input_base)
                except Exception as exc:
                    print(f"failed to start container for image {image}: {exc}", file=sys.stderr)
                    return 1
                start = time.time()
                cid = docker_id(name)
                while docker_running(name):
                    pid = docker_pid(name)
                    cg_peak = cgroup_peak_bytes(cid, pid)
                    if cg_peak > peak:
                        peak = cg_peak
                    mem = proc_rss_bytes(pid) if pid > 0 else 0
                    if mem == 0:
                        mem = docker_stats_mem(name)
                    if mem > peak:
                        peak = mem
                    time.sleep(0.05)
                docker_wait(name)
                pid = docker_pid(name)
                cg_peak = cgroup_peak_bytes(cid, pid)
                if cg_peak > peak:
                    peak = cg_peak
                total_duration += time.time() - start
            writer.writerow(
                [image, run_idx, f"{total_duration:.3f}", f"{bytes_to_mb(peak):.2f}", f"{size_mb:.2f}"]
            )
            sys.stdout.flush()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
