#ifndef MESSAGE_ROUTER_H
#define MESSAGE_ROUTER_H

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>

#include "chat_history.h"

#include <string>
#include <unordered_map>
#include <vector>

class MessageRouter {
public:
    explicit MessageRouter(ChatHistory& history);

    void registerUser(const std::string& user, SOCKET socket);
    void unregisterUser(SOCKET socket);
    void sendMessage(const std::string& sender, const std::string& receiver, const std::string& text);
    void broadcastUsers() const;
    std::vector<std::string> activeUsernames() const;

private:
    ChatHistory& history_;
    std::unordered_map<std::string, SOCKET> activeUsers_;
    std::unordered_map<SOCKET, std::string> socketUsers_;

    void sendFrame(SOCKET socket, const std::string& text) const;
};

#endif
