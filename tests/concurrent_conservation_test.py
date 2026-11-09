
import os
import select
import socket
import statistics
import subprocess
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from threading import Barrier


def read_response(stream):
    lines = []

    while True:
        line = stream.readline()

        if not line:
            raise AssertionError('Server disconnected before END')

        value = line.decode('utf-8').rstrip('\r\n')

        if value == 'END':
            return lines

        lines.append(value)


def run_client(client_id, port, count, barrier):
    matched_sells = []

    with socket.create_connection(
        ('127.0.0.1', port), timeout=10
    ) as connection:
        connection.settimeout(30)

        with connection.makefile('rb') as stream:
            barrier.wait(timeout=30)

            sells = ''.join(
                f'SELL s{client_id}_{i} 100 1\n'
                for i in range(count)
            )

            connection.sendall(sells.encode())

            for i in range(count):
                order_id = f's{client_id}_{i}'

                response = read_response(stream)

                assert response == [
                    f'ACCEPTED {order_id}, trades=0'
                ], response

            barrier.wait(timeout=30)
            started = time.perf_counter()

            buys = ''.join(
                f'BUY b{client_id}_{i} 100 1\n'
                for i in range(count)
            )

            connection.sendall(buys.encode())

            for i in range(count):
                order_id = f'b{client_id}_{i}'

                response = read_response(stream)

                assert len(response) == 2, response
                assert response[0] == (
                    f'ACCEPTED {order_id}, trades=1'
                ), response

                parts = response[1].split()

                assert len(parts) == 5, parts
                assert parts[0] == 'TRADE', parts
                assert parts[1] == f'buy={order_id}', parts
                assert parts[2].startswith('sell=s'), parts
                assert parts[3] == 'price=100', parts
                assert parts[4] == 'qty=1', parts

                matched_sells.append(parts[2].split('=', 1)[1])

            elapsed = time.perf_counter() - started

    return matched_sells, elapsed


def run_case(port, clients=8, orders_per_client=100):
    barrier = Barrier(clients)

    with ThreadPoolExecutor(max_workers=clients) as executor:
        futures = [
            executor.submit(
                run_client,
                client_id,
                port,
                orders_per_client,
                barrier
            )
            for client_id in range(clients)
        ]

        outputs = [future.result() for future in futures]

    matched_sells = [
        sell_id
        for matched, _ in outputs
        for sell_id in matched
    ]

    expected_sells = {
        f's{client_id}_{i}'
        for client_id in range(clients)
        for i in range(orders_per_client)
    }

    expected_count = clients * orders_per_client

    assert len(matched_sells) == expected_count
    assert len(set(matched_sells)) == expected_count
    assert set(matched_sells) == expected_sells

    with socket.create_connection(
        ('127.0.0.1', port), timeout=10
    ) as connection:
        connection.settimeout(10)

        with connection.makefile('rb') as stream:
            connection.sendall(b'BOOK\n')

            assert read_response(stream) == [
                'BIDS',
                'ASKS'
            ]

    times = [elapsed for _, elapsed in outputs]

    print(f'PASS clients={clients}')
    print(f'PASS sell orders={expected_count}')
    print(f'PASS buy orders={expected_count}')
    print(f'PASS unique trades={expected_count}')
    print('PASS final order book empty')
    print(f'Median client completion={statistics.median(times):.6f}s')
    print(f'Min client completion={min(times):.6f}s')
    print(f'Max client completion={max(times):.6f}s')


def main():
    server = os.environ.get(
        'LATENCY_SERVER_BIN',
        str(
            Path(__file__).resolve().parents[1]
            / 'build'
            / 'latency_server'
        )
    )

    process = subprocess.Popen(
        [server, '0'],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )

    try:
        readable, _, _ = select.select(
            [process.stdout], [], [], 10
        )

        assert readable, 'Server startup timeout'

        line = process.stdout.readline().strip()

        assert line.startswith(
            'LISTENING 127.0.0.1:'
        ), line

        port = int(line.rsplit(':', 1)[1])

        run_case(port)

    finally:
        process.terminate()

        try:
            process.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.communicate()


if __name__ == '__main__':
    main()
