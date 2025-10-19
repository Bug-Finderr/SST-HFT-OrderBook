// AI generated

#include "main.cpp"
#include <chrono>
#include <random>
#include <cassert>
#include <numeric>

using namespace std;

// ============================================================================
// Timing Utilities
// ============================================================================

class Timer {
private:
    chrono::high_resolution_clock::time_point start_time;

public:
    Timer() : start_time(chrono::high_resolution_clock::now()) {}

    void reset() {
        start_time = chrono::high_resolution_clock::now();
    }

    // Returns elapsed time in nanoseconds
    int64_t elapsed_ns() const {
        auto end_time = chrono::high_resolution_clock::now();
        return chrono::duration_cast<chrono::nanoseconds>(end_time - start_time).count();
    }

    // Returns elapsed time in microseconds
    double elapsed_us() const {
        return elapsed_ns() / 1000.0;
    }

    // Returns elapsed time in milliseconds
    double elapsed_ms() const {
        return elapsed_ns() / 1000000.0;
    }
};

// ============================================================================
// Test Framework
// ============================================================================

int test_count = 0;
int test_passed = 0;

#define TEST(name) \
    void name(); \
    struct name##_runner { \
        name##_runner() { \
            test_count++; \
            cout << "Running test: " << #name << "... "; \
            try { \
                name(); \
                test_passed++; \
                cout << "PASSED\n"; \
            } catch (const exception& e) { \
                cout << "FAILED: " << e.what() << "\n"; \
            } \
        } \
    } name##_instance; \
    void name()

#define ASSERT(condition, message) \
    if (!(condition)) { \
        throw runtime_error(string("Assertion failed: ") + message); \
    }

// ============================================================================
// Unit Tests
// ============================================================================

TEST(test_add_order_basic) {
    OrderBook book;

    // Add buy order
    book.add_order(Order(1, true, 100.0, 10, 1000));
    ASSERT(book.get_total_orders() == 1, "Should have 1 order");
    ASSERT(book.get_bid_levels() == 1, "Should have 1 bid level");

    // Add sell order
    book.add_order(Order(2, false, 101.0, 20, 2000));
    ASSERT(book.get_total_orders() == 2, "Should have 2 orders");
    ASSERT(book.get_ask_levels() == 1, "Should have 1 ask level");
}

TEST(test_add_multiple_orders_same_price) {
    OrderBook book;

    // Add multiple orders at same price
    book.add_order(Order(1, true, 100.0, 10, 1000));
    book.add_order(Order(2, true, 100.0, 20, 2000));
    book.add_order(Order(3, true, 100.0, 30, 3000));

    ASSERT(book.get_total_orders() == 3, "Should have 3 orders");
    ASSERT(book.get_bid_levels() == 1, "Should have 1 bid level");

    // Check aggregated quantity
    vector<PriceLevel> bids, asks;
    book.get_snapshot(5, bids, asks);
    ASSERT(bids.size() == 1, "Should have 1 bid level");
    ASSERT(bids[0].total_quantity == 60, "Total quantity should be 60");
}

TEST(test_cancel_order) {
    OrderBook book;

    book.add_order(Order(1, true, 100.0, 10, 1000));
    book.add_order(Order(2, true, 100.0, 20, 2000));

    ASSERT(book.get_total_orders() == 2, "Should have 2 orders");

    // Cancel first order
    bool result = book.cancel_order(1);
    ASSERT(result, "Cancel should succeed");
    ASSERT(book.get_total_orders() == 1, "Should have 1 order");

    // Check quantity updated
    vector<PriceLevel> bids, asks;
    book.get_snapshot(5, bids, asks);
    ASSERT(bids[0].total_quantity == 20, "Total quantity should be 20");

    // Cancel non-existent order
    result = book.cancel_order(999);
    ASSERT(!result, "Cancel should fail for non-existent order");
}

TEST(test_cancel_removes_price_level) {
    OrderBook book;

    book.add_order(Order(1, true, 100.0, 10, 1000));
    ASSERT(book.get_bid_levels() == 1, "Should have 1 bid level");

    book.cancel_order(1);
    ASSERT(book.get_bid_levels() == 0, "Should have 0 bid levels");
}

TEST(test_amend_order_quantity) {
    OrderBook book;

    book.add_order(Order(1, true, 100.0, 10, 1000));

    // Amend quantity only
    bool result = book.amend_order(1, 100.0, 50);
    ASSERT(result, "Amend should succeed");

    vector<PriceLevel> bids, asks;
    book.get_snapshot(5, bids, asks);
    ASSERT(bids[0].total_quantity == 50, "Quantity should be updated to 50");
}

TEST(test_amend_order_price) {
    OrderBook book;

    book.add_order(Order(1, true, 100.0, 10, 1000));
    ASSERT(book.get_bid_levels() == 1, "Should have 1 bid level");

    // Amend price (should be treated as cancel + add)
    bool result = book.amend_order(1, 101.0, 10);
    ASSERT(result, "Amend should succeed");
    ASSERT(book.get_bid_levels() == 1, "Should still have 1 bid level");

    vector<PriceLevel> bids, asks;
    book.get_snapshot(5, bids, asks);
    ASSERT(bids[0].price == 101.0, "Price should be updated to 101.0");
}

TEST(test_amend_non_existent_order) {
    OrderBook book;

    bool result = book.amend_order(999, 100.0, 10);
    ASSERT(!result, "Amend should fail for non-existent order");
}

TEST(test_snapshot_ordering) {
    OrderBook book;

    // Add bids at different prices
    book.add_order(Order(1, true, 100.0, 10, 1000));
    book.add_order(Order(2, true, 102.0, 20, 2000));
    book.add_order(Order(3, true, 101.0, 30, 3000));

    // Add asks at different prices
    book.add_order(Order(4, false, 103.0, 40, 4000));
    book.add_order(Order(5, false, 105.0, 50, 5000));
    book.add_order(Order(6, false, 104.0, 60, 6000));

    vector<PriceLevel> bids, asks;
    book.get_snapshot(5, bids, asks);

    // Bids should be in descending order (highest first)
    ASSERT(bids.size() == 3, "Should have 3 bid levels");
    ASSERT(bids[0].price == 102.0, "Best bid should be 102.0");
    ASSERT(bids[1].price == 101.0, "Second bid should be 101.0");
    ASSERT(bids[2].price == 100.0, "Third bid should be 100.0");

    // Asks should be in ascending order (lowest first)
    ASSERT(asks.size() == 3, "Should have 3 ask levels");
    ASSERT(asks[0].price == 103.0, "Best ask should be 103.0");
    ASSERT(asks[1].price == 104.0, "Second ask should be 104.0");
    ASSERT(asks[2].price == 105.0, "Third ask should be 105.0");
}

TEST(test_snapshot_depth_limit) {
    OrderBook book;

    // Add 10 bid levels
    for (int i = 0; i < 10; ++i) {
        book.add_order(Order(i, true, 100.0 + i, 10, 1000 + i));
    }

    vector<PriceLevel> bids, asks;
    book.get_snapshot(5, bids, asks);

    ASSERT(bids.size() == 5, "Should only return top 5 levels");
}

TEST(test_empty_book) {
    OrderBook book;

    vector<PriceLevel> bids, asks;
    book.get_snapshot(5, bids, asks);

    ASSERT(bids.empty(), "Bids should be empty");
    ASSERT(asks.empty(), "Asks should be empty");
}

TEST(test_fifo_ordering) {
    OrderBook book;

    // Add orders at same price with different timestamps
    book.add_order(Order(1, true, 100.0, 10, 1000));
    book.add_order(Order(2, true, 100.0, 20, 2000));
    book.add_order(Order(3, true, 100.0, 30, 3000));

    // Cancel the first order (FIFO)
    book.cancel_order(1);

    // Remaining quantity should be 50
    vector<PriceLevel> bids, asks;
    book.get_snapshot(5, bids, asks);
    ASSERT(bids[0].total_quantity == 50, "Remaining quantity should be 50");

    // Cancel middle order
    book.cancel_order(2);
    book.get_snapshot(5, bids, asks);
    ASSERT(bids[0].total_quantity == 30, "Remaining quantity should be 30");
}

// ============================================================================
// Performance Benchmarks
// ============================================================================

struct BenchmarkResult {
    string name;
    double avg_ns;
    double median_ns;
    double min_ns;
    double max_ns;
    double p95_ns;
    double p99_ns;

    void print() const {
        cout << "\n  " << name << ":\n";
        cout << "    Avg:    " << fixed << setprecision(2) << avg_ns << " ns (" << avg_ns / 1000.0 << " us)\n";
        cout << "    Median: " << median_ns << " ns (" << median_ns / 1000.0 << " us)\n";
        cout << "    Min:    " << min_ns << " ns (" << min_ns / 1000.0 << " us)\n";
        cout << "    Max:    " << max_ns << " ns (" << max_ns / 1000.0 << " us)\n";
        cout << "    P95:    " << p95_ns << " ns (" << p95_ns / 1000.0 << " us)\n";
        cout << "    P99:    " << p99_ns << " ns (" << p99_ns / 1000.0 << " us)\n";
    }
};

BenchmarkResult calculate_stats(vector<int64_t>& timings, const string& name) {
    sort(timings.begin(), timings.end());

    BenchmarkResult result;
    result.name = name;

    double sum = accumulate(timings.begin(), timings.end(), 0.0);
    result.avg_ns = sum / timings.size();
    result.median_ns = timings[timings.size() / 2];
    result.min_ns = timings.front();
    result.max_ns = timings.back();
    result.p95_ns = timings[static_cast<size_t>(timings.size() * 0.95)];
    result.p99_ns = timings[static_cast<size_t>(timings.size() * 0.99)];

    return result;
}

void benchmark_add_order() {
    const int NUM_ITERATIONS = 100000;
    vector<int64_t> timings;
    timings.reserve(NUM_ITERATIONS);

    OrderBook book;
    Timer timer;

    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        double price = 100.0 + (i % 100) * 0.01;
        timer.reset();
        book.add_order(Order(i, i % 2 == 0, price, 100, i));
        timings.push_back(timer.elapsed_ns());
    }

    auto result = calculate_stats(timings, "Add Order");
    result.print();
}

void benchmark_cancel_order() {
    const int NUM_ITERATIONS = 100000;
    OrderBook book;

    // Pre-populate the book
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        double price = 100.0 + (i % 100) * 0.01;
        book.add_order(Order(i, i % 2 == 0, price, 100, i));
    }

    vector<int64_t> timings;
    timings.reserve(NUM_ITERATIONS);

    Timer timer;
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        timer.reset();
        book.cancel_order(i);
        timings.push_back(timer.elapsed_ns());
    }

    auto result = calculate_stats(timings, "Cancel Order");
    result.print();
}

void benchmark_amend_order_quantity() {
    const int NUM_ITERATIONS = 10000;
    OrderBook book;

    // Pre-populate the book
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        double price = 100.0 + (i % 100) * 0.01;
        book.add_order(Order(i, true, price, 100, i));
    }

    vector<int64_t> timings;
    timings.reserve(NUM_ITERATIONS);

    Timer timer;
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        double price = 100.0 + (i % 100) * 0.01;
        timer.reset();
        book.amend_order(i, price, 200);
        timings.push_back(timer.elapsed_ns());
    }

    auto result = calculate_stats(timings, "Amend Order (Quantity)");
    result.print();
}

void benchmark_amend_order_price() {
    const int NUM_ITERATIONS = 10000;
    OrderBook book;

    // Pre-populate the book
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        double price = 100.0 + (i % 100) * 0.01;
        book.add_order(Order(i, true, price, 100, i));
    }

    vector<int64_t> timings;
    timings.reserve(NUM_ITERATIONS);

    Timer timer;
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        double old_price = 100.0 + (i % 100) * 0.01;
        double new_price = old_price + 0.01;
        timer.reset();
        book.amend_order(i, new_price, 100);
        timings.push_back(timer.elapsed_ns());
    }

    auto result = calculate_stats(timings, "Amend Order (Price)");
    result.print();
}

void benchmark_get_snapshot() {
    const int NUM_ORDERS = 10000;
    const int NUM_ITERATIONS = 100000;

    OrderBook book;

    // Pre-populate the book
    for (int i = 0; i < NUM_ORDERS; ++i) {
        double price = 100.0 + (i % 1000) * 0.01;
        book.add_order(Order(i, i % 2 == 0, price, 100, i));
    }

    vector<int64_t> timings;
    timings.reserve(NUM_ITERATIONS);

    vector<PriceLevel> bids, asks;
    Timer timer;

    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        timer.reset();
        book.get_snapshot(10, bids, asks);
        timings.push_back(timer.elapsed_ns());
    }

    auto result = calculate_stats(timings, "Get Snapshot (depth=10)");
    result.print();
}

void stress_test_large_book() {
    cout << "\n" << string(70, '=') << "\n";
    cout << "STRESS TEST: Large Order Book\n";
    cout << string(70, '=') << "\n";

    const int NUM_ORDERS = 100000;
    OrderBook book;

    Timer timer;

    // Add orders
    timer.reset();
    for (int i = 0; i < NUM_ORDERS; ++i) {
        double price = 100.0 + (i % 1000) * 0.01;
        book.add_order(Order(i, i % 2 == 0, price, 100 + i % 100, i));
    }
    double add_time_ms = timer.elapsed_ms();

    cout << "\nAdded " << NUM_ORDERS << " orders in " << fixed << setprecision(2)
              << add_time_ms << " ms\n";
    cout << "Average: " << (add_time_ms * 1000000.0 / NUM_ORDERS) << " ns per order\n";

    cout << "\nBook statistics:\n";
    cout << "  Total orders: " << book.get_total_orders() << "\n";
    cout << "  Bid levels: " << book.get_bid_levels() << "\n";
    cout << "  Ask levels: " << book.get_ask_levels() << "\n";

    // Cancel half the orders
    timer.reset();
    for (int i = 0; i < NUM_ORDERS / 2; ++i) {
        book.cancel_order(i * 2);
    }
    double cancel_time_ms = timer.elapsed_ms();

    cout << "\nCanceled " << (NUM_ORDERS / 2) << " orders in " << cancel_time_ms << " ms\n";
    cout << "Average: " << (cancel_time_ms * 1000000.0 / (NUM_ORDERS / 2)) << " ns per cancel\n";

    cout << "\nBook statistics after cancels:\n";
    cout << "  Total orders: " << book.get_total_orders() << "\n";
    cout << "  Bid levels: " << book.get_bid_levels() << "\n";
    cout << "  Ask levels: " << book.get_ask_levels() << "\n";
}

// ============================================================================
// Main
// ============================================================================

int main() {
    cout << "\n" << string(70, '=') << "\n";
    cout << "LIMIT ORDER BOOK - UNIT TESTS\n";
    cout << string(70, '=') << "\n\n";

    // Unit tests run automatically via static initialization

    cout << "\n" << string(70, '=') << "\n";
    cout << "TEST SUMMARY: " << test_passed << "/" << test_count << " tests passed\n";
    cout << string(70, '=') << "\n";

    if (test_passed != test_count) {
        cout << "\nSome tests failed!\n";
        return 1;
    }

    cout << "\n" << string(70, '=') << "\n";
    cout << "PERFORMANCE BENCHMARKS\n";
    cout << string(70, '=') << "\n";

    benchmark_add_order();
    benchmark_cancel_order();
    benchmark_amend_order_quantity();
    benchmark_amend_order_price();
    benchmark_get_snapshot();

    stress_test_large_book();

    // Demonstrate the order book
    cout << "\n" << string(70, '=') << "\n";
    cout << "DEMONSTRATION\n";
    cout << string(70, '=') << "\n";

    OrderBook demo_book;

    // Add some orders
    demo_book.add_order(Order(1, true, 99.50, 100, 1000));
    demo_book.add_order(Order(2, true, 99.45, 200, 2000));
    demo_book.add_order(Order(3, true, 99.40, 150, 3000));
    demo_book.add_order(Order(4, true, 99.50, 50, 4000));

    demo_book.add_order(Order(5, false, 100.00, 100, 5000));
    demo_book.add_order(Order(6, false, 100.05, 200, 6000));
    demo_book.add_order(Order(7, false, 100.10, 150, 7000));
    demo_book.add_order(Order(8, false, 100.00, 75, 8000));

    demo_book.print_book(5);

    cout << "\nAll tests and benchmarks completed successfully!\n\n";

    return 0;
}
