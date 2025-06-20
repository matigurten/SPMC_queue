#pragma once
#include "order_book.h"

// OrderBookManager: static utility/controller for managing one or more OrderBooks
class OrderBookManager {
public:
    // Example static utility: process an event for a given OrderBook
    static void process_event(OrderBook& book, Event* ev) { book.process_event(ev); }
    // Add more static utilities as needed (e.g., batch processing, multi-book management)
}; 