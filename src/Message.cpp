#include "Message.h"
#include <sstream>

Message::Message(MessageType type, int sender_id, int receiver_id)
    : type_(type), sender_id_(sender_id), receiver_id_(receiver_id),
      load_value_(0), task_(nullptr) {
}

MessageType Message::getType() const {
    return type_;
}

int Message::getSenderId() const {
    return sender_id_;
}

int Message::getReceiverId() const {
    return receiver_id_;
}

void Message::setLoadValue(int load) {
    load_value_ = load;
}

int Message::getLoadValue() const {
    return load_value_;
}

void Message::setTask(std::shared_ptr<Task> task) {
    task_ = task;
}

std::shared_ptr<Task> Message::getTask() const {
    return task_;
}

std::string Message::toString() const {
    std::stringstream ss;
    ss << "Message[";
    
    switch (type_) {
        case MessageType::LOAD_UPDATE:
            ss << "LOAD_UPDATE";
            break;
        case MessageType::TASK_REQUEST:
            ss << "TASK_REQUEST";
            break;
        case MessageType::TASK_TRANSFER:
            ss << "TASK_TRANSFER";
            break;
        case MessageType::PEER_DISCOVERY:
            ss << "PEER_DISCOVERY";
            break;
    }
    
    ss << " from=" << sender_id_ << " to=" << receiver_id_;
    
    if (type_ == MessageType::LOAD_UPDATE) {
        ss << " load=" << load_value_;
    } else if (type_ == MessageType::TASK_TRANSFER && task_) {
        ss << " task_id=" << task_->getId();
    }
    
    ss << "]";
    return ss.str();
}
