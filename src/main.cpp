#include <iostream>
#include <vector>
#include <memory>
#include <random>
#include <chrono>
#include <thread>
#include "PeerNode.h"
#include "NetworkManager.h"
#include "Task.h"
#include "Logger.h"

// Configuration
const int NUM_NODES = 5;
const int LOAD_THRESHOLD = 10;
const int SIMULATION_DURATION_SECONDS = 30;
const int TASK_GENERATION_INTERVAL_MS = 100;
const int MIN_TASK_COMPLEXITY = 50;   // ms
const int MAX_TASK_COMPLEXITY = 200;  // ms

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "Decentralized Load Balancer Simulation" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "Configuration:" << std::endl;
    std::cout << "  Number of nodes: " << NUM_NODES << std::endl;
    std::cout << "  Load threshold: " << LOAD_THRESHOLD << std::endl;
    std::cout << "  Simulation duration: " << SIMULATION_DURATION_SECONDS << "s" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << std::endl;
    
    // Setup logging
    Logger::getInstance().setLogFile("logs/simulation.log");
    Logger::getInstance().log("=== Simulation Started ===");
    
    // Create network manager
    auto network_manager = std::make_shared<NetworkManager>();
    
    // Create peer nodes
    std::vector<std::shared_ptr<PeerNode>> nodes;
    for (int i = 0; i < NUM_NODES; ++i) {
        auto node = std::make_shared<PeerNode>(i, LOAD_THRESHOLD, network_manager.get());
        nodes.push_back(node);
        network_manager->registerNode(i, node.get());
    }
    
    // Setup peer connections (fully connected mesh)
    for (int i = 0; i < NUM_NODES; ++i) {
        for (int j = 0; j < NUM_NODES; ++j) {
            if (i != j) {
                nodes[i]->addPeer(j);
            }
        }
    }
    
    // Start all nodes
    std::cout << "Starting " << NUM_NODES << " nodes..." << std::endl;
    for (auto& node : nodes) {
        node->start();
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "All nodes started successfully!" << std::endl;
    std::cout << std::endl;
    
    // Random number generation for task creation
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> node_dist(0, NUM_NODES - 1);
    std::uniform_int_distribution<> complexity_dist(MIN_TASK_COMPLEXITY, MAX_TASK_COMPLEXITY);
    
    // Task generation thread
    std::atomic<bool> generating(true);
    std::atomic<int> task_counter(0);
    
    std::thread task_generator([&]() {
        while (generating) {
            // Generate a task and assign to a random node
            int task_id = task_counter++;
            int target_node = node_dist(gen);
            int complexity = complexity_dist(gen);
            
            auto task = std::make_shared<Task>(task_id, complexity);
            nodes[target_node]->addTask(task);
            
            std::this_thread::sleep_for(std::chrono::milliseconds(TASK_GENERATION_INTERVAL_MS));
        }
    });
    
    // Run simulation
    std::cout << "Running simulation for " << SIMULATION_DURATION_SECONDS << " seconds..." << std::endl;
    std::cout << "Generating tasks every " << TASK_GENERATION_INTERVAL_MS << "ms" << std::endl;
    std::cout << std::endl;
    
    // Progress updates
    for (int i = 0; i < SIMULATION_DURATION_SECONDS; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        std::cout << "Time: " << (i + 1) << "s - ";
        int total_load = 0;
        int total_processed = 0;
        
        for (const auto& node : nodes) {
            total_load += node->getCurrentLoad();
            total_processed += node->getTasksProcessed();
        }
        
        std::cout << "Total queue: " << total_load 
                  << ", Total processed: " << total_processed << std::endl;
    }
    
    std::cout << std::endl;
    
    // Stop task generation
    generating = false;
    if (task_generator.joinable()) {
        task_generator.join();
    }
    
    // Allow some time for remaining tasks to be processed
    std::cout << "Stopping task generation, processing remaining tasks..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // Collect final statistics
    std::cout << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "Final Statistics:" << std::endl;
    std::cout << "==================================================" << std::endl;
    
    int total_processed = 0;
    int total_remaining = 0;
    
    for (const auto& node : nodes) {
        int processed = node->getTasksProcessed();
        int remaining = node->getCurrentLoad();
        
        total_processed += processed;
        total_remaining += remaining;
        
        std::cout << "Node " << node->getId() << ": "
                  << "Processed=" << processed << ", "
                  << "Remaining=" << remaining << std::endl;
    }
    
    std::cout << "--------------------------------------------------" << std::endl;
    std::cout << "Total tasks generated: " << task_counter.load() << std::endl;
    std::cout << "Total tasks processed: " << total_processed << std::endl;
    std::cout << "Total tasks remaining: " << total_remaining << std::endl;
    std::cout << "==================================================" << std::endl;
    
    Logger::getInstance().log("=== Final Statistics ===");
    Logger::getInstance().log("Total tasks generated: " + std::to_string(task_counter.load()));
    Logger::getInstance().log("Total tasks processed: " + std::to_string(total_processed));
    Logger::getInstance().log("Total tasks remaining: " + std::to_string(total_remaining));
    
    // Stop all nodes
    std::cout << std::endl;
    std::cout << "Stopping all nodes..." << std::endl;
    for (auto& node : nodes) {
        node->stop();
    }
    
    std::cout << "Simulation completed successfully!" << std::endl;
    Logger::getInstance().log("=== Simulation Completed ===");
    
    return 0;
}
