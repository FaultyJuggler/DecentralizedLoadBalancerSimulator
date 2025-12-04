/**
 * @file PeerNode.h
 * @brief Core autonomous node in the decentralized load balancing system
 *
 * SYSTEM OVERVIEW:
 * This is the heart of the distributed system. Each PeerNode operates autonomously,
 * making local decisions based on partial global information (gossip protocol).
 * The emergent behavior of all nodes results in system-wide load balancing.
 *
 * ACADEMIC CONTEXT - Distributed Systems:
 * - Implements concepts from: Chord DHT, SWIM membership, work-stealing schedulers
 * - No centralized coordinator (unlike traditional load balancers like HAProxy/Nginx)
 * - Each node is symmetric (peer-to-peer) not hierarchical (client-server)
 * - Eventually consistent state (CAP theorem: AP system, sacrifices Consistency)
 *
 * ACADEMIC CONTEXT - Operating Systems:
 * - Similar to Linux CFS (Completely Fair Scheduler) but distributed
 * - Worker threads model CPU cores, task queue models run queue
 * - Load balancing algorithm similar to push migration in SMP schedulers
 *
 * DESIGN PHILOSOPHY:
 * "Think globally, act locally" - Each node optimizes locally (greedy algorithm)
 * but the aggregate behavior produces global load distribution. This is inspired
 * by swarm intelligence and stigmergy (ants, bees).
 *
 * CONNECTION TO MoE RESEARCH:
 * - PeerNode ≈ Expert model in Mixture of Experts
 * - Task routing ≈ Gating mechanism
 * - Load balancing ≈ Expert utilization optimization
 * - Decentralized coordination avoids bottleneck of centralized gating
 *
 * THREADING MODEL:
 * Each node runs 4 concurrent threads:
 * 1. Worker Thread 1: Processes tasks from queue (CPU-bound work simulation)
 * 2. Worker Thread 2: Processes tasks from queue (parallel processing)
 * 3. Load Monitor: Gossip protocol (broadcast load every 500ms) + offloading logic
 * 4. Message Processor: Handles incoming messages from peers (event-driven)
 *
 * SYNCHRONIZATION:
 * - Task queue: Mutex + condition variable (producer-consumer pattern)
 * - Peer load map: Mutex (read-write lock could be more efficient)
 * - Message queue: Mutex + condition variable
 * - Task counter: Atomic (lock-free for performance)
 */

#ifndef PEERNODE_H
#define PEERNODE_H

#include <queue>
#include <map>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <memory>
#include "Task.h"
#include "Message.h"

// Forward declaration to break circular dependency
// (PeerNode uses NetworkManager, NetworkManager uses PeerNode)
class NetworkManager;

/**
 * @class PeerNode
 * @brief Autonomous worker node with local task queue and peer coordination
 *
 * INVARIANTS:
 * - Task queue size always equals current_load
 * - tasks_processed is monotonically increasing
 * - Each task processed exactly once (no duplicates)
 * - Node ID is immutable after construction
 *
 * LIFECYCLE:
 * 1. Construction: Initialize data structures, no threads running
 * 2. start(): Spawn worker threads, begin processing
 * 3. [Runtime]: Process tasks, exchange messages, balance load
 * 4. stop(): Signal threads to terminate, join all threads
 * 5. Destruction: Clean up resources
 */
class PeerNode {
public:
    /**
     * @brief Constructs a new PeerNode with specified configuration
     * @param id Unique identifier for this node (0 to N-1)
     * @param load_threshold Queue size triggering task offloading
     * @param network_manager Pointer to shared network layer (not owned)
     *
     * DESIGN DECISION: Why pass network_manager pointer?
     * - Dependency injection: Node doesn't own network, just uses it
     * - Allows testing with mock NetworkManager
     * - Alternative: Singleton NetworkManager, but harder to test
     *
     * LOAD THRESHOLD:
     * - Controls aggressiveness of load balancing
     * - Low threshold (e.g., 5): Aggressive offloading, more messages
     * - High threshold (e.g., 20): Tolerate imbalance, fewer messages
     * - Optimal value depends on task complexity and network latency
     * - Could be made adaptive (increase if offloading fails repeatedly)
     */
    PeerNode(int id, int load_threshold, NetworkManager* network_manager);

    /**
     * @brief Destructor - ensures clean shutdown
     *
     * RAII (Resource Acquisition Is Initialization):
     * Destructor calls stop() to join all threads before destruction.
     * This prevents dangling thread references and ensures clean shutdown.
     */
    ~PeerNode();

    /**
     * @brief Starts all worker threads (node begins processing)
     *
     * THREAD CREATION:
     * - 2 worker threads for parallel task processing
     * - 1 load monitor thread for gossip + offloading
     * - 1 message processor thread for handling incoming messages
     *
     * IDEMPOTENCY: Safe to call multiple times (checks running_ flag)
     *
     * WHY 2 WORKERS?
     * - Models multi-core CPU (can process 2 tasks concurrently)
     * - More workers = higher throughput but more context switches
     * - Production systems: #workers ≈ #CPU cores
     */
    void start();

    /**
     * @brief Stops all worker threads (graceful shutdown)
     *
     * SHUTDOWN PROTOCOL:
     * 1. Set running_ = false (signals all threads to exit)
     * 2. Notify all condition variables (wake sleeping threads)
     * 3. Join all threads (wait for completion)
     * 4. Outstanding tasks remain in queue (could be persisted)
     *
     * GRACEFUL vs. ABRUPT:
     * - Graceful: Let current tasks finish (what we do)
     * - Abrupt: pthread_cancel (dangerous, leaves inconsistent state)
     */
    void stop();

    /**
     * @brief Adds a new task to this node's queue
     * @param task Shared pointer to task to be processed
     *
     * THREAD SAFETY: Queue is protected by mutex
     * SIGNALING: Wakes up one worker thread (condition variable)
     *
     * USED BY:
     * - Main thread: Initial task assignment
     * - Peer nodes: Task offloading (TASK_TRANSFER messages)
     *
     * ENQUEUEING POLICY: FIFO (First-In-First-Out)
     * - Could be extended to priority queue for different task types
     * - Could be extended to LIFO (Last-In-First-Out) for better cache locality
     */
    void addTask(std::shared_ptr<Task> task);

    /**
     * @brief Returns current queue size (load)
     * @return Number of tasks waiting in queue
     *
     * THREAD SAFETY: Acquires mutex to read queue size
     * EVENTUAL CONSISTENCY: Value may be stale immediately after return
     *
     * USED FOR:
     * - Gossip protocol (broadcast to peers)
     * - Threshold check (should we offload?)
     * - Performance metrics (logging)
     */
    int getCurrentLoad() const;

    /**
     * @brief Returns total tasks completed by this node
     * @return Cumulative task count
     *
     * LOCK-FREE: Uses atomic for high-performance concurrent access
     * MONOTONIC: Always increases (useful for debugging invariants)
     *
     * FAIRNESS METRIC:
     * At steady state, all nodes should have similar tasks_processed
     * values. Large variance indicates load imbalance.
     */
    int getTasksProcessed() const;

    /**
     * @brief Handles an incoming message from a peer
     * @param message The message to process
     *
     * ASYNC HANDLING:
     * - Does not block caller (adds to internal message queue)
     * - Actual processing happens in messageProcessorLoop thread
     * - Decouples message arrival from processing
     *
     * CALLED BY: NetworkManager when message is delivered
     *
     * MESSAGE TYPES:
     * - LOAD_UPDATE: Update peer_loads_ map
     * - TASK_TRANSFER: Add task to local queue
     * - PEER_DISCOVERY: Add to peers_ list (future)
     */
    void handleMessage(const Message& message);

    /**
     * @brief Registers a peer node (topology management)
     * @param peer_id ID of the peer to add
     *
     * TOPOLOGY: Currently builds fully connected mesh (O(n²) links)
     * FUTURE: Could implement random graph, ring, or hierarchical topology
     *
     * DUPLICATE HANDLING: Checks if peer already exists before adding
     * IDEMPOTENCY: Safe to call multiple times with same peer_id
     */
    void addPeer(int peer_id);

    /**
     * @brief Gets list of all known peers
     * @return Vector of peer IDs
     *
     * THREAD SAFETY: Returns copy (not reference) to avoid concurrent modification
     * USED FOR: Testing, debugging, topology visualization
     */
    std::vector<int> getPeers() const;

    /**
     * @brief Gets this node's unique ID
     * @return Node ID
     *
     * IMMUTABLE: ID never changes after construction
     * NO LOCKING: Safe to call from any thread (read-only)
     */
    int getId() const;

private:
    /**
     * PRIVATE METHODS: Thread entry points and internal logic
     * These methods run in separate threads and implement the core algorithms.
     */

    /**
     * @brief Worker thread: Continuously processes tasks from queue
     *
     * ALGORITHM:
     * 1. Wait for task (condition variable - no busy waiting)
     * 2. Dequeue task
     * 3. Execute task (sleep to simulate work)
     * 4. Increment tasks_processed_
     * 5. Repeat until running_ = false
     *
     * TERMINATION:
     * - Exits when running_ = false AND queue is empty
     * - Ensures all tasks are processed before shutdown
     *
     * MULTIPLE WORKERS:
     * - Multiple threads run this same method
     * - Mutex ensures only one thread dequeues at a time
     * - Condition variable prevents thundering herd (only one wakes per notify)
     */
    void workerLoop();

    /**
     * @brief Load monitor thread: Implements gossip protocol and offloading
     *
     * ALGORITHM (every 500ms):
     * 1. Get current load (queue size)
     * 2. Broadcast LOAD_UPDATE to all peers (gossip)
     * 3. Log metrics (for performance analysis)
     * 4. If load > threshold: Offload one task to least-loaded peer
     * 5. Sleep 500ms, repeat
     *
     * GOSSIP PROTOCOL:
     * - All-to-all broadcast every 500ms
     * - O(n²) messages per round (n = number of nodes)
     * - Eventually consistent: All nodes converge to similar load view
     * - Trade-off: Frequent updates = better decisions but higher overhead
     *
     * WHY 500ms?
     * - Fast enough to react to load changes
     * - Slow enough to avoid message storms
     * - Could be tuned based on task complexity (fast tasks = faster gossip)
     */
    void loadMonitorLoop();

    /**
     * @brief Message processor thread: Handles incoming messages
     *
     * ALGORITHM:
     * 1. Wait for message (condition variable)
     * 2. Dequeue message
     * 3. Switch on message type:
     *    - LOAD_UPDATE: Update peer_loads_[sender] = load
     *    - TASK_TRANSFER: Add task to queue, signal workers
     *    - PEER_DISCOVERY: Add to peers_ list
     * 4. Repeat until running_ = false
     *
     * EVENT-DRIVEN:
     * - Reacts to incoming messages (not polling)
     * - Decouples message arrival from processing
     * - Similar to event loop in Node.js or reactor pattern
     */
    void messageProcessorLoop();

    /**
     * @brief Offloads a task to a less-loaded peer
     * @param task Task to offload
     *
     * ALGORITHM:
     * 1. Select best peer (least loaded, excluding self)
     * 2. If suitable peer found: Send TASK_TRANSFER message
     * 3. Else: Re-add task to own queue (no peer available)
     *
     * SELECTION POLICY: Greedy (pick minimum load peer)
     * ALTERNATIVE POLICIES:
     * - Random: Simple, avoids hotspots
     * - Weighted: Consider peer capabilities
     * - Round-robin: Ensure fairness
     *
     * STALE INFORMATION:
     * - Routing decision based on peer_loads_ map
     * - Map updated by gossip (may be 100-500ms stale)
     * - Could cause suboptimal routing but system still converges
     */
    void offloadTask(std::shared_ptr<Task> task);

    /**
     * @brief Selects the least-loaded peer for task routing
     * @return Peer ID, or -1 if no suitable peer
     *
     * GREEDY ALGORITHM:
     * - Iterate peer_loads_ map
     * - Track minimum load and corresponding peer ID
     * - Only consider peers with load < my_load (don't make things worse)
     *
     * COMPLEXITY: O(n) where n = number of peers
     * COULD OPTIMIZE: Maintain sorted data structure, but O(n) is fine for small n
     *
     * EMPTY PEER MAP:
     * - Returns -1 if no peers registered yet
     * - Returns -1 if all peers more loaded than self
     */
    int selectBestPeer();

    /**
     * MEMBER VARIABLES: Node state and synchronization primitives
     * Organized by purpose for clarity.
     */

    // Identity and configuration
    int id_;                              ///< Unique node identifier (immutable)
    int load_threshold_;                  ///< Queue size triggering offloading

    // Performance metrics
    std::atomic<int> tasks_processed_;    ///< Total tasks completed (lock-free)

    // Task queue (producer-consumer pattern)
    std::queue<std::shared_ptr<Task>> task_queue_;  ///< FIFO task queue
    mutable std::mutex queue_mutex_;                ///< Protects task_queue_
    std::condition_variable queue_cv_;              ///< Signals new task arrival

    // Peer load tracking (gossip protocol state)
    std::map<int, int> peer_loads_;       ///< Map: peer_id -> queue_size
    mutable std::mutex peer_loads_mutex_; ///< Protects peer_loads_

    // Topology information
    std::vector<int> peers_;              ///< List of known peer IDs
    mutable std::mutex peers_mutex_;      ///< Protects peers_

    // Message queue (event-driven processing)
    std::queue<Message> message_queue_;   ///< Incoming message queue
    mutable std::mutex message_mutex_;    ///< Protects message_queue_
    std::condition_variable message_cv_;  ///< Signals new message arrival

    // Thread management
    std::vector<std::thread> worker_threads_;  ///< Task processing threads
    std::thread load_monitor_thread_;          ///< Gossip + offloading thread
    std::thread message_processor_thread_;     ///< Message handling thread

    // Control flag
    std::atomic<bool> running_;           ///< Signals threads to continue/stop

    // External dependencies
    NetworkManager* network_manager_;     ///< Network layer (not owned)

    /**
     * SYNCHRONIZATION DESIGN NOTES:
     *
     * Why so many mutexes?
     * - Each data structure has independent access patterns
     * - Fine-grained locking reduces contention
     * - Alternative: One big lock (simpler but less concurrent)
     *
     * Why condition variables?
     * - Efficient thread wake-up (no busy-waiting/polling)
     * - Alternative: Sleep polling (wastes CPU, slower to react)
     *
     * Why atomics for tasks_processed_?
     * - Lock-free increment (very hot path)
     * - No need for mutex (only incremented, never decremented)
     *
     * Deadlock prevention:
     * - Lock ordering: Always acquire in same order if multiple locks needed
     * - Currently: Each method locks only one mutex (no deadlock possible)
     * - If adding cross-lock code: Document lock order carefully
     */
};

#endif // PEERNODE_H
