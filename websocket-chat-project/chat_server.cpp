#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <wincrypt.h>

#include "chat_history.h"
#include "message_router.h"

#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "advapi32.lib")

const int PORT = 8080;
const std::string USER_FILE = "users.txt";

std::mutex appMutex;
ChatHistory history("chat_log_ws.txt");
MessageRouter router(history);

std::string base64Encode(const unsigned char* data, DWORD length) {
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;

    for (DWORD i = 0; i < length; i += 3) {
        int value = (data[i] << 16) |
                    ((i + 1 < length ? data[i + 1] : 0) << 8) |
                    (i + 2 < length ? data[i + 2] : 0);

        out.push_back(table[(value >> 18) & 63]);
        out.push_back(table[(value >> 12) & 63]);
        out.push_back(i + 1 < length ? table[(value >> 6) & 63] : '=');
        out.push_back(i + 2 < length ? table[value & 63] : '=');
    }

    return out;
}

std::string websocketAcceptKey(const std::string& key) {
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    BYTE digest[20];
    DWORD digestLength = sizeof(digest);
    std::string input = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    CryptAcquireContext(&provider, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT);
    CryptCreateHash(provider, CALG_SHA1, 0, 0, &hash);
    CryptHashData(hash, reinterpret_cast<const BYTE*>(input.c_str()), static_cast<DWORD>(input.size()), 0);
    CryptGetHashParam(hash, HP_HASHVAL, digest, &digestLength, 0);
    CryptDestroyHash(hash);
    CryptReleaseContext(provider, 0);

    return base64Encode(digest, digestLength);
}

std::string readFile(const std::string& fileName) {
    std::ifstream in(fileName, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

void sendAll(SOCKET socket, const std::string& data) {
    send(socket, data.c_str(), static_cast<int>(data.size()), 0);
}

void sendHttp(SOCKET socket, const std::string& body, const std::string& type = "text/html") {
    sendAll(socket,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: " + type + "\r\n"
        "Connection: close\r\n\r\n" + body);
}

void sendWebSocketText(SOCKET socket, const std::string& text) {
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

std::map<std::string, std::string> loadUsers() {
    std::ifstream in(USER_FILE);
    std::map<std::string, std::string> users;
    std::string username;
    std::string password;

    while (in >> username >> password) {
        users[username] = password;
    }

    return users;
}

void saveUser(const std::string& username, const std::string& password) {
    std::ofstream out(USER_FILE, std::ios::app);
    out << username << ' ' << password << '\n';
}

std::string jsonValue(const std::string& body, const std::string& key) {
    std::string token = "\"" + key + "\"";
    size_t pos = body.find(token);
    if (pos == std::string::npos) return "";

    pos = body.find(':', pos);
    pos = body.find('"', pos);
    if (pos == std::string::npos) return "";

    size_t end = body.find('"', pos + 1);
    if (end == std::string::npos) return "";

    return body.substr(pos + 1, end - pos - 1);
}

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

void sendHistory(SOCKET socket, const std::string& user) {
    std::vector<std::string> rows = history.showHistory(user);
    std::string payload = "HISTORY:[";

    for (size_t i = 0; i < rows.size(); ++i) {
        if (i > 0) payload += ",";
        payload += "\"" + escapeJson(rows[i]) + "\"";
    }

    payload += "]";
    sendWebSocketText(socket, payload);
}

std::string decodeFrame(const char* buffer, int length) {
    if (length < 6) return "";

    int payloadLength = buffer[1] & 127;
    int offset = 2;
    if (payloadLength == 126) {
        payloadLength = (static_cast<unsigned char>(buffer[2]) << 8) | static_cast<unsigned char>(buffer[3]);
        offset = 4;
    }

    unsigned char mask[4];
    for (int i = 0; i < 4; ++i) {
        mask[i] = buffer[offset + i];
    }
    offset += 4;

    std::string message;
    for (int i = 0; i < payloadLength && offset + i < length; ++i) {
        message.push_back(buffer[offset + i] ^ mask[i % 4]);
    }

    return message;
}

void handleWebSocket(SOCKET client, const std::string& request) {
    size_t keyStart = request.find("Sec-WebSocket-Key:");
    if (keyStart == std::string::npos) {
        closesocket(client);
        return;
    }

    std::string key = request.substr(keyStart + 19);
    key = key.substr(0, key.find("\r\n"));

    sendAll(client,
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + websocketAcceptKey(key) + "\r\n\r\n");

    std::string currentUser;
    char buffer[4096];

    while (true) {
        int received = recv(client, buffer, sizeof(buffer), 0);
        if (received <= 0) break;

        std::string message = decodeFrame(buffer, received);
        if (message.rfind("LOGIN:", 0) == 0) {
            currentUser = message.substr(6);
            std::lock_guard<std::mutex> lock(appMutex);
            router.registerUser(currentUser, client);
            sendHistory(client, currentUser);
            router.broadcastUsers();
        } else if (message.rfind("MSG:", 0) == 0) {
            size_t split = message.find(':', 4);
            if (split != std::string::npos && !currentUser.empty()) {
                std::string receiver = message.substr(4, split - 4);
                std::string text = message.substr(split + 1);
                std::lock_guard<std::mutex> lock(appMutex);
                router.sendMessage(currentUser, receiver, text);
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(appMutex);
        router.unregisterUser(client);
        router.broadcastUsers();
    }
    closesocket(client);
}

void handleHttp(SOCKET client, const std::string& request) {
    std::string body;
    size_t bodyStart = request.find("\r\n\r\n");
    if (bodyStart != std::string::npos) {
        body = request.substr(bodyStart + 4);
    }

    if (request.find("GET / ") == 0 || request.find("GET /chat ") == 0) {
        sendHttp(client, readFile("websocket_chat.html"));
    } else if (request.find("POST /signup ") == 0) {
        std::string username = jsonValue(body, "username");
        std::string password = jsonValue(body, "password");
        std::map<std::string, std::string> users = loadUsers();

        if (username.empty() || password.empty()) {
            sendHttp(client, "{\"ok\":false,\"message\":\"Enter username and password\"}", "application/json");
        } else if (users.count(username)) {
            sendHttp(client, "{\"ok\":false,\"message\":\"Account already exists\"}", "application/json");
        } else {
            saveUser(username, password);
            sendHttp(client, "{\"ok\":true,\"username\":\"" + username + "\"}", "application/json");
        }
    } else if (request.find("POST /login ") == 0) {
        std::string username = jsonValue(body, "username");
        std::string password = jsonValue(body, "password");
        std::map<std::string, std::string> users = loadUsers();

        if (users.count(username) && users[username] == password) {
            sendHttp(client, "{\"ok\":true,\"username\":\"" + username + "\"}", "application/json");
        } else {
            sendHttp(client, "{\"ok\":false,\"message\":\"Invalid username or password\"}", "application/json");
        }
    } else {
        sendHttp(client, "Not found", "text/plain");
    }

    closesocket(client);
}

void handleClient(SOCKET client) {
    char buffer[8192];
    int received = recv(client, buffer, sizeof(buffer), 0);
    if (received <= 0) {
        closesocket(client);
        return;
    }

    std::string request(buffer, received);
    if (request.find("Upgrade: websocket") != std::string::npos) {
        handleWebSocket(client, request);
    } else {
        handleHttp(client, request);
    }
}

int main() {
    WSADATA data;
    WSAStartup(MAKEWORD(2, 2), &data);

    SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(PORT);

    bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address));
    listen(server, 10);

    std::cout << "C++ WebSocket chat running at http://localhost:" << PORT << std::endl;

    while (true) {
        SOCKET client = accept(server, nullptr, nullptr);
        std::thread(handleClient, client).detach();
    }

    closesocket(server);
    WSACleanup();
    return 0;
}
