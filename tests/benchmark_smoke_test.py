import os
import subprocess
from pathlib import Path


def main():
    default_binary = Path(__file__).resolve().parents[1] / 'build' / 'latency_bench'
    binary = os.environ.get('LATENCY_BENCH_BIN', str(default_binary))

    process = subprocess.run(
        [binary, '1000'],
        capture_output=True,
        text=True,
        check=True,
        timeout=15
    )

    metrics = dict(
        line.split('=', 1)
        for line in process.stdout.splitlines()
        if '=' in line
    )

    assert int(metrics['samples']) == 1000
    assert int(metrics['matched_quantity']) == 1000
    assert int(metrics['throughput_matched_quantity']) == 1000
    assert 0 <= int(metrics['p50_ns']) <= int(metrics['p95_ns'])
    assert int(metrics['p95_ns']) <= int(metrics['p99_ns'])
    assert float(metrics['throughput_orders_per_sec']) > 0

    print('PASS benchmark output, matching correctness, and latency ordering')


if __name__ == '__main__':
    main()
