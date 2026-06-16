#include "message_router.h"

#include "message.h"

#include <cstring>

namespace {
std::string escapeJson(const std::string& text) {
    std::string out;
    for (char c : text) {
        if (c == '\\') out += "\\\\";
        else if (c == '"') out += "\\\"";
        else if (c == '\n') out += "\\n";
        else out += c;
    }
    return out;
}
}

MessageRouter::MessageRouter(ChatHistory& history) : history_(history) {}

void MessageRouter::registerUser(const std::string& user, SOCKET socket) {
    activeUsers_[user] = socket;
    socketUsers_[socket] = user;
}

void MessageRouter::unregisterUser(SOCKET socket) {
    auto it = socketUsers_.find(socket);
    if (it != socketUsers_.end()) {
        activeUsers_.erase(it->second);
        socketUsers_.erase(it);
    }
}

void MessageRouter::sendMessage(const std::string& sender, const std::string& receiver, const std::string& text) {
    Message message(sender, receiver, text);
    history_.storeMessage(message);

    std::string payload = "{\"sender\":\"" + escapeJson(sender) +
                          "\",\"receiver\":\"" + escapeJson(receiver) +
                          "\",\"text\":\"" + escapeJson(text) + "\"}";
    std::string frameText = "MESSAGE:" + payload;

    auto senderSocket = activeUsers_.find(sender);
    if (senderSocket != activeUsers_.end()) {
        sendFrame(senderSocket->second, frameText);
    }

    auto receiverSocket = activeUsers_.find(receiver);
    if (receiverSocket != activeUsers_.end() && receiver != sender) {
        sendFrame(receiverSocket->second, frameText);
    }
}

std::vector<std::string> MessageRouter::activeUsernames() const {
    std::vector<std::string> users;
    for (const auto& pair : activeUsers_) {
        users.push_back(pair.first);
    }
    return users;
}

void MessageRouter::broadcastUsers() const {
    std::string list = "USERS:";
    for (const auto& pair : activeUsers_) {
        list += pair.first + ",";
    }

    for (const auto& pair : activeUsers_) {
        sendFrame(pair.second, list);
    }
}

void MessageRouter::sendFrame(SOCKET socket, const std::string& text) const {
    std::string frame;
    frame.push_back(static_cast<char>(0x81));

    if (text.size() < 126) {
        frame.push_back(static_cast<char>(text.size()));
    } else {
        frame.push_back(static_cast<char>(126));
        frame.push_back(static_cast<char>((text.size() >> 8) & 0xff));
        frame.push_back(static_cast<char>(text.size() & 0xff));
    }

    frame += text;
    send(socket, frame.data(), static_cast<int>(frame.size()), 0);
}
