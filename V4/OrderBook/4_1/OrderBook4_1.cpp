//
// Created by Curtis on 30/10/26.
//

#include "OrderBook4_1.h"

#include <algorithm>
#include <cassert>
#include <iterator>

namespace v4 {

OrderBook4_1::L1 OrderBook4_1::top() const {
    return l1_;
}

void OrderBook4_1::update_l1(Side side) {
    if (side == Side::BID) {
        l1_.highest_bid.reset();
        l1_.highest_bid_qty = 0;
        if (!bid_map.empty()) {
            const PriceBucket& bucket = bid_map.rbegin()->second;
            const OrdersAtPrice& level = bucket.levels[bucket.occupied.last()];
            l1_.highest_bid = level.price;
            l1_.highest_bid_qty = level.quantity;
        }
        if (!denseBidBitmap_.empty()) {
            const OrdersAtPrice& level = denseBids_[denseBidBitmap_.last()];
            if (!l1_.highest_bid || level.price > *l1_.highest_bid) {
                l1_.highest_bid = level.price;
                l1_.highest_bid_qty = level.quantity;
            }
        }
        return;
    }

    l1_.lowest_ask.reset();
    l1_.lowest_ask_qty = 0;
    if (!ask_map.empty()) {
        const PriceBucket& bucket = ask_map.begin()->second;
        const OrdersAtPrice& level = bucket.levels[bucket.occupied.first()];
        l1_.lowest_ask = level.price;
        l1_.lowest_ask_qty = level.quantity;
    }
    if (!denseAskBitmap_.empty()) {
        const OrdersAtPrice& level = denseAsks_[denseAskBitmap_.first()];
        if (!l1_.lowest_ask || level.price < *l1_.lowest_ask) {
            l1_.lowest_ask = level.price;
            l1_.lowest_ask_qty = level.quantity;
        }
    }
}

void OrderBook4_1::apply(const AddOrder& o) {
    // match bid to ask
    if (o.side == Side::BID) {
        OrderBook4_1::handle_bid(o);
    } else {
        OrderBook4_1::handle_ask(o);
    }
}

void OrderBook4_1::apply(const CancelOrder& o) {
    const auto location = order_locations.find(o.header.order_id);
    // order doesn't exist
    if (location == order_locations.end()) return;

    const OrderLocation orderLoc = location->second;
    const std::uint64_t price = orderLoc.price;
    const bool dense = price_in_dense(price);
    const std::size_t dense_idx = dense ? price - denseLow_ : 0;
    std::map<std::uint64_t, PriceBucket>& buckets = orderLoc.side == Side::BID ? bid_map : ask_map;
    auto bucket = buckets.end();
    OrdersAtPrice* level_ptr;
    if (dense) {
        // The price tells us which preallocated slot holds the order.
        level_ptr = &(orderLoc.side == Side::BID ? denseBids_[dense_idx] : denseAsks_[dense_idx]);
    } else {
        bucket = buckets.find(price / kTicksPerBucket);
        level_ptr = &bucket->second.levels[price % kTicksPerBucket];
    }
    OrdersAtPrice& level = *level_ptr;
    std::deque<Order>& orders = level.orders;

    // cancel order
    const auto order = std::find_if(orders.begin(), orders.end(),
        [&](const Order& resting) { return resting.id == o.header.order_id; });
    assert(order != orders.end());
    level.quantity -= order->quantity;
    orders.erase(order);
    // removing from unordered map
    order_locations.erase(location);

    // Clear occupancy only when the last order at this price is gone.
    const bool level_empty = orders.empty();
    if (level_empty) {
        if (dense) {
            (orderLoc.side == Side::BID ? denseBidBitmap_ : denseAskBitmap_).clear(dense_idx);
        } else {
            bucket->second.occupied.clear(price % kTicksPerBucket);
            if (bucket->second.occupied.empty()) buckets.erase(bucket);
        }
    }

    if (orderLoc.side == Side::BID && l1_.highest_bid == price) {
        if (level_empty) update_l1(Side::BID);
        else l1_.highest_bid_qty = level.quantity;
    } else if (orderLoc.side == Side::ASK && l1_.lowest_ask == price) {
        if (level_empty) update_l1(Side::ASK);
        else l1_.lowest_ask_qty = level.quantity;
    }
}

void OrderBook4_1::handle_bid(const AddOrder& o) {
    const std::uint64_t remaining = match_bid_to_ask(o);
    const std::uint64_t filled = o.quantity - remaining;
    std::uint64_t resting_qty = 0;

    if (remaining > 0) {
        if (price_in_dense(o.price_ticks)) {
            const std::uint64_t price_idx = o.price_ticks - denseLow_;
            OrdersAtPrice& orders_at_price = denseBids_[price_idx];
            orders_at_price.price = o.price_ticks;
            orders_at_price.orders.emplace_back(o.header.order_id, o.price_ticks, remaining, o.header.timestamp);
            denseBidBitmap_.set(price_idx);
            orders_at_price.quantity += remaining;
            resting_qty = orders_at_price.quantity;
            order_locations[o.header.order_id] = {Side::BID, o.price_ticks};
        } else {
            const std::uint64_t price_bucket = o.price_ticks / kTicksPerBucket;
            const std::uint64_t offset = o.price_ticks % kTicksPerBucket;
            auto [it, inserted] = bid_map.try_emplace(price_bucket);
            if (inserted) it->second.levels.resize(kTicksPerBucket);

            OrdersAtPrice& level = it->second.levels[offset];
            level.price = o.price_ticks;
            level.orders.emplace_back(o.header.order_id, o.price_ticks, remaining,
                                      o.header.timestamp);
            it->second.occupied.set(offset); // indicate there are bids at this exact price
            level.quantity += remaining;
            resting_qty = level.quantity;
            order_locations[o.header.order_id] = {Side::BID, o.price_ticks};
        }
    }

    // update L1
    // if filled > 0, possible there is an update to best ask price
    if (filled > 0) update_l1(Side::ASK);
    if (remaining > 0 && (!l1_.highest_bid || o.price_ticks >= *l1_.highest_bid)) {
        l1_.highest_bid = o.price_ticks;
        l1_.highest_bid_qty = resting_qty;
    }
}

std::uint64_t OrderBook4_1::match_bid_to_ask(const AddOrder& o) {
    std::uint64_t remaining = o.quantity;
    while (remaining > 0) {
        const bool have_tree = !ask_map.empty();
        const bool have_dense = !denseAskBitmap_.empty();
        if (!have_tree && !have_dense) break;

        // The cheapest ask wins, whether it lives in the tree or dense array.
        const std::size_t dense_idx = have_dense ? denseAskBitmap_.first() : 0;
        const std::uint64_t dense_price = denseLow_ + dense_idx;
        auto tree = ask_map.end();
        std::size_t tree_offset = 0;
        if (have_tree) {
            tree = ask_map.begin();
            tree_offset = tree->second.occupied.first();
        }
        // Tree prices can sit on either side of the dense window.
        // A large bid may cross tree, dense, then tree again.
        const std::uint64_t tree_price = have_tree ? tree->second.levels[tree_offset].price : 0;
        const bool use_tree = have_tree && (!have_dense || tree_price < dense_price);
        const std::uint64_t best_price = use_tree ? tree_price : dense_price;
        if (best_price > o.price_ticks) break;

        OrdersAtPrice& level = use_tree ? tree->second.levels[tree_offset] : denseAsks_[dense_idx];
        // At one price, fill the oldest order first.
        while (!level.orders.empty() && remaining > 0) {
            Order& resting = level.orders.front();
            const std::uint64_t filled = std::min(remaining, resting.quantity);
            resting.quantity -= filled;
            level.quantity -= filled;
            remaining -= filled;
            if (resting.quantity == 0) {
                order_locations.erase(resting.id);
                level.orders.pop_front();
            }
        }

        if (level.orders.empty()) {
            if (use_tree) {
                tree->second.occupied.clear(tree_offset);
                if (tree->second.occupied.empty()) ask_map.erase(tree);
            } else {
                denseAskBitmap_.clear(dense_idx);
            }
        }
    }

    return remaining;
}

void OrderBook4_1::handle_ask(const AddOrder& o) {
    const std::uint64_t remaining = match_ask_to_bid(o);
    const std::uint64_t filled = o.quantity - remaining;
    std::uint64_t resting_qty = 0;

    if (remaining > 0) {
        if (price_in_dense(o.price_ticks)) {
            // Common prices stay in the preallocated window.
            const std::size_t price_idx = o.price_ticks - denseLow_;
            OrdersAtPrice& level = denseAsks_[price_idx];
            level.price = o.price_ticks;
            level.orders.emplace_back(o.header.order_id, o.price_ticks, remaining, o.header.timestamp);
            level.quantity += remaining;
            denseAskBitmap_.set(price_idx);
            resting_qty = level.quantity;
        } else {
            // Keep prices outside the window in the tree.
            const std::uint64_t price_bucket = o.price_ticks / kTicksPerBucket;
            const std::size_t offset = o.price_ticks % kTicksPerBucket;
            auto [it, inserted] = ask_map.try_emplace(price_bucket);
            if (inserted) it->second.levels.resize(kTicksPerBucket);

            OrdersAtPrice& level = it->second.levels[offset];
            level.price = o.price_ticks;
            level.orders.emplace_back(o.header.order_id, o.price_ticks, remaining, o.header.timestamp);
            level.quantity += remaining;
            it->second.occupied.set(offset);
            resting_qty = level.quantity;
        }
        order_locations[o.header.order_id] = {Side::ASK, o.price_ticks};
    }

    // A fill may have moved the best bid; a resting remainder may become the best ask.
    if (filled > 0) update_l1(Side::BID);
    if (remaining > 0 && (!l1_.lowest_ask || o.price_ticks <= *l1_.lowest_ask)) {
        l1_.lowest_ask = o.price_ticks;
        l1_.lowest_ask_qty = resting_qty;
    }
}

std::uint64_t OrderBook4_1::match_ask_to_bid(const AddOrder& o) {
    std::uint64_t remaining = o.quantity;
    while (remaining > 0) {
        const bool have_tree = !bid_map.empty();
        const bool have_dense = !denseBidBitmap_.empty();
        if (!have_tree && !have_dense) break;

        // The highest bid wins, regardless of where it is stored.
        const std::size_t dense_idx = have_dense ? denseBidBitmap_.last() : 0;
        const std::uint64_t dense_price = denseLow_ + dense_idx;
        auto tree = bid_map.end();
        std::size_t tree_offset = 0;
        if (have_tree) {
            tree = std::prev(bid_map.end());
            tree_offset = tree->second.occupied.last();
        }
        // the best bid is either the highest price in the tree (if non-empty)
        // or the highest occupied slot (price) in dense arr
        // we don't always assume the tree's bid will be higher than the dense arr because if the order is large enough
        // we can traverse tree -> dense arr -> tree
        const std::uint64_t tree_price = have_tree ? tree->second.levels[tree_offset].price : 0;
        const bool use_tree = have_tree && (!have_dense || tree_price > dense_price);
        const std::uint64_t best_price = use_tree ? tree_price : dense_price;
        if (best_price < o.price_ticks) break;

        OrdersAtPrice& level = use_tree ? tree->second.levels[tree_offset] : denseBids_[dense_idx];
        // At one price, fill the oldest order first.
        while (!level.orders.empty() && remaining > 0) {
            Order& resting = level.orders.front();
            const std::uint64_t filled = std::min(remaining, resting.quantity);
            resting.quantity -= filled;
            level.quantity -= filled;
            remaining -= filled;
            if (resting.quantity == 0) {
                order_locations.erase(resting.id);
                level.orders.pop_front();
            }
        }

        if (level.orders.empty()) {
            if (use_tree) {
                tree->second.occupied.clear(tree_offset);
                if (tree->second.occupied.empty()) bid_map.erase(tree);
            } else {
                denseBidBitmap_.clear(dense_idx);
            }
        }
    }

    return remaining;
}

} // namespace v4
