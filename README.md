C++ WebSocket Chat Project
This is a simple localhost chat application using C++ as the main backend source:

chat_server.cpp for the HTTP and WebSocket server
message_router.cpp / message_router.h for routing messages with a hash map
message.cpp / message.h for the message data structure
chat_history.cpp / chat_history.h for file-based chat history
websocket_chat.html for the browser UI
users.txt for created accounts
chat_log_ws.txt for saved chat messages
Run
Open a Visual Studio Developer PowerShell in this folder and compile:

cl /EHsc chat_server.cpp message.cpp message_router.cpp chat_history.cpp ws2_32.lib crypt32.lib advapi32.lib
Then run:

.\chat_server.exe
Then open:

http://localhost:8080
Create two accounts in two browser tabs, then send messages between them.
