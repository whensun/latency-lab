import os
import select
import socket
import subprocess
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path


def read_response(stream):
    output = []

    while True:
        line = stream.readline()

        if not line:
            raise AssertionError('Server disconnected before END')

        value = line.decode('utf-8').rstrip('\r\n')

        if value == 'END':
            return output

        output.append(value)


def submit_order(port, index):
    with socket.create_connection(('127.0.0.1', port), timeout=5) as connection:
        connection.settimeout(5)

        with connection.makefile('rb') as stream:
            connection.sendall(f'BUY order{index} 50 1\n'.encode())
            assert read_response(stream) == [
                f'ACCEPTED order{index}, trades=0'
            ]
            connection.sendall(b'QUIT\n')
            assert read_response(stream) == ['BYE']


def run_cases(port):
    with socket.create_connection(('127.0.0.1', port), timeout=5) as first:
        first.settimeout(5)

        with first.makefile('rb') as first_stream:
            first.sendall(b'SELL resting 100 5\n')
            assert read_response(first_stream) == [
                'ACCEPTED resting, trades=0'
            ]

            with socket.create_connection(('127.0.0.1', port), timeout=5) as second:
                second.settimeout(5)

                with second.makefile('rb') as second_stream:
                    second.sendall(b'BUY crossing 101 2\n')
                    assert read_response(second_stream) == [
                        'ACCEPTED crossing, trades=1',
                        'TRADE buy=crossing sell=resting price=100 qty=2'
                    ]

            first.sendall(b'BOOK\n')
            assert read_response(first_stream) == [
                'BIDS',
                'ASKS',
                '  price=100 qty=3 orders=1'
            ]

            first.sendall(b'QUIT\n')
            assert read_response(first_stream) == ['BYE']

    with ThreadPoolExecutor(max_workers=12) as executor:
        list(executor.map(lambda index: submit_order(port, index), range(48)))

    with socket.create_connection(('127.0.0.1', port), timeout=5) as connection:
        connection.settimeout(5)

        with connection.makefile('rb') as stream:
            connection.sendall(b'BOOK\n')
            assert read_response(stream) == [
                'BIDS',
                '  price=50 qty=48 orders=48',
                'ASKS',
                '  price=100 qty=3 orders=1'
            ]


def main():
    server = os.environ.get('LATENCY_SERVER_BIN')

    if not server:
        server = str(
            Path(__file__).resolve().parents[1]
            / 'build' / 'latency_server'
        )

    process = subprocess.Popen(
        [server, '0'],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )

    try:
        readable, _, _ = select.select(
            [process.stdout], [], [], 5
        )
        assert readable, 'Server startup timed out'

        line = process.stdout.readline().strip()
        assert line.startswith('LISTENING 127.0.0.1:'), line

        port = int(line.rsplit(':', 1)[1])
        run_cases(port)

        print('PASS concurrent clients, shared order book, 48 parallel orders')
    finally:
        process.terminate()

        try:
            process.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.communicate()


if __name__ == '__main__':
    main()
