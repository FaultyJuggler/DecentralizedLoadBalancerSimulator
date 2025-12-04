cmake_minimum_required(VERSION 3.20)
project(DecentralizedLoadBalancerSimulator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -pthread)
endif()

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# Source files
set(SOURCES
    src/main.cpp
    src/Task.cpp
    src/Message.cpp
    src/Logger.cpp
    src/PeerNode.cpp
    src/NetworkManager.cpp
)

# Create executable
add_executable(load_balancer ${SOURCES})

# Link threading library
find_package(Threads REQUIRED)
target_link_libraries(load_balancer Threads::Threads)

# For macOS, ensure proper threading support
if(APPLE)
    target_compile_definitions(load_balancer PRIVATE _DARWIN_C_SOURCE)
endif()
