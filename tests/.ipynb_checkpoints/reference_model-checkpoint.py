from dataclasses import dataclass

@dataclass
class Order:
    order_id: str
    side: str
    price: int
    quantity: int
    sequence: int

class ReferenceBook:
    def __init__(self):
        self.orders: list[Order] = []
        self.sequence = 0

    def add(self, order_id: str, side: str, price: int, quantity: int):
        if not order_id or price <= 0 or quantity <= 0:
            raise ValueError('id must be nonempty; price and quantity must be positive')

        if any(order.order_id == order_id for order in self.orders):
            raise ValueError('duplicate active order id')

        trades = []

        while quantity > 0:
            if side == 'BUY':
                eligible = [
                    order for order in self.orders
                    if order.side == 'SELL' and order.price <= price
                ]
                maker = min(
                    eligible,
                    key=lambda order: (order.price, order.sequence),
                    default=None
                )
            else:
                eligible = [
                    order for order in self.orders
                    if order.side == 'BUY' and order.price >= price
                ]
                maker = min(
                    eligible,
                    key=lambda order: (-order.price, order.sequence),
                    default=None
                )

            if maker is None:
                break

            filled = min(quantity, maker.quantity)
            buy_id = order_id if side == 'BUY' else maker.order_id
            sell_id = maker.order_id if side == 'BUY' else order_id

            trades.append((buy_id, sell_id, maker.price, filled))

            quantity -= filled
            maker.quantity -= filled

            if maker.quantity == 0:
                self.orders.remove(maker)

        if quantity > 0:
            self.orders.append(
                Order(order_id, side, price, quantity, self.sequence)
            )
            self.sequence += 1

        return trades

    def cancel(self, order_id: str) -> bool:
        for order in self.orders:
            if order.order_id == order_id:
                self.orders.remove(order)
                return True

        return False

    def depth(self, side: str, max_levels: int = 5):
        totals = {}

        for order in self.orders:
            if order.side == side:
                quantity, count = totals.get(order.price, (0, 0))
                totals[order.price] = (
                    quantity + order.quantity,
                    count + 1
                )

        prices = sorted(totals, reverse=(side == 'BUY'))

        return [
            (price, *totals[price])
            for price in prices[:max_levels]
        ]
