//
// Created by Curtis on 30/10/26.
//

#include "OrderBook3_0.h"

#include <algorithm>
#include <cassert>
#include <iterator>

OrderBook3_0::L1 OrderBook3_0::top() const {
    return l1_;
}

void OrderBook3_0::update_l1(Side side) {
    if (side == Side::BID) {
        l1_.highest_bid.reset();
        l1_.highest_bid_qty = 0;
        if (bid_map.empty()) return;
        const PriceBucket& bucket = bid_map.rbegin()->second;
        for (auto level = bucket.levels.rbegin(); level != bucket.levels.rend(); ++level) {
            if (level->orders.empty()) continue;
            l1_.highest_bid = level->price;
            l1_.highest_bid_qty = level->quantity;
            return;
        }
        return;
    }

    l1_.lowest_ask.reset();
    l1_.lowest_ask_qty = 0;
    if (ask_map.empty()) return;
    const PriceBucket& bucket = ask_map.begin()->second;
    for (const OrdersAtPrice& level : bucket.levels) {
        if (level.orders.empty()) continue;
        l1_.lowest_ask = level.price;
        l1_.lowest_ask_qty = level.quantity;
        return;
    }
}

void OrderBook3_0::apply(const AddOrder& o) {
    // match bid to ask
    if (o.side == Side::BID) {
        OrderBook3_0::handle_bid(o);
    } else {
        OrderBook3_0::handle_ask(o);
    }
}

void OrderBook3_0::apply(const CancelOrder& o) {
    const auto location = order_locations.find(o.header.order_id);
    // order doesn't exist
    if (location == order_locations.end()) return;

    const OrderLocation orderLoc = location->second;
    const std::uint64_t price = orderLoc.price;
    std::map<std::uint64_t, PriceBucket>& buckets =
        orderLoc.side == Side::BID ? bid_map : ask_map;
    auto bucket = buckets.find(price / kTicksPerBucket);
    OrdersAtPrice& level = bucket->second.levels[price % kTicksPerBucket];
    std::deque<Order>& orders = level.orders;

    // cancel order
    const auto order = std::find_if(orders.begin(), orders.end(),
        [&](const Order& resting) { return resting.id == o.header.order_id; });
    assert(order != orders.end());
    level.quantity -= order->quantity;
    orders.erase(order);
    order_locations.erase(location);

    // if price bucket is empty, remove from ordered map
    const bool level_empty = orders.empty();
    if (level_empty) {
        const bool bucket_empty = std::all_of(bucket->second.levels.begin(), bucket->second.levels.end(),
            [](const OrdersAtPrice& price_level) { return price_level.orders.empty(); });
        if (bucket_empty) buckets.erase(bucket);
    }

    if (orderLoc.side == Side::BID && l1_.highest_bid == price) {
        if (level_empty) update_l1(Side::BID);
        else l1_.highest_bid_qty = level.quantity;
    } else if (orderLoc.side == Side::ASK && l1_.lowest_ask == price) {
        if (level_empty) update_l1(Side::ASK);
        else l1_.lowest_ask_qty = level.quantity;
    }
}

void OrderBook3_0::handle_bid(const AddOrder& o) {
    const std::uint64_t remaining = match_bid_to_ask(o);
    const std::uint64_t filled = o.quantity - remaining;
    std::uint64_t resting_qty = 0;

    if (remaining > 0) {
        const std::uint64_t price_bucket = o.price_ticks / kTicksPerBucket;
        const std::uint64_t offset = o.price_ticks % kTicksPerBucket;
        auto [it, inserted] = bid_map.try_emplace(price_bucket);
        if (inserted) it->second.levels.resize(kTicksPerBucket);

        OrdersAtPrice& level = it->second.levels[offset];
        level.price = o.price_ticks;
        level.orders.emplace_back(o.header.order_id, o.price_ticks, remaining,
                                  o.header.timestamp);
        level.quantity += remaining;
        resting_qty = level.quantity;
        order_locations[o.header.order_id] = {Side::BID, o.price_ticks};
    }

    // update L1
    // if filled > 0, possible there is an update to best ask price
    if (filled > 0) update_l1(Side::ASK);
    if (remaining > 0 && (!l1_.highest_bid || o.price_ticks >= *l1_.highest_bid)) {
        l1_.highest_bid = o.price_ticks;
        l1_.highest_bid_qty = resting_qty;
    }
}

std::uint64_t OrderBook3_0::match_bid_to_ask(const AddOrder& o) {
    std::uint64_t remaining = o.quantity;
    const std::uint64_t limit_bucket = o.price_ticks / kTicksPerBucket;

    // Buckets and offsets are ascending, so the cheapest ask matches first.
    for (auto it = ask_map.begin(); it != ask_map.end() && remaining > 0; ) {
        if (it->first > limit_bucket) break;

        std::vector<OrdersAtPrice>& levels = it->second.levels;
        const std::size_t last_offset = it->first == limit_bucket
            ? o.price_ticks % kTicksPerBucket
            : kTicksPerBucket - 1;

        for (std::size_t offset = 0;
             offset < levels.size() && offset <= last_offset && remaining > 0;
             ++offset) {
            OrdersAtPrice& level = levels[offset];
            std::deque<Order>& orders = level.orders;

            // The front is the oldest resting order at this exact price.
            while (!orders.empty() && remaining > 0) {
                Order& resting = orders.front();
                const std::uint64_t filled = std::min(remaining, resting.quantity);
                resting.quantity -= filled;
                level.quantity -= filled;
                remaining -= filled;

                if (resting.quantity == 0) {
                    order_locations.erase(resting.id);
                    orders.pop_front();
                }
            }
        }

        // Remove a spent bucket so it cannot appear as a best-price candidate.
        // Even with 1000 price levels this is taking a long time
        const bool empty = std::all_of(levels.begin(), levels.end(),
            [](const OrdersAtPrice& level) { return level.orders.empty(); });
        if (empty) {
            it = ask_map.erase(it);
        } else {
            ++it;
        }
    }

    return remaining;
}

void OrderBook3_0::handle_ask(const AddOrder& o) {
    const std::uint64_t remaining = match_ask_to_bid(o);
    const std::uint64_t filled = o.quantity - remaining;
    std::uint64_t resting_qty = 0;

    if (remaining > 0) {
        const std::uint64_t price_bucket = o.price_ticks / kTicksPerBucket;
        const std::uint64_t offset = o.price_ticks % kTicksPerBucket;
        auto [it, inserted] = ask_map.try_emplace(price_bucket);
        if (inserted) it->second.levels.resize(kTicksPerBucket);

        OrdersAtPrice& level = it->second.levels[offset];
        level.price = o.price_ticks;
        level.orders.emplace_back(o.header.order_id, o.price_ticks, remaining,
                                  o.header.timestamp);
        level.quantity += remaining;
        resting_qty = level.quantity;
        order_locations[o.header.order_id] = {Side::ASK, o.price_ticks};
    }

    // Both maps now contain the final state for this order.
    if (filled > 0) update_l1(Side::BID);
    if (remaining > 0 && (!l1_.lowest_ask || o.price_ticks <= *l1_.lowest_ask)) {
        l1_.lowest_ask = o.price_ticks;
        l1_.lowest_ask_qty = resting_qty;
    }
}

std::uint64_t OrderBook3_0::match_ask_to_bid(const AddOrder& o) {
    std::uint64_t remaining = o.quantity;
    const std::uint64_t limit_bucket = o.price_ticks / kTicksPerBucket;
    const std::size_t limit_offset = o.price_ticks % kTicksPerBucket;

    // Always start with the highest remaining bid bucket.
    while (!bid_map.empty() && remaining > 0) {
        auto current = std::prev(bid_map.end());
        if (current->first < limit_bucket) break;

        std::vector<OrdersAtPrice>& levels = current->second.levels;
        for (std::size_t index = levels.size(); index > 0 && remaining > 0; --index) {
            const std::size_t offset = index - 1;
            if (current->first == limit_bucket && offset < limit_offset) break;

            OrdersAtPrice& level = levels[offset];
            std::deque<Order>& orders = level.orders;
            // At one price, match the oldest resting bid first.
            while (!orders.empty() && remaining > 0) {
                Order& resting = orders.front();
                const std::uint64_t filled = std::min(remaining, resting.quantity);
                resting.quantity -= filled;
                level.quantity -= filled;
                remaining -= filled;

                if (resting.quantity == 0) {
                    order_locations.erase(resting.id);
                    orders.pop_front();
                }
            }
        }

        // A nonempty bucket means the ask is filled or the remaining bids are below its limit.
        const bool empty = std::all_of(levels.begin(), levels.end(),
            [](const OrdersAtPrice& level) { return level.orders.empty(); });
        if (empty) {
            // O(log(n)) operation. Recolouring the RB tree
            bid_map.erase(current);
        } else {
            break;
        }
    }

    return remaining;
}
