#ifndef MESSAGE_H
#define MESSAGE_H

#include <string>

class Message {
public:
    Message(const std::string& sender, const std::string& receiver, const std::string& text);

    const std::string& sender() const;
    const std::string& receiver() const;
    const std::string& text() const;
    std::string format() const;

private:
    std::string sender_;
    std::string receiver_;
    std::string text_;
};

#endif
