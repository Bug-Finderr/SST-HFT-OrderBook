#include <cstdint>
#include <vector>
#include <string>
#include <map>
#include <list>
#include <unordered_map>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <memory>
#include <cstring>

using namespace std;

struct Order {
    uint64_t order_id;
    bool is_buy;
    double price;
    uint64_t quantity;
    uint64_t timestamp_ns;

    Order(uint64_t id, bool buy, double p, uint64_t q, uint64_t ts)
        : order_id(id), is_buy(buy), price(p), quantity(q), timestamp_ns(ts) {}
};

struct PriceLevel {
    double price;
    uint64_t total_quantity;

    PriceLevel(double p = 0.0, uint64_t q = 0) : price(p), total_quantity(q) {}
};

template<typename T, size_t BlockSize = 4096>
class MemoryPool {
private:
    struct Block {
        uint8_t data[BlockSize];
        Block* next;
        Block() : next(nullptr) {}
    };

    Block* current_block;
    size_t offset;
    vector<Block*> all_blocks;

public:
    MemoryPool() : current_block(nullptr), offset(0) {
        allocate_block();
    }

    ~MemoryPool() {
        for (auto* block : all_blocks) {
            delete block;
        }
    }

    void allocate_block() {
        Block* new_block = new Block();
        all_blocks.push_back(new_block);
        if (current_block) {
            current_block->next = new_block;
        }
        current_block = new_block;
        offset = 0;
    }

    template<typename... Args>
    T* allocate(Args&&... args) {
        if (offset + sizeof(T) > BlockSize) {
            allocate_block();
        }

        void* ptr = current_block->data + offset;
        offset += sizeof(T);

        return new (ptr) T(std::forward<Args>(args)...);
    }

    void reset() {
        offset = 0;
        current_block = all_blocks.empty() ? nullptr : all_blocks[0];
    }
};

class OrderBook {
private:
    // price level data structure: price -> list of orders (FIFO)
    using OrderList = list<Order*>;

    struct PriceLevelData {
        OrderList orders;
        uint64_t total_quantity = 0;

        inline void add_order(Order* order) {
            orders.push_back(order);
            total_quantity += order->quantity;
        }

        inline void remove_order(OrderList::iterator it) {
            total_quantity -= (*it)->quantity;
            orders.erase(it);
        }

        inline void update_quantity(Order* order, uint64_t old_qty) {
            total_quantity = total_quantity - old_qty + order->quantity;
        }
    };

    // bids: descending order
    map<double, PriceLevelData, greater<double>> bids;

    // asks: ascending order
    map<double, PriceLevelData, less<double>> asks;

    // O(1) order lookup: order_id -> (price, iterator, is_buy)
    struct OrderLocation {
        double price;
        OrderList::iterator it;
        bool is_buy;
    };
    unordered_map<uint64_t, OrderLocation> order_lookup;

    // memory pool for orders
    MemoryPool<Order, 8192> order_pool;

public:
    OrderBook() = default;

    // prevent copying
    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;

    // insert new order into book
    void add_order(const Order& order) {
        // allocate order from memory pool
        Order* new_order = order_pool.allocate(
            order.order_id, order.is_buy, order.price,
            order.quantity, order.timestamp_ns
        );

        if (order.is_buy) {
            auto& price_level = bids[order.price];
            price_level.add_order(new_order);
            auto it = price_level.orders.end();
            --it;  // point to newly added order
            order_lookup[order.order_id] = {order.price, it, order.is_buy};
        } else {
            auto& price_level = asks[order.price];
            price_level.add_order(new_order);
            auto it = price_level.orders.end();
            --it;  // point to newly added order
            order_lookup[order.order_id] = {order.price, it, order.is_buy};
        }
    }

    // cancel existing order by ID
    bool cancel_order(uint64_t order_id) {
        auto lookup_it = order_lookup.find(order_id);
        if (lookup_it == order_lookup.end()) {
            return false;
        }

        auto& loc = lookup_it->second;

        if (loc.is_buy) {
            auto side_it = bids.find(loc.price);
            if (side_it == bids.end()) {
                return false;
            }
            auto& price_level = side_it->second;
            price_level.remove_order(loc.it);
            if (price_level.orders.empty()) {
                bids.erase(side_it);
            }
        } else {
            auto side_it = asks.find(loc.price);
            if (side_it == asks.end()) {
                return false;
            }
            auto& price_level = side_it->second;
            price_level.remove_order(loc.it);
            if (price_level.orders.empty()) {
                asks.erase(side_it);
            }
        }

        // remove from lookup
        order_lookup.erase(lookup_it);

        return true;
    }

    // amend existing order's price or quantity
    bool amend_order(uint64_t order_id, double new_price, uint64_t new_quantity) {
        auto lookup_it = order_lookup.find(order_id);
        if (lookup_it == order_lookup.end()) {
            return false;
        }

        auto& loc = lookup_it->second;
        Order* order = *loc.it;

        // if price changes, treat as cancel + add
        if (order->price != new_price) {
            bool is_buy = order->is_buy;
            uint64_t timestamp = order->timestamp_ns;

            cancel_order(order_id);
            add_order(Order(order_id, is_buy, new_price, new_quantity, timestamp));
        } else {
            // only quantity changes - update in place
            if (loc.is_buy) {
                auto& price_level = bids[loc.price];
                uint64_t old_qty = order->quantity;
                order->quantity = new_quantity;
                price_level.update_quantity(order, old_qty);
            } else {
                auto& price_level = asks[loc.price];
                uint64_t old_qty = order->quantity;
                order->quantity = new_quantity;
                price_level.update_quantity(order, old_qty);
            }

            // if quantity becomes 0, remove order
            if (new_quantity == 0) {
                return cancel_order(order_id);
            }
        }

        return true;
    }

    // get a snapshot of top N bid and ask levels
    void get_snapshot(size_t depth, vector<PriceLevel>& bids_out, vector<PriceLevel>& asks_out) const {
        bids_out.clear();
        asks_out.clear();
        bids_out.reserve(depth);
        asks_out.reserve(depth);

        // get top N bids (already in descending order)
        size_t count = 0;
        for (const auto& [price, level] : bids) {
            if (count >= depth) break;
            bids_out.emplace_back(price, level.total_quantity);
            ++count;
        }

        // get top N asks (already in ascending order)
        count = 0;
        for (const auto& [price, level] : asks) {
            if (count >= depth) break;
            asks_out.emplace_back(price, level.total_quantity);
            ++count;
        }
    }

    // print current state of order book
    void print_book(size_t depth = 10) const {
        vector<PriceLevel> bids_snapshot, asks_snapshot;
        get_snapshot(depth, bids_snapshot, asks_snapshot);

        cout << "\n" << string(50, '=') << "\n";
        cout << "ORDER BOOK (Top " << depth << " levels)\n";
        cout << string(50, '=') << "\n\n";

        cout << setw(15) << "BIDS" << " | " << setw(15) << "ASKS" << "\n";
        cout << setw(8) << "Price" << " " << setw(6) << "Qty"
                  << " | " << setw(8) << "Price" << " " << setw(6) << "Qty" << "\n";
        cout << string(50, '-') << "\n";

        size_t max_levels = max(bids_snapshot.size(), asks_snapshot.size());
        for (size_t i = 0; i < max_levels; ++i) {
            // print bid
            if (i < bids_snapshot.size()) {
                cout << fixed << setprecision(2)
                         << setw(8) << bids_snapshot[i].price << " "
                         << setw(6) << bids_snapshot[i].total_quantity;
            } else {
                cout << setw(15) << " ";
            }

            cout << " | ";

            // print ask
            if (i < asks_snapshot.size()) {
                cout << fixed << setprecision(2)
                         << setw(8) << asks_snapshot[i].price << " "
                         << setw(6) << asks_snapshot[i].total_quantity;
            }

            cout << "\n";
        }

        cout << string(50, '=') << "\n";

        // print spread
        if (!bids_snapshot.empty() && !asks_snapshot.empty()) {
            double spread = asks_snapshot[0].price - bids_snapshot[0].price;
            cout << "Spread: " << fixed << setprecision(2) << spread << "\n";
        }

        cout << string(50, '=') << "\n\n";
    }

    // utility functions for testing
    size_t get_total_orders() const {
        return order_lookup.size();
    }

    size_t get_bid_levels() const {
        return bids.size();
    }

    size_t get_ask_levels() const {
        return asks.size();
    }
};
