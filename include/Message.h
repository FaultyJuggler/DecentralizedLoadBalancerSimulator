/**
 * @file Message.h
 * @brief Defines the communication protocol for peer-to-peer node interaction
 *
 * DESIGN RATIONALE:
 * - Type-safe message passing prevents protocol violations at compile-time
 * - Each message type serves a specific purpose in the distributed protocol
 * - Messages are self-contained (carry all needed data) for easy serialization
 * - Sender and receiver IDs enable point-to-point and broadcast semantics
 *
 * ACADEMIC CONTEXT:
 * - Message passing is fundamental to distributed systems (Lamport's message-passing model)
 * - Similar to MPI (Message Passing Interface) in HPC or Actor model in Erlang/Akka
 * - Type system prevents mixing different protocol layers (application vs. control)
 *
 * PROTOCOL DESIGN:
 * - LOAD_UPDATE: Gossip protocol for disseminating load information
 * - TASK_TRANSFER: Remote procedure call (RPC) for task migration
 * - TASK_REQUEST: Pull-based work stealing (not yet implemented)
 * - PEER_DISCOVERY: Membership protocol for dynamic topology (future work)
 *
 * FUTURE EXTENSIONS:
 * - Could add sequence numbers for ordering (causal or total order)
 * - Could add checksums or signatures for Byzantine fault tolerance
 * - Could add TTL (time-to-live) for gossip message propagation limits
 */

#ifndef MESSAGE_H
#define MESSAGE_H

#include <string>
#include <memory>
#include "Task.h"

/**
 * @enum MessageType
 * @brief Enumeration of all message types in the communication protocol
 *
 * Using a strongly-typed enum (enum class) prevents accidental integer conversions
 * and makes the code more maintainable. Each type represents a different layer
 * of the distributed protocol.
 */
enum class MessageType {
    LOAD_UPDATE,     ///< Broadcast: Node announces current queue length (gossip)
    TASK_REQUEST,    ///< Pull: Node requests work from a peer (future work)
    TASK_TRANSFER,   ///< Push: Node sends a task to a peer for execution
    PEER_DISCOVERY   ///< Membership: Node announces presence (future work)
};

/**
 * @class Message
 * @brief Encapsulates all inter-node communication in the distributed system
 *
 * THREAD SAFETY:
 * - Message objects are immutable after creation (const methods only)
 * - Shared pointers to Tasks enable safe cross-thread transfer without copying
 * - No internal state changes after construction = naturally thread-safe
 *
 * DESIGN PATTERN:
 * - Uses discriminated union pattern (type + data fields)
 * - Alternative designs: inheritance (MessageBase with subclasses) or
 *   protocol buffers for language-independent serialization
 *
 * PERFORMANCE CONSIDERATIONS:
 * - Uses shared_ptr for Task to avoid expensive copying
 * - Small message size (few integers + pointer) = cache-friendly
 * - Could be further optimized with custom allocators for high-frequency messages
 */
class Message {
public:
    /**
     * @brief Constructs a new Message with specified type and routing information
     * @param type The message type (determines which fields are valid)
     * @param sender_id ID of the sending node
     * @param receiver_id ID of the receiving node (-1 for broadcast)
     *
     * ROUTING SEMANTICS:
     * - receiver_id >= 0: Point-to-point message to specific node
     * - receiver_id == -1: Broadcast to all peers (NetworkManager handles fanout)
     *
     * This mirrors IP routing (unicast vs. multicast) but at application layer.
     */
    Message(MessageType type, int sender_id, int receiver_id);

    /**
     * @brief Gets the message type
     * @return MessageType enum value
     *
     * Receiver must switch on this to determine which data fields are valid.
     * This is similar to protocol parsing in network stacks.
     */
    MessageType getType() const;

    /**
     * @brief Gets the sender's node ID
     * @return Sender ID
     *
     * Used for:
     * - Updating peer load maps (who sent this LOAD_UPDATE?)
     * - Logging and debugging (trace message flow)
     * - Preventing routing loops (don't send back to sender)
     */
    int getSenderId() const;

    /**
     * @brief Gets the intended receiver's node ID
     * @return Receiver ID (-1 for broadcast)
     */
    int getReceiverId() const;

    /**
     * @brief Sets the load value for LOAD_UPDATE messages
     * @param load Current queue size of the sending node
     *
     * PROTOCOL NOTE: Only meaningful for LOAD_UPDATE messages.
     * Calling this for other message types is technically allowed but semantically incorrect.
     * Could be enforced with assertions in debug mode.
     */
    void setLoadValue(int load);

    /**
     * @brief Gets the load value from LOAD_UPDATE messages
     * @return Load value (queue size)
     *
     * USAGE: Receiver updates its local view of sender's load:
     *   peer_loads_[msg.getSenderId()] = msg.getLoadValue();
     *
     * This implements the gossip protocol's information dissemination.
     */
    int getLoadValue() const;

    /**
     * @brief Attaches a task to TASK_TRANSFER messages
     * @param task Shared pointer to the task being transferred
     *
     * DESIGN CHOICE: shared_ptr instead of unique_ptr because:
     * - Message might be logged/copied before delivery
     * - Allows safe concurrent access for monitoring/debugging
     * - Alternative: std::move with unique_ptr for strict ownership transfer
     *
     * TASK MIGRATION:
     * The task is logically "moved" from sender's queue to receiver's queue.
     * This is the core mechanism for load balancing - tasks flow from
     * high-load to low-load nodes.
     */
    void setTask(std::shared_ptr<Task> task);

    /**
     * @brief Gets the attached task from TASK_TRANSFER messages
     * @return Shared pointer to the task (nullptr if none attached)
     *
     * RECEIVER BEHAVIOR:
     * Upon receiving a TASK_TRANSFER, the receiver should:
     * 1. Extract the task with getTask()
     * 2. Add it to its local task queue
     * 3. Log the transfer event
     * 4. Signal worker threads to process new work
     */
    std::shared_ptr<Task> getTask() const;

    /**
     * @brief Generates a human-readable string representation for logging
     * @return String describing message type, sender, receiver, and relevant data
     *
     * DEBUGGING AID:
     * - Logs show message flow through the system
     * - Helps verify protocol correctness (e.g., no routing loops)
     * - Enables replay-based debugging from log files
     *
     * Example output: "Message[LOAD_UPDATE from=2 to=-1 load=5]"
     */
    std::string toString() const;

private:
    MessageType type_;                     ///< Discriminator for message contents
    int sender_id_;                        ///< Origin node ID
    int receiver_id_;                      ///< Destination node ID (-1 = broadcast)

    // Optional data fields (valid based on type_)
    int load_value_;                       ///< For LOAD_UPDATE messages
    std::shared_ptr<Task> task_;           ///< For TASK_TRANSFER messages

    /**
     * PROTOCOL INVARIANTS (enforced by convention):
     * - LOAD_UPDATE messages have valid load_value_
     * - TASK_TRANSFER messages have non-null task_
     * - TASK_REQUEST messages have neither (just sender ID is needed)
     *
     * A production system might use std::variant or inheritance to enforce
     * these invariants at compile-time.
     */
};

#endif // MESSAGE_H
