
import json
import subprocess
import sys
from pathlib import Path


def main():
    script = (
        Path(__file__).resolve().parents[1]
        / 'benchmarks'
        / 'tcp_benchmark.py'
    )

    server = sys.argv[1]

    process = subprocess.run(
        [
            sys.executable, str(script), '--server', server,
            '--latency-samples', '40',
            '--warmup-pairs', '10',
            '--throughput-pairs', '200',
            '--clients', '4',
            '--batch-pairs', '10'
        ],
        capture_output=True,
        text=True,
        check=True,
        timeout=30
    )

    report = json.loads(process.stdout)
    latency = report['latency']
    throughput = report['throughput']

    assert 0 <= latency['p50_us'] <= latency['p95_us'] <= latency['p99_us']
    assert throughput['commands'] == 400
    assert throughput['orders_per_second'] > 0

    print('PASS TCP latency, concurrent throughput, and response correctness')


if __name__ == '__main__':
    main()
