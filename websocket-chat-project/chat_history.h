#ifndef CHAT_HISTORY_H
#define CHAT_HISTORY_H

#include "message.h"

#include <string>
#include <vector>

class ChatHistory {
public:
    explicit ChatHistory(const std::string& fileName);

    void storeMessage(const Message& message);
    std::vector<std::string> showHistory(const std::string& user) const;

private:
    std::string fileName_;
};

#endif
