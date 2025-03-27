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
struct UdpClientData {
    sockaddr_in clientAddr;
    char data[1024];  // Buffer for incoming data
    int dataSize;     // Size of the incoming data
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
    // Bind the listener socket to the server's address
    bool bindListener();
    // Start listening for incoming connections
    bool startListening();
    // Handle a connected client
    void handleClient(SOCKET clientSocket);
    //handling UDP client data.
    void handleUdpClient(UdpClientData messsage);
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
    if (!createListener()) return false; // Create the listener socket
    if (!bindListener()) return false; // Bind the listener socket
    //if (!startListening()) return false; // Start listening for connections

    return true; // Initialization successful
}

// Run the server
void Server::run() {
    // Define the action lambda to handle UDP messages
    auto action = [this](UdpClientData message) {
        handleUdpClient(message); // Handle the UDP message
        return true; // Ensure the lambda returns a boolean
        };

    // Define the onDisconnect lambda (though for UDP, disconnect isn't needed)
    const auto onDisconnect = [&]() {
        closesocket(listenerSocket); // Close the listener socket
        return true;
        };

    // Initialize TaskQueue with the correct parameters
    auto tq = TaskQueue<UdpClientData, decltype(action), decltype(onDisconnect)>{
        10, 20, action, onDisconnect
    };

    // Main server loop: Use recvfrom() to receive UDP messages
    while (true) {
        sockaddr_in clientAddr{};
        socklen_t clientAddrSize = sizeof(clientAddr);
        char buffer[1024];  // Buffer to hold incoming data

        // Receive the UDP message from the client
        int bytesReceived = recvfrom(listenerSocket, buffer, sizeof(buffer), 0,
            (sockaddr*)&clientAddr, &clientAddrSize);

        if (bytesReceived == SOCKET_ERROR) {
            //std::cerr << "Receive failed." << std::endl;
            continue;
        }

        // Create the UdpClientData object with received data
        UdpClientData clientData;
        clientData.clientAddr = clientAddr;
        memcpy(clientData.data, buffer, bytesReceived);
        clientData.dataSize = bytesReceived;

        // Add the client data to the task queue for processing by worker threads
        tq.produce(clientData);
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
    hints.ai_family = AF_INET; // Use IPv4
    hints.ai_socktype = SOCK_DGRAM; // Use UDP
    hints.ai_protocol = IPPROTO_UDP; // Use UDP
    hints.ai_flags = AI_PASSIVE; // Use the server's IP address

    char host[MAX_STR_LEN];
    gethostname(host, MAX_STR_LEN); // Get the server's hostname

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

    freeaddrinfo(info); // Free the address info
    return true;
}


// Create the listener socket
bool Server::createListener() {
    listenerSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); // Changed to SOCK_DGRAM for UDP
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
    serverAddr.sin_family = AF_INET; // Use IPv4
    serverAddr.sin_addr.s_addr = INADDR_ANY; // Bind to any available interface
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
    sockaddr_in clientAddr{};
    int addrSize = sizeof(clientAddr);
    getpeername(clientSocket, (sockaddr*)&clientAddr, &addrSize); // Get client's address

    char clientIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN); // Convert IP to string
    uint16_t clientPort = ntohs(clientAddr.sin_port); // Convert port to host byte order
    std::string clientKey = std::string(clientIP) + ":" + std::to_string(clientPort); // Create client key

    {
        std::lock_guard<std::mutex> lock(clientsMutex); // Lock the clients map
        clients[clientKey] = clientSocket; // Add the client to the map
    }

    std::cout << "Client connected: " << clientKey << std::endl;

    char buffer[MAX_STR_LEN] = {};
    while (true) {
        int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0); // Receive data from the client
        if (bytesRead <= 0) {
            std::cout << "Client disconnected: " << clientKey << std::endl;
            break;
        }

        CommandID commandID = static_cast<CommandID>(buffer[0]); // Extract the command ID
        switch (commandID) {
        case CommandID::REQ_QUIT:
            std::cout << "Client requested to quit: " << clientKey << std::endl;
            break;
        case CommandID::REQ_ECHO:
            forwardEchoMessage(buffer, bytesRead, clientKey); // Forward the echo message
            break;
        case CommandID::REQ_LISTUSERS:
            sendUserList(clientSocket); // Send the list of users
            break;
        default:
            std::cerr << "Unhandled command ID: " << static_cast<int>(commandID) << std::endl;
            break;
        }
    }

    {
        std::lock_guard<std::mutex> lock(clientsMutex); // Lock the clients map
        clients.erase(clientKey); // Remove the client from the map
    }

    shutdown(clientSocket, SD_BOTH); // Shutdown the client socket
    closesocket(clientSocket); // Close the client socket
}

void Server::handleUdpClient(UdpClientData message)
{
    // Unpack the message and client address from the incoming message
    const char* messageData = message.data; // Message content
    sockaddr_in clientAddr = message.clientAddr; // Client address

    // Null-terminate the message if it's not already null-terminated
    message.data[message.dataSize] = '\0';  // Ensure null-termination at the end

    // Get client IP and port
    char clientIp[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, sizeof(clientIp));
    int clientPort = ntohs(clientAddr.sin_port);

    // Print the message along with client IP and port
    std::cout << "Received message from client (" << clientIp << ":" << clientPort << "): "
        << messageData << std::endl;

    // Create the message to send back, prepending "Message from server: "
    std::string serverMessage = "This is a Message from server:" + std::string(messageData);

    // Send the message back to the client (echoing it with the prefix)
    int sendResult = sendto(listenerSocket, serverMessage.c_str(), serverMessage.length(), 0,
        reinterpret_cast<sockaddr*>(&clientAddr), sizeof(clientAddr));

    if (sendResult == SOCKET_ERROR) {
        std::cerr << "Failed to send message to client." << std::endl;
    }
    else {
        std::cout << "Echoed message to client: " << serverMessage << std::endl;
    }

    // Free up the thread (the thread will exit when the task completes)
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