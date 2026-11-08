
import argparse
import json
import math
import select
import socket
import statistics
import subprocess
import time
from concurrent.futures import ThreadPoolExecutor
from threading import Barrier


def read_response(stream):
    lines = []

    while True:
        line = stream.readline()

        if not line:
            raise RuntimeError('TCP server disconnected before END')

        value = line.decode('utf-8').rstrip('\r\n')

        if value == 'END':
            return lines

        lines.append(value)


def launch_server(server_path):
    process = subprocess.Popen(
        [server_path, '0'],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )

    try:
        readable, _, _ = select.select([process.stdout], [], [], 10)

        if not readable:
            raise RuntimeError('TCP server startup timed out')

        line = process.stdout.readline().strip()

        if not line.startswith('LISTENING 127.0.0.1:'):
            raise RuntimeError(f'Unexpected server response: {line}')

        port = int(line.rsplit(':', 1)[1])
        return process, port
    except Exception:
        process.terminate()
        process.communicate(timeout=5)
        raise


def stop_server(process):
    process.terminate()

    try:
        process.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.communicate()


def measure_latency(port, samples, warmup_pairs):
    latencies = []

    with socket.create_connection(('127.0.0.1', port), timeout=10) as connection:
        connection.settimeout(15)

        with connection.makefile('rb') as stream:
            for index in range(warmup_pairs + samples):
                maker = f'lat_m_{index}'
                taker = f'lat_t_{index}'

                connection.sendall(f'SELL {maker} 100 1\n'.encode())

                if read_response(stream) != [f'ACCEPTED {maker}, trades=0']:
                    raise AssertionError('Unexpected resting order response')

                start = time.perf_counter_ns()
                connection.sendall(f'BUY {taker} 100 1\n'.encode())
                response = read_response(stream)
                elapsed = time.perf_counter_ns() - start

                if response != [
                    f'ACCEPTED {taker}, trades=1',
                    f'TRADE buy={taker} sell={maker} price=100 qty=1'
                ]:
                    raise AssertionError(f'Unexpected matching response: {response}')

                if index >= warmup_pairs:
                    latencies.append(elapsed)

    latencies.sort()

    def percentile(p):
        return latencies[math.ceil(len(latencies) * p / 100) - 1] / 1000

    return {
        'samples': samples,
        'warmup_pairs': warmup_pairs,
        'scope': 'TCP BUY send to complete END response on localhost',
        'p50_us': round(percentile(50), 3),
        'p95_us': round(percentile(95), 3),
        'p99_us': round(percentile(99), 3),
        'mean_us': round(statistics.fmean(latencies) / 1000, 3)
    }


def throughput_client(port, client_id, pairs, batch_pairs, barrier):
    with socket.create_connection(('127.0.0.1', port), timeout=10) as connection:
        connection.settimeout(20)

        with connection.makefile('rb') as stream:
            barrier.wait(timeout=15)

            for offset in range(0, pairs, batch_pairs):
                count = min(batch_pairs, pairs - offset)
                commands = []

                for index in range(offset, offset + count):
                    maker = f'thr_m_{client_id}_{index}'
                    taker = f'thr_t_{client_id}_{index}'
                    commands.append(f'SELL {maker} 100 1\n')
                    commands.append(f'BUY {taker} 100 1\n')

                connection.sendall(''.join(commands).encode())

                for index in range(offset, offset + count):
                    maker = f'thr_m_{client_id}_{index}'
                    taker = f'thr_t_{client_id}_{index}'
                    sell_response = read_response(stream)
                    buy_response = read_response(stream)

                    if sell_response != [f'ACCEPTED {maker}, trades=0']:
                        raise AssertionError(f'Unexpected SELL response: {sell_response}')

                    if (
                        len(buy_response) != 2
                        or buy_response[0] != f'ACCEPTED {taker}, trades=1'
                        or not buy_response[1].startswith(f'TRADE buy={taker} sell=thr_m_')
                        or not buy_response[1].endswith(' price=100 qty=1')
                    ):
                        raise AssertionError(f'Unexpected BUY response: {buy_response}')

    return pairs * 2


def measure_throughput(port, pairs, clients, batch_pairs):
    counts = [pairs // clients + (index < pairs % clients) for index in range(clients)]
    barrier = Barrier(clients + 1)

    with ThreadPoolExecutor(max_workers=clients) as executor:
        futures = [
            executor.submit(throughput_client, port, index, count, batch_pairs, barrier)
            for index, count in enumerate(counts)
        ]

        start = time.perf_counter()
        barrier.wait(timeout=15)
        completed = sum(future.result() for future in futures)
        seconds = time.perf_counter() - start

    if completed != pairs * 2:
        raise AssertionError('Wrong number of processed orders')

    with socket.create_connection(('127.0.0.1', port), timeout=10) as connection:
        connection.settimeout(10)

        with connection.makefile('rb') as stream:
            connection.sendall(b'BOOK\n')

            if read_response(stream) != ['BIDS', 'ASKS']:
                raise AssertionError('Order book was not empty after throughput benchmark')

    return {
        'clients': clients,
        'pairs': pairs,
        'commands': completed,
        'batch_pairs': batch_pairs,
        'scope': 'Full localhost TCP requests and responses with batching',
        'elapsed_seconds': round(seconds, 6),
        'orders_per_second': round(completed / seconds, 2)
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--server', default='./build/latency_server')
    parser.add_argument('--latency-samples', type=int, default=3000)
    parser.add_argument('--warmup-pairs', type=int, default=200)
    parser.add_argument('--throughput-pairs', type=int, default=5000)
    parser.add_argument('--clients', type=int, default=4)
    parser.add_argument('--batch-pairs', type=int, default=20)
    args = parser.parse_args()

    if (
        args.latency_samples <= 0
        or args.warmup_pairs < 0
        or args.throughput_pairs <= 0
        or args.clients <= 0
        or args.clients > args.throughput_pairs
        or args.clients > 32
        or args.batch_pairs <= 0
    ):
        parser.error('Invalid sample, pair, client, or batch configuration')

    process, port = launch_server(args.server)

    try:
        latency = measure_latency(port, args.latency_samples, args.warmup_pairs)
        throughput = measure_throughput(
            port, args.throughput_pairs, args.clients, args.batch_pairs
        )
    finally:
        stop_server(process)

    result = {
        'benchmark': 'localhost_tcp',
        'latency': latency,
        'throughput': throughput
    }

    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
