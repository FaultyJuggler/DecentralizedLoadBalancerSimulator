/**
 * @file NetworkManager.h
 * @brief Simulated network layer for inter-node communication
 *
 * DESIGN RATIONALE:
 * - Abstracts network communication from application logic (separation of concerns)
 * - Currently implements simulated in-memory message passing
 * - Designed to be replaceable with real TCP sockets with minimal code changes
 * - Provides both unicast (point-to-point) and broadcast messaging
 *
 * ACADEMIC CONTEXT:
 * - Message-passing abstraction is fundamental to distributed systems
 * - Similar to MPI (Message Passing Interface) in HPC
 * - Similar to ZeroMQ, nanomsg in industry
 * - Hides network details from application layer (OSI model)
 *
 * SIMULATION vs. REALITY:
 * CURRENT (Simulation):
 * - In-memory message queues (no actual network I/O)
 * - Instant delivery (no latency)
 * - Perfect reliability (no packet loss)
 * - Unbounded bandwidth
 *
 * FUTURE (Real Network):
 * - TCP sockets for actual inter-process communication
 * - Realistic latency (milliseconds)
 * - Handle connection failures, retransmissions
 * - Network congestion and bandwidth limits
 *
 * WHY START WITH SIMULATION?
 * - Focus on algorithm correctness first
 * - Easier to debug (deterministic, reproducible)
 * - No need for multiple processes or machines
 * - Can add network simulation later (e.g., add artificial delays)
 *
 * EXTENSIBILITY:
 * To convert to real networking:
 * 1. Replace nodes_ map with socket connections
 * 2. Implement message serialization (e.g., Protocol Buffers)
 * 3. Add error handling (connection drops, timeouts)
 * 4. Handle asynchronous I/O (non-blocking sockets)
 */

#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <map>
#include <mutex>
#include <memory>
#include "Message.h"

// Forward declaration to break circular dependency
class PeerNode;

/**
 * @class NetworkManager
 * @brief Central hub for routing messages between peer nodes
 *
 * DESIGN PATTERN:
 * - Mediator pattern: Nodes don't talk directly, they go through NetworkManager
 * - Benefit: Centralized place to add logging, filtering, rate limiting
 * - Benefit: Easy to mock/stub for testing
 * - Drawback: Single point of coordination (but not failure in simulation)
 *
 * THREADING:
 * - Thread-safe: Mutex protects nodes_ map
 * - Multiple nodes can send messages concurrently
 * - Message delivery is synchronous (sendMessage blocks until delivered)
 *
 * REAL-WORLD ANALOGUE:
 * - Like a network switch/router in physical networks
 * - Like RabbitMQ/Kafka in message queue systems
 * - Like Redis Pub/Sub for broadcast messages
 */
class NetworkManager {
public:
    /**
     * @brief Constructor - initializes empty network
     *
     * No nodes are registered initially. Nodes must call registerNode()
     * before they can send or receive messages.
     */
    NetworkManager();

    /**
     * @brief Destructor - cleans up resources
     *
     * NOTE: NetworkManager does NOT own the PeerNode pointers.
     * Nodes are owned by main() and NetworkManager just holds references.
     * This follows the observer pattern.
     */
    ~NetworkManager();

    /**
     * @brief Registers a node with the network
     * @param node_id Unique identifier for the node
     * @param node Pointer to the PeerNode (not owned)
     *
     * OWNERSHIP:
     * - NetworkManager does not take ownership of the node
     * - Node must outlive any messages sent to it
     * - In production: Use weak_ptr to handle node failures
     *
     * USAGE: Called during initialization phase before simulation starts
     *
     * IDEMPOTENCY: Registering same ID twice overwrites previous registration
     * (could add assertion to catch bugs)
     */
    void registerNode(int node_id, PeerNode* node);

    /**
     * @brief Sends a message from one node to another (unicast)
     * @param message The message to send (contains sender_id and receiver_id)
     *
     * ROUTING:
     * - Extracts receiver_id from message
     * - Looks up receiver in nodes_ map
     * - Calls receiver->handleMessage(message)
     *
     * ERROR HANDLING:
     * - If receiver not found: Log error, drop message
     * - In production: Could return error code or throw exception
     *
     * SYNCHRONOUS DELIVERY:
     * - Blocks until message is delivered (added to receiver's queue)
     * - No store-and-forward (unlike real network routers)
     * - For async: Would need background thread + pending message queue
     *
     * ATOMICITY:
     * - Message delivery is atomic (either delivered or not, no partial)
     * - No message duplication or reordering (in simulation)
     */
    void sendMessage(const Message& message);

    /**
     * @brief Broadcasts a message to all nodes except sender (one-to-many)
     * @param sender_id ID of the node sending the broadcast
     * @param message The message to broadcast (receiver_id is ignored)
     *
     * ALGORITHM:
     * 1. Acquire lock on nodes_ map
     * 2. Copy all node pointers except sender
     * 3. Release lock
     * 4. Deliver to each node (outside critical section)
     *
     * COMPLEXITY: O(n) where n = number of nodes
     *
     * BROADCASTING SEMANTICS:
     * - "Best effort" delivery to all peers
     * - In simulation: All succeed or all fail
     * - In reality: Some deliveries might fail (partial failure)
     *
     * USED FOR: Gossip protocol (LOAD_UPDATE messages)
     *
     * OPTIMIZATION:
     * - For large networks: Use multicast IP or pub/sub system
     * - For epidemic protocols: Random k-subset instead of all nodes
     */
    void broadcastMessage(int sender_id, const Message& message);

    /**
     * @brief Gets list of all registered node IDs
     * @return Vector of node IDs
     *
     * THREAD SAFETY: Returns copy of IDs (not references to internal map)
     *
     * USAGE:
     * - Topology discovery: Nodes can query who else is in the network
     * - Testing: Verify all nodes were registered
     * - Visualization: Generate network topology graphs
     *
     * IN REAL SYSTEM:
     * - Would implement proper service discovery (Consul, etcd, ZooKeeper)
     * - Would handle dynamic membership (nodes joining/leaving)
     */
    std::vector<int> getAllNodeIds() const;

private:
    /**
     * MEMBER VARIABLES: Network state and synchronization
     */

    /// Map from node ID to PeerNode pointer (registry of all nodes)
    /// We use pointers (not owned) to allow nodes to be managed externally
    /// In production: Consider weak_ptr for graceful node removal
    std::map<int, PeerNode*> nodes_;

    /// Protects concurrent access to nodes_ map
    /// CRITICAL SECTIONS:
    /// - registerNode: Writes to map
    /// - sendMessage: Reads from map
    /// - broadcastMessage: Reads from map
    /// - getAllNodeIds: Reads from map
    ///
    /// READ-WRITE LOCK OPPORTUNITY:
    /// - Many concurrent reads (message delivery)
    /// - Rare writes (node registration)
    /// - Could use std::shared_mutex (C++17) for better performance
    mutable std::mutex nodes_mutex_;

    /**
     * DESIGN NOTES:
     *
     * Why not have nodes send messages directly to each other?
     * - Centralized logging of all messages
     * - Can add message filtering/routing policies
     * - Can simulate network conditions (latency, packet loss)
     * - Can collect network-level statistics
     *
     * Why store raw pointers instead of shared_ptr?
     * - Nodes are owned by main(), NetworkManager just observes
     * - Avoids circular ownership (Node -> NetworkManager -> Node)
     * - In production: Use weak_ptr to detect dead nodes
     *
     * Thread safety guarantees:
     * - Safe to call any method from any thread
     * - Message delivery is atomic (no torn reads/writes)
     * - Order is not guaranteed between concurrent senders
     *   (could add sequence numbers if ordering needed)
     */
};

#endif // NETWORKMANAGER_H
