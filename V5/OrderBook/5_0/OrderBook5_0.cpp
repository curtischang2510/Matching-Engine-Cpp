//
// Created by Curtis on 30/10/26.
//

#include "OrderBook5_0.h"

#include <algorithm>
#include <iterator>

namespace v5 {

OrderBook5_0::L1 OrderBook5_0::top() const {
    return l1_;
}

void OrderBook5_0::update_l1(Side side) {
    if (side == Side::BID) {
        l1_.highest_bid.reset();
        l1_.highest_bid_qty = 0;
        if (!bid_map.empty()) {
            const PriceBucket5_0& bucket = bid_map.rbegin()->second;
            const OrdersAtPrice5_0& level = bucket.levels[bucket.occupied.last()];
            l1_.highest_bid = level.price;
            l1_.highest_bid_qty = level.quantity;
        }
        if (!denseBidBitmap_.empty()) {
            const OrdersAtPrice5_0& level = denseBids_[denseBidBitmap_.last()];
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
        const PriceBucket5_0& bucket = ask_map.begin()->second;
        const OrdersAtPrice5_0& level = bucket.levels[bucket.occupied.first()];
        l1_.lowest_ask = level.price;
        l1_.lowest_ask_qty = level.quantity;
    }
    if (!denseAskBitmap_.empty()) {
        const OrdersAtPrice5_0& level = denseAsks_[denseAskBitmap_.first()];
        if (!l1_.lowest_ask || level.price < *l1_.lowest_ask) {
            l1_.lowest_ask = level.price;
            l1_.lowest_ask_qty = level.quantity;
        }
    }
}

void OrderBook5_0::apply(const AddOrder& o) {
    // match bid to ask
    if (o.side == Side::BID) {
        OrderBook5_0::handle_bid(o);
    } else {
        OrderBook5_0::handle_ask(o);
    }
}

void OrderBook5_0::apply(const CancelOrder& o) {
    const auto location = order_locations.find(o.header.order_id);
    if (location == order_locations.end()) return;

    // The registry gives us both the price level and the exact list node.
    const OrderLocation orderLoc = location->second;
    OrdersAtPrice5_0& level = *orderLoc.level;
    const std::uint64_t price = level.price;
    level.quantity -= orderLoc.order->quantity;
    level.orders.erase(orderLoc.order); // DLL so done in O(1) time
    order_locations.erase(location);

    // Only the last order at a price changes its occupancy bit.
    const bool level_empty = level.orders.empty();
    if (level_empty) {
        BucketMap& buckets = orderLoc.side == Side::BID ? bid_map : ask_map;
        if (orderLoc.bucket == buckets.end()) {
            (orderLoc.side == Side::BID ? denseBidBitmap_ : denseAskBitmap_).clear(price - denseLow_);
        } else {
            orderLoc.bucket->second.occupied.clear(price % kTicksPerBucket);
            if (orderLoc.bucket->second.occupied.empty()) buckets.erase(orderLoc.bucket);
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

void OrderBook5_0::handle_bid(const AddOrder& o) {
    const std::uint64_t remaining = match_bid_to_ask(o);
    const std::uint64_t filled = o.quantity - remaining;
    std::uint64_t resting_qty = 0;

    if (remaining > 0) {
        if (price_in_dense(o.price_ticks)) {
            const std::uint64_t price_idx = o.price_ticks - denseLow_;
            OrdersAtPrice5_0& orders_at_price = denseBids_[price_idx];
            orders_at_price.price = o.price_ticks;
            orders_at_price.orders.emplace_back(o.header.order_id, o.price_ticks, remaining, o.header.timestamp);
            denseBidBitmap_.set(price_idx);
            orders_at_price.quantity += remaining;
            resting_qty = orders_at_price.quantity;
            // Remember the new tail so a later cancel can erase it directly.
            order_locations.emplace(o.header.order_id, OrderLocation{
                Side::BID, &orders_at_price, std::prev(orders_at_price.orders.end()), bid_map.end()});
        } else {
            const std::uint64_t price_bucket = o.price_ticks / kTicksPerBucket;
            const std::uint64_t offset = o.price_ticks % kTicksPerBucket;
            auto [it, inserted] = bid_map.try_emplace(price_bucket);
            if (inserted) it->second.levels.resize(kTicksPerBucket);

            OrdersAtPrice5_0& level = it->second.levels[offset];
            level.price = o.price_ticks;
            level.orders.emplace_back(o.header.order_id, o.price_ticks, remaining,
                                      o.header.timestamp);
            it->second.occupied.set(offset); // indicate there are bids at this exact price
            level.quantity += remaining;
            resting_qty = level.quantity;
            order_locations.emplace(o.header.order_id, OrderLocation{
                Side::BID, &level, std::prev(level.orders.end()), it});
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

std::uint64_t OrderBook5_0::match_bid_to_ask(const AddOrder& o) {
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

        OrdersAtPrice5_0& level = use_tree ? tree->second.levels[tree_offset] : denseAsks_[dense_idx];
        // At one price, fill the oldest order first.
        while (!level.orders.empty() && remaining > 0) {
            Order& resting = level.orders.front();
            const std::uint64_t filled = std::min(remaining, resting.quantity);
            resting.quantity -= filled;
            level.quantity -= filled;
            remaining -= filled;
            if (resting.quantity == 0) {
                // A filled order must disappear from both the ID map and FIFO.
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

void OrderBook5_0::handle_ask(const AddOrder& o) {
    const std::uint64_t remaining = match_ask_to_bid(o);
    const std::uint64_t filled = o.quantity - remaining;
    std::uint64_t resting_qty = 0;

    if (remaining > 0) {
        if (price_in_dense(o.price_ticks)) {
            // Common prices stay in the preallocated window.
            const std::size_t price_idx = o.price_ticks - denseLow_;
            OrdersAtPrice5_0& level = denseAsks_[price_idx];
            level.price = o.price_ticks;
            level.orders.emplace_back(o.header.order_id, o.price_ticks, remaining, o.header.timestamp);
            level.quantity += remaining;
            denseAskBitmap_.set(price_idx);
            resting_qty = level.quantity;
            // The list iterator remains valid as other orders are added or removed.
            order_locations.emplace(o.header.order_id, OrderLocation{
                Side::ASK, &level, std::prev(level.orders.end()), ask_map.end()});
        } else {
            // Keep prices outside the window in the tree.
            const std::uint64_t price_bucket = o.price_ticks / kTicksPerBucket;
            const std::size_t offset = o.price_ticks % kTicksPerBucket;
            auto [it, inserted] = ask_map.try_emplace(price_bucket);
            if (inserted) it->second.levels.resize(kTicksPerBucket);

            OrdersAtPrice5_0& level = it->second.levels[offset];
            level.price = o.price_ticks;
            level.orders.emplace_back(o.header.order_id, o.price_ticks, remaining, o.header.timestamp);
            level.quantity += remaining;
            it->second.occupied.set(offset);
            resting_qty = level.quantity;
            order_locations.emplace(o.header.order_id, OrderLocation{
                Side::ASK, &level, std::prev(level.orders.end()), it});
        }
    }

    // A fill may have moved the best bid; a resting remainder may become the best ask.
    if (filled > 0) update_l1(Side::BID);
    if (remaining > 0 && (!l1_.lowest_ask || o.price_ticks <= *l1_.lowest_ask)) {
        l1_.lowest_ask = o.price_ticks;
        l1_.lowest_ask_qty = resting_qty;
    }
}

std::uint64_t OrderBook5_0::match_ask_to_bid(const AddOrder& o) {
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
        // Compare both regions: a large ask can cross tree, dense, then tree again.
        const std::uint64_t tree_price = have_tree ? tree->second.levels[tree_offset].price : 0;
        const bool use_tree = have_tree && (!have_dense || tree_price > dense_price);
        const std::uint64_t best_price = use_tree ? tree_price : dense_price;
        if (best_price < o.price_ticks) break;

        OrdersAtPrice5_0& level = use_tree ? tree->second.levels[tree_offset] : denseBids_[dense_idx];
        // At one price, fill the oldest order first.
        while (!level.orders.empty() && remaining > 0) {
            Order& resting = level.orders.front();
            const std::uint64_t filled = std::min(remaining, resting.quantity);
            resting.quantity -= filled;
            level.quantity -= filled;
            remaining -= filled;
            if (resting.quantity == 0) {
                // Remove the lookup before popping the node it identifies.
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

} // namespace v5
