/* Start Header
*****************************************************************/
/*!
\file server.cpp
\author AMOS LAU (junhaoamos.lau@digipen.edu)
\par Course CSD2161
\date 03/16/2025
\brief
This file implements the server file which will be used to implement a
simple echo server.
Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
*******************************************************************/

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include <string>
#include <unordered_map>
#include <mutex>
#include <vector>
#include "taskqueue.h"

#pragma comment(lib, "ws2_32.lib")

// Constants
#define WINSOCK_VERSION     2
#define WINSOCK_SUBVERSION  2
#define MAX_STR_LEN         1000
#define RETURN_CODE_1       1
#define RETURN_CODE_2       2
#define RETURN_CODE_3       3
#define RETURN_CODE_4       4

// Command IDs
enum class CommandID : unsigned char {
    UNKNOWN = 0x0,  // Unknown command
    REQ_QUIT = 0x1,  // Client requests to quit
    REQ_ECHO = 0x2,  // Client requests an echo
    RSP_ECHO = 0x3,  // Server responds to an echo request
    REQ_LISTUSERS = 0x4,  // Client requests the list of users
    RSP_LISTUSERS = 0x5,  // Server responds with the list of users
    CMD_TEST = 0x20, // Test command (not used)
    ECHO_ERROR = 0x30  // Server indicates an echo error
};

// Server class to encapsulate server functionality
class Server {
public:
    Server() = default; // Default constructor
    ~Server() { cleanup(); } // Destructor to clean up resources

    // Initialize the server with the given port
    bool initialize(const std::string& port);

    // Run the server
    void run();

private:
    SOCKET listenerSocket = INVALID_SOCKET; // Socket for listening to incoming connections
    std::unordered_map<std::string, SOCKET> clients; // Map of client keys to their sockets
    std::mutex clientsMutex; // Mutex to protect access to the clients map
    std::string port; // Port number the server listens on

    // Set up Winsock
    bool setupWinsock();
    // Resolve the server's address
    bool resolveAddress(const std::string& port);
    // Create the listener socket
    bool createListener();
    //create UDP listener.
    bool createSocket();
    // Bind the listener socket to the server's address
    bool bindListener();
    // Start listening for incoming connections
    bool startListening();
    // Handle a connected client
    void handleClient(SOCKET clientSocket);
    // Forward an echo message to another client
    void forwardEchoMessage(char* buffer, int length, const std::string& senderKey);
    // Send the list of connected users to a client
    void sendUserList(SOCKET clientSocket);
    // Handle server disconnection
    void onDisconnect();
    // Clean up resources
    void cleanup();
};

// Main function
int main() {
    std::string portNumber;
    std::cout << "Server Port Number: ";
    std::getline(std::cin, portNumber);

    Server server;
    if (!server.initialize(portNumber)) {
        std::cerr << "Server initialization failed." << std::endl;
        return 1;
    }

    server.run();
    return 0;
}

// Server implementation

// Initialize the server
bool Server::initialize(const std::string& port) {
    this->port = port; // Initialize the port member variable
    if (!setupWinsock()) return false; // Set up Winsock
    if (!resolveAddress(port)) return false; // Resolve the server's address
    if (!createSocket()) return false; // Create the listener socket
    if (!bindListener()) return false; // Bind the listener socket
    //if (!startListening()) return false; // Start listening for connections

    return true; // Initialization successful
}

// Run the server
void Server::run() {
    // Wrap handleClient in a lambda to make it compatible with TaskQueue
    auto action = [this](SOCKET socket) {
        handleClient(socket); // Each worker will call recvfrom() internally
        return true;
        };

    // Define the onDisconnect lambda (not really needed for UDP)
    const auto onDisconnect = [&]() {
        shutdown(listenerSocket, SD_BOTH);
        closesocket(listenerSocket);
        return true;
        };

    // Initialize TaskQueue with SOCKET instead of message data
    auto tq = TaskQueue<SOCKET, decltype(action), decltype(onDisconnect)>{
        10, 20, action, onDisconnect
    };

    // Main server loop for UDP
    while (true) {
        tq.produce(listenerSocket); // Always add the same listener socket to the queue
    }
}

// Set up Winsock
bool Server::setupWinsock() {
    WSADATA wsaData{};
    int errorCode = WSAStartup(MAKEWORD(WINSOCK_VERSION, WINSOCK_SUBVERSION), &wsaData);
    if (errorCode != NO_ERROR) {
        std::cerr << "WSAStartup() failed." << std::endl;
        return false;
    }
    return true;
}

// Resolve the server's address
bool Server::resolveAddress(const std::string& port) {
    addrinfo hints{};
    SecureZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;        // Use IPv4
    hints.ai_socktype = SOCK_DGRAM;   // Use UDP (change from SOCK_STREAM)
    hints.ai_protocol = IPPROTO_UDP;  // Use UDP protocol (change from IPPROTO_TCP)
    hints.ai_flags = AI_PASSIVE;      // Use the server's IP address

    char host[MAX_STR_LEN];
    gethostname(host, MAX_STR_LEN);   // Get the server's hostname

    addrinfo* info = nullptr;
    int errorCode = getaddrinfo(host, port.c_str(), &hints, &info);
    if (errorCode != NO_ERROR || info == nullptr) {
        std::cerr << "getaddrinfo() failed." << std::endl;
        WSACleanup();
        return false;
    }

    // Print the server's IP address and port number
    char serverIPAddr[MAX_STR_LEN];
    struct sockaddr_in* serverAddress = reinterpret_cast<struct sockaddr_in*>(info->ai_addr);
    inet_ntop(AF_INET, &(serverAddress->sin_addr), serverIPAddr, INET_ADDRSTRLEN);
    std::cout << "Server IP Address: " << serverIPAddr << std::endl;
    std::cout << "Server Port Number: " << port << std::endl;

    freeaddrinfo(info);  // Free the address info
    return true;
}

bool Server::createSocket() {
    listenerSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);  // Use SOCK_DGRAM for UDP
    if (listenerSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed." << std::endl;
        WSACleanup();
        return false;
    }
    return true;
}


// Create the listener socket
bool Server::createListener() {
    listenerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenerSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed." << std::endl;
        WSACleanup();
        return false;
    }
    return true;
}

// Bind the listener socket to the server's address
bool Server::bindListener() {
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;              // Use IPv4
    serverAddr.sin_addr.s_addr = INADDR_ANY;      // Bind to any available interface
    serverAddr.sin_port = htons(std::stoi(port)); // Convert port to network byte order

    if (bind(listenerSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "bind() failed." << std::endl;
        closesocket(listenerSocket);
        WSACleanup();
        return false;
    }
    return true;
}


// Start listening for incoming connections
bool Server::startListening() {
    if (listen(listenerSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "listen() failed." << std::endl;
        closesocket(listenerSocket);
        WSACleanup();
        return false;
    }
    return true;
}

// Handle a connected client
void Server::handleClient(SOCKET clientSocket) {
    sockaddr_in clientAddr{};  // Store client address
    int addrSize = sizeof(clientAddr);
    char buffer[MAX_STR_LEN] = {}; // Buffer for received data

    // Receive a message from any client
    int bytesReceived = recvfrom(clientSocket, buffer, sizeof(buffer), 0,
        (sockaddr*)&clientAddr, &addrSize);

    if (bytesReceived == SOCKET_ERROR) {
        std::cerr << "recvfrom() failed: " << WSAGetLastError() << std::endl;
        return;
    }

    // Convert client address to readable format
    char clientIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
    uint16_t clientPort = ntohs(clientAddr.sin_port);
    std::string clientKey = std::string(clientIP) + ":" + std::to_string(clientPort);

    std::cout << "Message received from: " << clientKey << std::endl;

    // Send acknowledgment message to client
    const char* ackMessage = "Connected to server";
    int bytesSent = sendto(clientSocket, ackMessage, strlen(ackMessage), 0,
        (sockaddr*)&clientAddr, addrSize);

    if (bytesSent == SOCKET_ERROR) {
        std::cerr << "sendto() failed: " << WSAGetLastError() << std::endl;
        return;
    }

    std::cout << "Sent acknowledgment to " << clientKey << std::endl;
}


// Forward an echo message to another client
void Server::forwardEchoMessage(char* buffer, int length, const std::string& senderKey) {
    uint32_t destIPAddress;
    uint16_t destPort;

    // Extract destination IP and port from the buffer
    memcpy(&destIPAddress, buffer + 1, 4);
    memcpy(&destPort, buffer + 5, 2);
    destPort = ntohs(destPort); // Convert port to host byte order

    char destIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &destIPAddress, destIP, INET_ADDRSTRLEN); // Convert IP to string
    std::string destKey = std::string(destIP) + ":" + std::to_string(destPort); // Create destination key

    std::lock_guard<std::mutex> lock(clientsMutex); // Lock the clients map

    // Check if the destination client exists
    if (clients.find(destKey) == clients.end()) {
        std::cerr << "Client not found: " << destKey << std::endl;
        char errorMessage[1] = { static_cast<char>(CommandID::ECHO_ERROR) };
        send(clients[senderKey], errorMessage, 1, 0); // Send an error message to the sender
        return;
    }

    buffer[0] = static_cast<char>(CommandID::RSP_ECHO); // Change the command ID to RSP_ECHO
    send(clients[destKey], buffer, length, 0); // Send the message to the destination client

    uint32_t senderIP;
    uint16_t senderPort;

    // Extract sender IP and port from the sender key
    inet_pton(AF_INET, senderKey.substr(0, senderKey.find(':')).c_str(), &senderIP);
    senderPort = htons(std::stoi(senderKey.substr(senderKey.find(':') + 1)));

    // Update the buffer with the sender's IP and port
    memcpy(buffer + 1, &senderIP, 4);
    memcpy(buffer + 5, &senderPort, 2);

    send(clients[senderKey], buffer, length, 0); // Send the updated message back to the sender

    // Log the forwarded message
    std::cout << "==========FORWARDED MESSAGE==========\n"
        << "From: " << senderKey << "\n"
        << "To: " << destKey << "\n"
        << "Message: " << std::string(buffer + 11, length - 11) << "\n"
        << "======================================\n";
}

// Send the list of connected users to a client
void Server::sendUserList(SOCKET clientSocket) {
    std::lock_guard<std::mutex> lock(clientsMutex); // Lock the clients map

    uint16_t userCount = htons(clients.size()); // Convert user count to network byte order
    std::vector<char> message;
    message.push_back(static_cast<char>(CommandID::RSP_LISTUSERS)); // Add the command ID
    message.insert(message.end(), reinterpret_cast<char*>(&userCount), reinterpret_cast<char*>(&userCount) + 2); // Add the user count

    // Add each client's IP and port to the message
    for (const auto& [key, socket] : clients) {
        uint32_t ip;
        uint16_t port;
        std::string ipStr = key.substr(0, key.find(':'));
        inet_pton(AF_INET, ipStr.c_str(), &ip); // Convert IP to binary
        port = htons(std::stoi(key.substr(key.find(':') + 1))); // Convert port to network byte order

        message.insert(message.end(), reinterpret_cast<char*>(&ip), reinterpret_cast<char*>(&ip) + 4); // Add IP
        message.insert(message.end(), reinterpret_cast<char*>(&port), reinterpret_cast<char*>(&port) + 2); // Add port
    }

    send(clientSocket, message.data(), message.size(), 0); // Send the message to the client
}

// Handle server disconnection
void Server::onDisconnect() {
    std::cout << "Server is shutting down..." << std::endl;
}

// Clean up resources
void Server::cleanup() {
    if (listenerSocket != INVALID_SOCKET) {
        closesocket(listenerSocket); // Close the listener socket
    }
    WSACleanup(); // Clean up Winsock
}