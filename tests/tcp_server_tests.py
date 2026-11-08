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


def run_client(port):
    with socket.create_connection(('127.0.0.1', port), timeout=5) as connection:
        connection.settimeout(5)

        with connection.makefile('rb') as stream:
            connection.sendall(b'SELL a 100 5\nBUY b 105 3\n')

            assert read_response(stream) == [
                'ACCEPTED a, trades=0'
            ]

            assert read_response(stream) == [
                'ACCEPTED b, trades=1',
                'TRADE buy=b sell=a price=100 qty=3'
            ]

            connection.sendall(b'BOOK\n')

            assert read_response(stream) == [
                'BIDS',
                'ASKS',
                '  price=100 qty=2 orders=1'
            ]

            connection.sendall(b'SELL incomplete')
            connection.sendall(b' 102 2\n')

            assert read_response(stream) == [
                'ACCEPTED incomplete, trades=0'
            ]

            connection.sendall(b'BUY x 0 3\n')

            assert read_response(stream) == [
                'REJECTED: id must be nonempty; price and quantity must be positive'
            ]

            connection.sendall(b'QUIT\n')

            assert read_response(stream) == ['BYE']

    with socket.create_connection(('127.0.0.1', port), timeout=5) as connection:
        connection.settimeout(5)

        with connection.makefile('rb') as stream:
            connection.sendall(b'BOOK\n')

            assert read_response(stream) == [
                'BIDS',
                'ASKS',
                '  price=100 qty=2 orders=1',
                '  price=102 qty=2 orders=1'
            ]

            connection.sendall(b'CANCEL a\nBOOK\n')

            assert read_response(stream) == ['CANCELLED a']

            assert read_response(stream) == [
                'BIDS',
                'ASKS',
                '  price=102 qty=2 orders=1'
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

        if not readable:
            raise AssertionError('Server startup timed out')

        line = process.stdout.readline().strip()
        assert line.startswith('LISTENING 127.0.0.1:'), line

        port = int(line.rsplit(':', 1)[1])

        run_client(port)

        print(
            'PASS TCP commands, matching, partial reads, '
            'disconnects, state persistence'
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