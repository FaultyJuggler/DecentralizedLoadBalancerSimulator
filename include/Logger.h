/**
 * @file Logger.h
 * @brief Thread-safe singleton logger for system-wide event and metrics tracking
 *
 * DESIGN RATIONALE:
 * - Singleton pattern ensures all components log to the same output stream
 * - Thread-safe to handle concurrent logging from multiple worker threads
 * - Timestamps enable trace analysis and performance debugging
 * - Supports both console and file output for different use cases
 *
 * ACADEMIC CONTEXT:
 * - Distributed systems require consistent logging for debugging and verification
 * - Similar to syslog in Unix or Log4j in Java
 * - Lamport's logical clocks could be added for causality tracking
 * - Essential for post-mortem debugging and correctness verification
 *
 * CONCURRENCY CHALLENGES:
 * - Multiple threads logging simultaneously can interleave output
 * - File I/O is not atomic - need explicit synchronization
 * - Mutex protects both stdout and file writes
 * - Trade-off: Synchronous logging (blocks) vs. async (lock-free queue)
 *
 * WHY SINGLETON?
 * - Centralized control over log destination (console vs. file)
 * - No need to pass logger references through every class constructor
 * - Ensures chronological ordering of events across all nodes
 * - Alternative: Dependency injection, but adds complexity for small project
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <mutex>
#include <memory>

/**
 * @class Logger
 * @brief Global thread-safe logger using the Singleton pattern
 *
 * USAGE EXAMPLE:
 *   Logger::getInstance().log("System starting up");
 *   Logger::getInstance().logNodeEvent(0, "Processing task 42");
 *
 * THREAD SAFETY:
 * - All public methods acquire mutex before I/O operations
 * - getInstance() is thread-safe in C++11+ (static initialization)
 * - No risk of deadlock since logger doesn't call into other mutexes
 *
 * PERFORMANCE:
 * - Mutex contention is the main bottleneck under heavy logging
 * - For production: Use lock-free circular buffer + background writer thread
 * - For simulation/debugging: Synchronous logging is acceptable
 */
class Logger {
public:
    /**
     * @brief Gets the singleton instance of the Logger
     * @return Reference to the single Logger instance
     *
     * THREAD SAFETY:
     * C++11 guarantees thread-safe initialization of function-local statics.
     * The instance is created on first call (lazy initialization) and
     * persists for program lifetime.
     *
     * MEYER'S SINGLETON:
     * This is the modern C++ idiom for singletons - simpler than
     * classical implementations with double-checked locking.
     */
    static Logger& getInstance();

    /**
     * @brief Logs a general message with timestamp
     * @param message The message to log
     *
     * FORMAT: [YYYY-MM-DD HH:MM:SS.mmm] message
     *
     * TIMESTAMPING:
     * - Uses system clock for human-readable timestamps
     * - Millisecond precision for fine-grained event ordering
     * - Could add logical clocks (Lamport, vector) for causal ordering
     */
    void log(const std::string& message);

    /**
     * @brief Logs a node-specific event with formatting
     * @param node_id ID of the node generating the event
     * @param event Description of the event
     *
     * FORMAT: [timestamp] Node[id] event
     * EXAMPLE: [2025-11-13 18:13:52.023] Node[2] Processing task 42
     *
     * WHY NODE-SPECIFIC?
     * - In distributed systems, identifying the originating node is critical
     * - Enables filtering logs by node for debugging specific behavior
     * - Helps visualize concurrent execution across nodes
     */
    void logNodeEvent(int node_id, const std::string& event);

    /**
     * @brief Logs performance metrics for a specific node
     * @param node_id ID of the node
     * @param current_load Current queue size
     * @param tasks_processed Total tasks completed
     *
     * FORMAT: [timestamp] Node[id] Load=X TasksProcessed=Y
     *
     * METRICS TRACKING:
     * - Called periodically (e.g., every 500ms) for time-series data
     * - Post-processing these logs can generate performance graphs
     * - Useful for evaluating fairness (variance in tasks_processed)
     * - Useful for analyzing convergence (how fast load stabilizes)
     */
    void logMetrics(int node_id, int current_load, int tasks_processed);

    /**
     * @brief Sets output destination to a file (instead of console)
     * @param filename Path to log file (will be created/appended)
     *
     * USAGE: Call once at startup to redirect all logging to file
     *   Logger::getInstance().setLogFile("logs/simulation.log");
     *
     * FILE MANAGEMENT:
     * - File is opened in append mode (preserves previous runs)
     * - Automatically closed in destructor
     * - Error handling: If open fails, falls back to console output
     */
    void setLogFile(const std::string& filename);

    // Prevent copying and assignment (singleton must have single instance)
    Logger(const Logger&) = delete;              ///< No copy constructor
    Logger& operator=(const Logger&) = delete;    ///< No copy assignment

private:
    /**
     * @brief Private constructor (enforces singleton pattern)
     *
     * Only getInstance() can create the Logger instance.
     * Initializes with console output (no file).
     */
    Logger();

    /**
     * @brief Private destructor (cleanup)
     *
     * Closes log file if open. Called automatically at program exit.
     */
    ~Logger();

    /**
     * @brief Generates a formatted timestamp string
     * @return String in format "YYYY-MM-DD HH:MM:SS.mmm"
     *
     * IMPLEMENTATION:
     * - Uses std::chrono for time manipulation
     * - Formats with std::put_time for human readability
     * - Adds milliseconds manually (not supported by strftime)
     *
     * ALTERNATIVE:
     * Could use std::format in C++20 for cleaner code
     */
    std::string getCurrentTimestamp();

    // Synchronization primitive
    std::mutex mutex_;        ///< Protects all I/O operations (console and file)

    // Output destination
    std::ofstream log_file_;  ///< File stream for logging (optional)
    bool use_file_;           ///< Flag: true = log to file, false = log to console

    /**
     * DESIGN NOTE: Why not use both console and file simultaneously?
     * - Avoids duplicated output when running interactively
     * - User can redirect stdout to file if needed (./program > log.txt)
     * - Production systems often have separate console (errors only) and
     *   file (everything) but adds complexity
     */
};

#endif // LOGGER_H
