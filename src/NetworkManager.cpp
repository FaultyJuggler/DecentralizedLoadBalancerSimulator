#include "NetworkManager.h"
#include "PeerNode.h"
#include "Logger.h"

NetworkManager::NetworkManager() {
}

NetworkManager::~NetworkManager() {
}

void NetworkManager::registerNode(int node_id, PeerNode* node) {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    nodes_[node_id] = node;
    
    Logger::getInstance().log("NetworkManager: Registered node " + 
                              std::to_string(node_id));
}

void NetworkManager::sendMessage(const Message& message) {
    PeerNode* receiver = nullptr;
    
    {
        std::lock_guard<std::mutex> lock(nodes_mutex_);
        auto it = nodes_.find(message.getReceiverId());
        if (it != nodes_.end()) {
            receiver = it->second;
        }
    }
    
    if (receiver) {
        receiver->handleMessage(message);
        Logger::getInstance().log("NetworkManager: Sent " + message.toString());
    } else {
        Logger::getInstance().log(std::string("NetworkManager: Failed to send message - ") +
                                  "receiver " + std::to_string(message.getReceiverId()) +
                                  " not found");
    }
}

void NetworkManager::broadcastMessage(int sender_id, const Message& message) {
    std::vector<PeerNode*> receivers;
    
    {
        std::lock_guard<std::mutex> lock(nodes_mutex_);
        for (const auto& [node_id, node] : nodes_) {
            if (node_id != sender_id) {  // Don't send to self
                receivers.push_back(node);
            }
        }
    }
    
    for (PeerNode* receiver : receivers) {
        receiver->handleMessage(message);
    }
    
    if (!receivers.empty()) {
        Logger::getInstance().log("NetworkManager: Broadcast from node " + 
                                  std::to_string(sender_id) + " to " +
                                  std::to_string(receivers.size()) + " peers");
    }
}

std::vector<int> NetworkManager::getAllNodeIds() const {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    
    std::vector<int> node_ids;
    for (const auto& [node_id, _] : nodes_) {
        node_ids.push_back(node_id);
    }
    
    return node_ids;
}
