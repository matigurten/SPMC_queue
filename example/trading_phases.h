#pragma once
#include <cstdint>
#include <string>

// Trading Phase Constants
// Based on common exchange standards (e.g., NASDAQ, NYSE, CME)
namespace TradingPhase {
    // Pre-market phases
    constexpr uint16_t PRE_MARKET_OPEN = 1;      // Pre-market trading session
    constexpr uint16_t PRE_MARKET_CLOSE = 2;     // Pre-market session ending
    
    // Regular market phases
    constexpr uint16_t MARKET_OPEN = 3;          // Regular market opening
    constexpr uint16_t CONTINUOUS_TRADING = 4;   // Normal continuous trading (crossing allowed)
    constexpr uint16_t MARKET_CLOSE = 5;         // Regular market closing
    
    // Auction phases
    constexpr uint16_t OPENING_AUCTION = 6;      // Opening auction (no crossing)
    constexpr uint16_t CLOSING_AUCTION = 7;      // Closing auction (no crossing)
    constexpr uint16_t INTRADAY_AUCTION = 8;     // Intraday auction (no crossing)
    
    // Halt phases
    constexpr uint16_t TRADING_HALT = 9;         // Trading halted
    constexpr uint16_t VOLATILITY_HALT = 10;     // Volatility circuit breaker
    constexpr uint16_t NEWS_HALT = 11;           // News-related halt
    
    // After hours
    constexpr uint16_t AFTER_HOURS = 12;         // After hours trading
    
    // Special states
    constexpr uint16_t UNKNOWN = 0;              // Unknown/undefined phase
    constexpr uint16_t ERROR = 255;              // Error state
    
    // Helper function to get phase name
    inline const char* get_phase_name(uint16_t phase) {
        switch (phase) {
            case PRE_MARKET_OPEN: return "PRE_MARKET_OPEN";
            case PRE_MARKET_CLOSE: return "PRE_MARKET_CLOSE";
            case MARKET_OPEN: return "MARKET_OPEN";
            case CONTINUOUS_TRADING: return "CONTINUOUS_TRADING";
            case MARKET_CLOSE: return "MARKET_CLOSE";
            case OPENING_AUCTION: return "OPENING_AUCTION";
            case CLOSING_AUCTION: return "CLOSING_AUCTION";
            case INTRADAY_AUCTION: return "INTRADAY_AUCTION";
            case TRADING_HALT: return "TRADING_HALT";
            case VOLATILITY_HALT: return "VOLATILITY_HALT";
            case NEWS_HALT: return "NEWS_HALT";
            case AFTER_HOURS: return "AFTER_HOURS";
            case UNKNOWN: return "UNKNOWN";
            case ERROR: return "ERROR";
            default: return "INVALID_PHASE";
        }
    }
    
    // Helper function to check if crossing is allowed
    inline bool is_crossing_allowed(uint16_t phase) {
        return phase == CONTINUOUS_TRADING || phase == AFTER_HOURS;
    }
    
    // Helper function to check if trading is active
    inline bool is_trading_active(uint16_t phase) {
        return phase == CONTINUOUS_TRADING || 
               phase == PRE_MARKET_OPEN || 
               phase == AFTER_HOURS;
    }
} 