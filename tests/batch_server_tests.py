
import os
import select
import socket
import subprocess
from pathlib import Path


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

def run_cases(port):
    with socket.create_connection(
        ('127.0.0.1', port), timeout=5
    ) as connection:
        connection.settimeout(10)

        with connection.makefile('rb') as stream:
            commands = ''.join(
                f'SELL maker{i} 100 1\n'
                for i in range(100)
            )

            commands += (
                'BUY taker 100 100\n'
                'QUIT\n'
                'SELL ignored 100 1\n'
            )

            connection.sendall(commands.encode('utf-8'))

            for index in range(100):
                assert read_response(stream) == [
                    f'ACCEPTED maker{index}, trades=0'
                ]

            expected = ['ACCEPTED taker, trades=100']

            expected.extend(
                f'TRADE buy=taker sell=maker{i} price=100 qty=1'
                for i in range(100)
            )

            assert read_response(stream) == expected
            assert read_response(stream) == ['BYE']

    with socket.create_connection(
        ('127.0.0.1', port), timeout=5
    ) as connection:
        connection.settimeout(5)

        with connection.makefile('rb') as stream:
            connection.sendall(b'BOOK\n')

            assert read_response(stream) == [
                'BIDS',
                'ASKS'
            ]

def main():
    server = os.environ.get('LATENCY_SERVER_BIN')

    if not server:
        server = str(
            Path(__file__).resolve().parents[1]
            / 'build'
            / 'latency_server'
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
        assert line.startswith(
            'LISTENING 127.0.0.1:'
        ), line

        port = int(line.rsplit(':', 1)[1])

        run_cases(port)

        print(
            'PASS 100 pipelined orders, FIFO matching, '
            'and QUIT boundary'
        )

    finally:
        process.terminate()

        try:
            process.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.communicate()


if __name__ == '__main__':
    main()
