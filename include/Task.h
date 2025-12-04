/**
 * @file Task.h
 * @brief Represents a unit of work in the distributed load balancing system
 *
 * DESIGN RATIONALE:
 * - Tasks are the fundamental unit of work being balanced across nodes
 * - Each task has a unique ID for tracking and debugging purposes
 * - Complexity represents processing time, allowing simulation of heterogeneous workloads
 * - Creation timestamp enables latency analysis (queuing time vs. processing time)
 *
 * ACADEMIC CONTEXT:
 * - Similar to "jobs" in scheduling literature (e.g., FCFS, SJF algorithms)
 * - Complexity mimics CPU burst time in OS scheduling
 * - In real systems, this would represent: ML inference requests, database queries,
 *   rendering jobs, or computational tasks in a distributed computing framework
 *
 * CONNECTION TO MoE RESEARCH:
 * - Tasks could be extended to include "type" for expert specialization
 * - Complexity could model different expert processing capabilities
 * - Timestamp data useful for analyzing routing latency vs. processing latency
 */

#ifndef TASK_H
#define TASK_H

#include <string>
#include <chrono>

/**
 * @class Task
 * @brief Encapsulates a single unit of work to be processed by a PeerNode
 *
 * This class is intentionally lightweight to minimize overhead in the
 * message-passing system. In a real distributed system, tasks might carry
 * serialized payloads, input data, or function pointers.
 */
class Task {
public:
    /**
     * @brief Constructs a new Task with specified ID and complexity
     * @param id Unique identifier for this task (for tracking and logging)
     * @param complexity Processing time in milliseconds (simulates heterogeneous work)
     *
     * NOTE: In production systems, task IDs might be UUIDs or globally unique
     * identifiers to handle distributed task creation. Here, we use simple ints
     * since task generation is centralized in the simulation.
     */
    Task(int id, int complexity);

    /**
     * @brief Gets the unique identifier for this task
     * @return Task ID
     *
     * Used for logging, debugging, and ensuring tasks are processed exactly once
     * (at-most-once or exactly-once semantics depending on failure model).
     */
    int getId() const;

    /**
     * @brief Gets the processing complexity (time) for this task
     * @return Complexity in milliseconds
     *
     * Allows simulation of heterogeneous workloads where some tasks take
     * longer than others. This is critical for realistic load balancing
     * evaluation.
     */
    int getComplexity() const;

    /**
     * @brief Gets the creation timestamp for latency analysis
     * @return Steady clock timepoint when task was created
     *
     * PERFORMANCE NOTE: We use steady_clock (monotonic) rather than system_clock
     * because it's not affected by system time adjustments, making it suitable
     * for measuring durations.
     */
    std::chrono::steady_clock::time_point getCreationTime() const;

    /**
     * @brief Simulates task execution by sleeping for the complexity duration
     *
     * SIMULATION APPROACH:
     * - Uses std::this_thread::sleep_for to simulate CPU-bound work
     * - In real systems, this would be actual computation (ML inference, etc.)
     * - Sleep-based simulation is common in distributed systems research
     *   (see CloudSim, SimGrid frameworks)
     *
     * LIMITATION: This doesn't model I/O wait, memory access patterns, or
     * resource contention. Future work could use more sophisticated workload
     * models (e.g., exponential distribution for service times).
     */
    void execute();

private:
    int id_;                  ///< Unique task identifier
    int complexity_;          ///< Processing time in milliseconds

    /// Creation timestamp for measuring queuing delay and end-to-end latency
    /// This is crucial for evaluating Quality of Service (QoS) metrics
    std::chrono::steady_clock::time_point creation_time_;
};

#endif // TASK_H
