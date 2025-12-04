#include "PeerNode.h"
#include "NetworkManager.h"
#include "Logger.h"
#include <algorithm>
#include <random>

PeerNode::PeerNode(int id, int load_threshold, NetworkManager* network_manager)
    : id_(id), load_threshold_(load_threshold), tasks_processed_(0),
      running_(false), network_manager_(network_manager) {
}

PeerNode::~PeerNode() {
    stop();
}

void PeerNode::start() {
    if (running_.exchange(true)) {
        return;  // Already running
    }
    
    Logger::getInstance().logNodeEvent(id_, "Starting node");
    
    // Start worker threads (2 workers per node)
    for (int i = 0; i < 2; ++i) {
        worker_threads_.emplace_back(&PeerNode::workerLoop, this);
    }
    
    // Start load monitor thread
    load_monitor_thread_ = std::thread(&PeerNode::loadMonitorLoop, this);
    
    // Start message processor thread
    message_processor_thread_ = std::thread(&PeerNode::messageProcessorLoop, this);
}

void PeerNode::stop() {
    if (!running_.exchange(false)) {
        return;  // Already stopped
    }
    
    Logger::getInstance().logNodeEvent(id_, "Stopping node");
    
    // Wake up all waiting threads
    queue_cv_.notify_all();
    message_cv_.notify_all();
    
    // Join worker threads
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    worker_threads_.clear();
    
    // Join other threads
    if (load_monitor_thread_.joinable()) {
        load_monitor_thread_.join();
    }
    if (message_processor_thread_.joinable()) {
        message_processor_thread_.join();
    }
}

void PeerNode::addTask(std::shared_ptr<Task> task) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        task_queue_.push(task);
    }
    queue_cv_.notify_one();
    
    Logger::getInstance().logNodeEvent(id_, 
        "Added task " + std::to_string(task->getId()) + 
        " (queue size: " + std::to_string(getCurrentLoad()) + ")");
}

int PeerNode::getCurrentLoad() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return static_cast<int>(task_queue_.size());
}

int PeerNode::getTasksProcessed() const {
    return tasks_processed_.load();
}

void PeerNode::handleMessage(const Message& message) {
    {
        std::lock_guard<std::mutex> lock(message_mutex_);
        message_queue_.push(message);
    }
    message_cv_.notify_one();
}

void PeerNode::addPeer(int peer_id) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    if (std::find(peers_.begin(), peers_.end(), peer_id) == peers_.end()) {
        peers_.push_back(peer_id);
        Logger::getInstance().logNodeEvent(id_, 
            "Added peer " + std::to_string(peer_id));
    }
}

std::vector<int> PeerNode::getPeers() const {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    return peers_;
}

int PeerNode::getId() const {
    return id_;
}

// Worker thread: processes tasks from the queue
void PeerNode::workerLoop() {
    while (running_) {
        std::shared_ptr<Task> task;
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] { 
                return !task_queue_.empty() || !running_; 
            });
            
            if (!running_ && task_queue_.empty()) {
                break;
            }
            
            if (!task_queue_.empty()) {
                task = task_queue_.front();
                task_queue_.pop();
            }
        }
        
        if (task) {
            Logger::getInstance().logNodeEvent(id_, 
                "Processing task " + std::to_string(task->getId()));
            
            task->execute();
            tasks_processed_++;
            
            Logger::getInstance().logNodeEvent(id_, 
                "Completed task " + std::to_string(task->getId()) +
                " (total processed: " + std::to_string(tasks_processed_.load()) + ")");
        }
    }
}

// Load monitor thread: periodically checks load and sends updates
void PeerNode::loadMonitorLoop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        if (!running_) break;
        
        int current_load = getCurrentLoad();
        
        // Log metrics periodically
        Logger::getInstance().logMetrics(id_, current_load, tasks_processed_);
        
        // Broadcast load update to all peers
        if (network_manager_) {
            Message load_msg(MessageType::LOAD_UPDATE, id_, -1);  // -1 means broadcast
            load_msg.setLoadValue(current_load);
            network_manager_->broadcastMessage(id_, load_msg);
        }
        
        // If load exceeds threshold, try to offload a task
        if (current_load > load_threshold_) {
            std::shared_ptr<Task> task;
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                if (!task_queue_.empty()) {
                    task = task_queue_.front();
                    task_queue_.pop();
                }
            }
            
            if (task) {
                offloadTask(task);
            }
        }
    }
}

// Message processor thread: handles incoming messages
void PeerNode::messageProcessorLoop() {
    while (running_) {
        Message message(MessageType::LOAD_UPDATE, -1, -1);
        
        {
            std::unique_lock<std::mutex> lock(message_mutex_);
            message_cv_.wait(lock, [this] { 
                return !message_queue_.empty() || !running_; 
            });
            
            if (!running_ && message_queue_.empty()) {
                break;
            }
            
            if (!message_queue_.empty()) {
                message = message_queue_.front();
                message_queue_.pop();
            }
        }
        
        // Process message based on type
        switch (message.getType()) {
            case MessageType::LOAD_UPDATE: {
                int peer_id = message.getSenderId();
                int load = message.getLoadValue();
                
                std::lock_guard<std::mutex> lock(peer_loads_mutex_);
                peer_loads_[peer_id] = load;
                
                Logger::getInstance().logNodeEvent(id_, 
                    "Received load update from node " + std::to_string(peer_id) +
                    ": load=" + std::to_string(load));
                break;
            }
            
            case MessageType::TASK_TRANSFER: {
                auto task = message.getTask();
                if (task) {
                    addTask(task);
                    Logger::getInstance().logNodeEvent(id_, 
                        "Received task " + std::to_string(task->getId()) +
                        " from node " + std::to_string(message.getSenderId()));
                }
                break;
            }
            
            case MessageType::PEER_DISCOVERY: {
                addPeer(message.getSenderId());
                break;
            }
            
            default:
                break;
        }
    }
}

// Offload a task to the least-loaded peer
void PeerNode::offloadTask(std::shared_ptr<Task> task) {
    int best_peer = selectBestPeer();
    
    if (best_peer != -1 && network_manager_) {
        Message transfer_msg(MessageType::TASK_TRANSFER, id_, best_peer);
        transfer_msg.setTask(task);
        network_manager_->sendMessage(transfer_msg);
        
        Logger::getInstance().logNodeEvent(id_, 
            "Offloaded task " + std::to_string(task->getId()) +
            " to node " + std::to_string(best_peer));
    } else {
        // No suitable peer, add back to own queue
        addTask(task);
    }
}

// Select the least-loaded peer for task routing
int PeerNode::selectBestPeer() {
    std::lock_guard<std::mutex> lock(peer_loads_mutex_);
    
    if (peer_loads_.empty()) {
        return -1;
    }
    
    int best_peer = -1;
    int min_load = getCurrentLoad();  // Only offload to peers with less load
    
    for (const auto& [peer_id, load] : peer_loads_) {
        if (load < min_load) {
            min_load = load;
            best_peer = peer_id;
        }
    }
    
    return best_peer;
}
