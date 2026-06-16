#include "message.h"

Message::Message(const std::string& sender, const std::string& receiver, const std::string& text)
    : sender_(sender), receiver_(receiver), text_(text) {}

const std::string& Message::sender() const {
    return sender_;
}

const std::string& Message::receiver() const {
    return receiver_;
}

const std::string& Message::text() const {
    return text_;
}

std::string Message::format() const {
    return sender_ + " -> " + receiver_ + " : " + text_;
}
