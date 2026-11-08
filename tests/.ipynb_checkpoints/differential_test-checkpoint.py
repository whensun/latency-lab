import random
import subprocess
import sys
from pathlib import Path

from reference_model import ReferenceBook

def generate_commands(seed: int, steps: int) -> list[str]:
    rng = random.Random(seed)
    book = ReferenceBook()
    commands = []
    next_id = 0

    for index in range(steps):
        choice = rng.random()

        if choice < 0.65:
            side = rng.choice(['BUY', 'SELL'])

            if book.orders and rng.random() < 0.12:
                order_id = rng.choice(book.orders).order_id
            else:
                order_id = f'o{next_id}'
                next_id += 1

            price = rng.randint(90, 110)
            quantity = rng.randint(1, 15)

            if rng.random() < 0.04:
                price = 0

            if rng.random() < 0.04:
                quantity = -1

            commands.append(f'{side} {order_id} {price} {quantity}')

            try:
                book.add(order_id, side, price, quantity)
            except ValueError:
                pass

        elif choice < 0.85:
            if book.orders and rng.random() < 0.8:
                order_id = rng.choice(book.orders).order_id
            else:
                order_id = f'missing{index}'

            commands.append(f'CANCEL {order_id}')
            book.cancel(order_id)

        else:
            commands.append('BOOK')

        if index % 7 == 0:
            commands.append('BOOK')

    commands.extend(['BOOK', 'QUIT'])
    return commands

def expected_lines(commands: list[str]) -> list[str]:
    book = ReferenceBook()
    output = []

    for command in commands:
        parts = command.split()
        action = parts[0]

        if action in ('BUY', 'SELL'):
            order_id = parts[1]
            price = int(parts[2])
            quantity = int(parts[3])

            try:
                trades = book.add(order_id, action, price, quantity)

                output.append(
                    f'ACCEPTED {order_id}, trades={len(trades)}'
                )

                for buy_id, sell_id, trade_price, filled in trades:
                    output.append(
                        f'TRADE buy={buy_id} sell={sell_id} '
                        f'price={trade_price} qty={filled}'
                    )

            except ValueError as error:
                output.append(f'REJECTED: {error}')

        elif action == 'CANCEL':
            order_id = parts[1]
            status = (
                'CANCELLED' if book.cancel(order_id)
                else 'NOT FOUND'
            )
            output.append(f'{status} {order_id}')

        elif action == 'BOOK':
            for side, heading in [('BUY', 'BIDS'), ('SELL', 'ASKS')]:
                output.append(heading)

                for price, quantity, count in book.depth(side):
                    output.append(
                        f'  price={price} qty={quantity} '
                        f'orders={count}'
                    )

    return output

def actual_lines(cli_path: str, commands: list[str]) -> list[str]:
    process = subprocess.run(
        [cli_path],
        input='\n'.join(commands) + '\n',
        text=True,
        capture_output=True,
        timeout=30,
        check=False
    )

    if process.returncode != 0:
        raise RuntimeError(f'CLI failed: {process.stderr}')

    lines = process.stdout.splitlines()[2:]
    output = []

    for line in lines:
        if line.startswith('> '):
            line = line[2:]

        if line:
            output.append(line)

    return output

def main() -> int:
    if len(sys.argv) != 2:
        print('Usage: python3 tests/differential_test.py ./build/latency_cli')
        return 2

    cli_path = str(Path(sys.argv[1]).resolve())
    scenarios = 25
    steps = 200

    for index in range(scenarios):
        seed = 20261108 + index
        commands = generate_commands(seed, steps)

        expected = expected_lines(commands)
        actual = actual_lines(cli_path, commands)

        if actual != expected:
            length = max(len(expected), len(actual))

            mismatch = next(
                position for position in range(length)
                if position >= len(expected)
                or position >= len(actual)
                or expected[position] != actual[position]
            )

            expected_value = (
                expected[mismatch]
                if mismatch < len(expected) else '<missing>'
            )
            actual_value = (
                actual[mismatch]
                if mismatch < len(actual) else '<missing>'
            )

            print(f'FAIL seed={seed}, output_line={mismatch + 1}')
            print(f'Expected: {expected_value}')
            print(f'Actual:   {actual_value}')
            return 1

    print(
        f'PASS {scenarios} scenarios '
        f'with {steps} randomized actions each'
    )
    print(
        'Actions include add, match, cancel, '
        'invalid input, and book inspection'
    )
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
