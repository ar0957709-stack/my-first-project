#include "chat_history.h"

#include <fstream>

ChatHistory::ChatHistory(const std::string& fileName) : fileName_(fileName) {}

void ChatHistory::storeMessage(const Message& message) {
    std::ofstream out(fileName_, std::ios::app);
    if (out) {
        out << message.format() << '\n';
    }
}

std::vector<std::string> ChatHistory::showHistory(const std::string& user) const {
    std::ifstream in(fileName_);
    std::vector<std::string> rows;
    std::string line;

    while (std::getline(in, line)) {
        if (line.find(user + " -> ") != std::string::npos ||
            line.find(" -> " + user + " : ") != std::string::npos) {
            rows.push_back(line);
        }
    }

    return rows;
}
