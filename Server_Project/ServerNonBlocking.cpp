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
#include <sstream>
#include "taskqueue.h"
#include "playerdata.h"

#pragma comment(lib, "ws2_32.lib")

// Constants
#define WINSOCK_VERSION     2
#define WINSOCK_SUBVERSION  2
#define MAX_STR_LEN         1000
#define RETURN_CODE_1       1
#define RETURN_CODE_2       2
#define RETURN_CODE_3       3
#define RETURN_CODE_4       4

// Global timer variable on the server:
float gameTimer = 60.0f; // 60 seconds game duration

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

    // For asteroids management
    void createAsteroid();
    void broadcastAsteroids(SOCKET socket);
    void handleAsteroidDestruction(const std::string& asteroidID);
    void updateAsteroids();
    void updatePlayerScore(const std::string& scoreUpdateData);
    void broadcastScoreUpdate(const playerData& player);
    // Timers for asteroid creation and updates
    std::chrono::steady_clock::time_point lastAsteroidCreation;
    std::chrono::steady_clock::time_point lastAsteroidUpdate;

    // Constants for asteroid creation
    const float ASTEROID_MIN_SCALE = 50.0f;
    const float ASTEROID_MAX_SCALE = 100.0f;
    const int ASTEROID_MAX_COUNT = 12; // Maximum number of asteroids
    const float ASTEROID_CREATION_INTERVAL = 2.0f; // Seconds between asteroid creation
    const float ASTEROID_UPDATE_INTERVAL = 0.05f; // Seconds between asteroid updates

    // For bullets management
    void broadcastBullets(SOCKET socket);
    void updateBullets();
    std::chrono::steady_clock::time_point lastBulletUpdate;
    const float BULLET_LIFETIME = 2.5f; // Seconds until a bullet disappears

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
    // For Assignment 4:
    // Sends position to all connected clients
    void sendPlayerData(SOCKET);
    // Remove inactive players
    void removeInactivePlayers();
    void updateGameTimer(float dt);
    void sendTimeUpdate();
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

void Server::updateGameTimer(float dt) {
    gameTimer -= dt;
    if (gameTimer < 0.0f) {
        gameTimer = 0.0f;
    }
}

// Call this function every frame (or at fixed intervals)
void Server::sendTimeUpdate() {
    // Create a message that starts with a keyword, e.g., "TIME"
    std::string timeMessage = "TIME " + std::to_string(gameTimer) + "\n";

    // Broadcast this message to all players:
    for (auto& [_, player] : players) {
        sendto(listenerSocket, timeMessage.c_str(), timeMessage.size(), 0,
            reinterpret_cast<sockaddr*>(&player.cAddr), sizeof(player));
    }
}


// Run the server
void Server::run() {

    auto lastTime = std::chrono::steady_clock::now();
    bool gameStarted = false; // Flag to start timer when 4 players are connected

    lastBulletUpdate = std::chrono::steady_clock::now();

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

    lastAsteroidCreation = std::chrono::steady_clock::now();
    lastAsteroidUpdate = std::chrono::steady_clock::now();

    static float timeUpdateInterval = 0.0f;

    // Main server loop: Use recvfrom() to receive UDP messages
    while (true) {
        auto currentTime = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;

        // Check if there are 4 or more players to start the game timer
        if (!gameStarted && players.size() >= 4) {
            gameStarted = true;
            std::cout << "4 players have connected. Starting game timer." << std::endl;
        }

        if (gameStarted) {

            updateGameTimer(dt);
            timeUpdateInterval += dt;
            if (timeUpdateInterval >= 1.0f) {  // Send update every 1 second
                sendTimeUpdate();
                timeUpdateInterval = 0.0f;
            }
        }

        sockaddr_in clientAddr{};
        socklen_t clientAddrSize = sizeof(clientAddr);
        char buffer[1024];  // Buffer to hold incoming data

        // Check if it's time to create a new asteroid
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration<float>(now - lastAsteroidCreation).count() > ASTEROID_CREATION_INTERVAL) {
            createAsteroid();
            lastAsteroidCreation = now;
        }

        // Update asteroid positions
        if (std::chrono::duration<float>(now - lastAsteroidUpdate).count() > ASTEROID_UPDATE_INTERVAL) {
            updateAsteroids();
            lastAsteroidUpdate = now;
        }

        // Broadcast asteroid data to all clients
        broadcastAsteroids(listenerSocket);

        updateBullets();
        broadcastBullets(listenerSocket);

        // Receive the UDP message from the client
        int bytesReceived = recvfrom(listenerSocket, buffer, sizeof(buffer), 0,
            (sockaddr*)&clientAddr, &clientAddrSize);

        // removeInactivePlayers();

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

        sendPlayerData(listenerSocket);
    }
}

void Server::createAsteroid() {
    // Only create a new asteroid if we're under the maximum
    std::lock_guard<std::mutex> lock(asteroidsMutex);
    if (asteroids.size() >= ASTEROID_MAX_COUNT) {
        return;
    }

    // Generate a unique ID for the asteroid
    std::string asteroidID = "ast_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());

    // Create random position outside the visible area (reverted to original positions)
    float x, y;
    // Randomly choose a side (0-3: top, right, bottom, left)
    int side = rand() % 4;
    if (side == 0) { // Top
        x = (float)(rand() % 1600 - 800); // Range: -800 to 800
        y = 400.0f; // Above the screen
    }
    else if (side == 1) { // Right
        x = 800.0f; // Right of the screen
        y = (float)(rand() % 1200 - 600); // Range: -600 to 600
    }
    else if (side == 2) { // Bottom
        x = (float)(rand() % 1600 - 800); // Range: -800 to 800
        y = -400.0f; // Below the screen
    }
    else { // Left
        x = -800.0f; // Left of the screen
        y = (float)(rand() % 1200 - 600); // Range: -600 to 600
    }

    // Create faster velocity directed toward the center (keeping the faster speed)
    float speed = (float)(rand() % 90 + 90) / 1000.0f; // Range: 0.09 to 0.18 (3x faster than original)

    // Calculate direction vector to center (0,0)
    float dirX = 0.0f - x;
    float dirY = 0.0f - y;

    // Normalize direction vector
    float length = sqrt(dirX * dirX + dirY * dirY);
    if (length > 0) {
        dirX /= length;
        dirY /= length;
    }

    // Apply speed to direction
    float velX = dirX * speed;
    float velY = dirY * speed;

    // Random scale with more consistent sizing
    float baseScale = (float)(rand() % (int)(ASTEROID_MAX_SCALE - ASTEROID_MIN_SCALE + 1) + ASTEROID_MIN_SCALE);
    float scaleX = baseScale * (0.8f + (float)(rand() % 40) / 100.0f); // 80-120% of base scale
    float scaleY = baseScale * (0.8f + (float)(rand() % 40) / 100.0f); // 80-120% of base scale

    // Create the asteroid
    asteroidData newAsteroid;
    newAsteroid.asteroidID = asteroidID;
    newAsteroid.x = x;
    newAsteroid.y = y;
    newAsteroid.velX = velX;
    newAsteroid.velY = velY;
    newAsteroid.scaleX = scaleX;
    newAsteroid.scaleY = scaleY;
    newAsteroid.active = true;
    newAsteroid.creationTime = std::chrono::steady_clock::now();

    // Add the asteroid to the map
    asteroids[asteroidID] = newAsteroid;

    //std::cout << "Created asteroid: " << asteroidID
    //    << " at (" << x << ", " << y << ")"
    //    << " with velocity (" << velX << ", " << velY << ")"
    //    << " and scale (" << scaleX << ", " << scaleY << ")" << std::endl;
}

void Server::updateAsteroids() {
    std::lock_guard<std::mutex> lock(asteroidsMutex);

    auto now = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(now - lastAsteroidUpdate).count();

    // Cap deltaTime to prevent huge jumps
    if (deltaTime > 0.1f) {
        deltaTime = 0.1f;
    }

    // Update position of all asteroids
    for (auto& [id, asteroid] : asteroids) {
        if (!asteroid.active) continue;

        // Update position with increased velocity multiplier (from 60.0f to 120.0f)
        asteroid.x += asteroid.velX * deltaTime * 120.0f;
        asteroid.y += asteroid.velY * deltaTime * 120.0f;

        // Check if the asteroid is too far out of bounds (for cleanup)
        if (std::abs(asteroid.x) > 1200 || std::abs(asteroid.y) > 800) {
            // Instead of immediate deletion, mark for respawn with faster velocity
            // Returning to original spawn positions
            int side = rand() % 4;
            if (side == 0) { // Top
                asteroid.x = (float)(rand() % 1600 - 800);
                asteroid.y = 400.0f;
            }
            else if (side == 1) { // Right
                asteroid.x = 800.0f;
                asteroid.y = (float)(rand() % 1200 - 600);
            }
            else if (side == 2) { // Bottom
                asteroid.x = (float)(rand() % 1600 - 800);
                asteroid.y = -400.0f;
            }
            else { // Left
                asteroid.x = -800.0f;
                asteroid.y = (float)(rand() % 1200 - 600);
            }

            // Create faster velocity directed toward the center
            float speed = (float)(rand() % 90 + 90) / 1000.0f; // Range: 0.09 to 0.18 (3x faster)

            // Calculate direction vector to center (0,0)
            float dirX = 0.0f - asteroid.x;
            float dirY = 0.0f - asteroid.y;

            // Normalize direction vector
            float length = sqrt(dirX * dirX + dirY * dirY);
            if (length > 0) {
                dirX /= length;
                dirY /= length;
            }

            // Apply speed to direction
            asteroid.velX = dirX * speed;
            asteroid.velY = dirY * speed;
        }
    }

    lastAsteroidUpdate = now;
}
void Server::broadcastAsteroids(SOCKET socket) {
    std::string data = "ASTEROIDS";

    // Lock the asteroids for thread safety
    {
        std::lock_guard<std::mutex> lock(asteroidsMutex);

        // Format: ASTEROIDS|id1,x1,y1,velX1,velY1,scaleX1,scaleY1,active1|id2,...
        for (const auto& [id, asteroid] : asteroids) {
            if (!asteroid.active) continue; // Skip inactive asteroids

            data += "|" + asteroid.asteroidID + ","
                + std::to_string(asteroid.x) + ","
                + std::to_string(asteroid.y) + ","
                + std::to_string(asteroid.velX) + ","
                + std::to_string(asteroid.velY) + ","
                + std::to_string(asteroid.scaleX) + ","
                + std::to_string(asteroid.scaleY) + ","
                + (asteroid.active ? "1" : "0");
        }
    }

    // Send this data to all clients
    for (const auto& [_, player] : players) {
        sendto(socket, data.c_str(), data.size(), 0, (sockaddr*)&player.cAddr, sizeof(player.cAddr));
    }
}

void Server::handleAsteroidDestruction(const std::string& asteroidID) {
    std::lock_guard<std::mutex> lock(asteroidsMutex);

    // Find the asteroid
    auto it = asteroids.find(asteroidID);
    if (it != asteroids.end()) {
        // Mark the asteroid as inactive
        it->second.active = false;

        std::cout << "Destroyed asteroid: " << asteroidID << std::endl;

        // Broadcast the destruction to all clients
        std::string data = "DESTROY_ASTEROID|" + asteroidID;
        for (const auto& [_, player] : players) {
            sendto(listenerSocket, data.c_str(), data.size(), 0, (sockaddr*)&player.cAddr, sizeof(player.cAddr));
        }

        // After a short delay, remove the asteroid
        asteroids.erase(it);
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
// DELETE
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

    // For game
    float x, y, rot;
    int score; // Add variable for score
    std::string clientIPstr(clientIp);
    std::string clientKey = clientIPstr + ":" +
        std::to_string(clientPort);

    // Check if this is an asteroid destruction message
    std::string messageStr(messageData);
    if (messageStr.find("DESTROY_ASTEROID|") == 0) {
        std::string asteroidID = messageStr.substr(17); // Skip "DESTROY_ASTEROID|"
        handleAsteroidDestruction(asteroidID);
        return;
    }

    if (messageStr.find("BULLET_CREATE ") == 0) {
        // Parse the bullet creation message
        // Format: "BULLET_CREATE posX posY velX velY dir bulletID"
        float x, y, velX, velY, dir;
        std::string bulletID;

        // Parse the first 5 values
        std::istringstream iss(messageStr.substr(14)); // Skip "BULLET_CREATE "
        if (iss >> x >> y >> velX >> velY >> dir >> bulletID) {
            // Use the provided bullet ID
            // If no ID was provided, generate one (this is a fallback)
            if (bulletID.empty()) {
                bulletID = clientKey + "_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
            }

            // Create new bullet data
            bulletData bullet;
            bullet.bulletID = bulletID;
            bullet.x = x;
            bullet.y = y;
            bullet.velX = velX;
            bullet.velY = velY;
            bullet.dir = dir;
            bullet.creationTime = std::chrono::steady_clock::now();

            // Store the bullet
            bullets[bulletID] = bullet;

            // Debug output
            std::cout << "Server received bullet: " << bulletID << " at (" << x << ", " << y << ")" << std::endl;
        }
        return;
    }
    if (messageStr.find("UPDATE_SCORE|") == 0)
    {
        std::string scoreUpdateData = messageStr.substr(13); // Skip "SCORE_UPDATE|"
        updatePlayerScore(scoreUpdateData);
        return;
    }

    // Parse received position and score data - now looking for 4 values instead of 3
    if (sscanf_s(messageData, "%f %f %f %d", &x, &y, &rot, &score) != 4) {
        std::cerr << "Data received invalid: " << messageData << std::endl;
        return;
    }

    // Check if player already exists
    auto it = players.find(clientKey);
    if (it != players.end()) {
        // Player exists, update position and score
        it->second.x = x;
        it->second.y = y;
        it->second.rot = rot;
        it->second.score = score; // Update the score
    }
    else {
        // New player, assign ID
        static int nextPlayerID = 1;
        int newPlayerID = nextPlayerID++;
        playerData p;
        p.playerID = std::to_string(newPlayerID);
        p.x = x;
        p.y = y;
        p.rot = rot;
        p.score = score; // Set initial score
        p.cAddr = clientAddr;
        p.cIP = clientIp;
        players[clientKey] = p;
        sendto(listenerSocket, p.playerID.c_str(), p.playerID.size(), 0, (sockaddr*)&p.cAddr, sizeof(p));
    }
}
// Send all players position to each client
void Server::sendPlayerData(SOCKET servSocket) {
    std::string data; // = std::to_string(players.size()) + " ";
    
    // [Player ID] [X pos] [Y pos] [Rotation] [IP]
    for (const auto& [pID, player] : players) {
        data += player.playerID + " " + std::to_string(player.x) + " " + std::to_string(player.y) + " " + std::to_string(player.rot) + " " 
            + std::to_string(player.score) + " " + player.cIP + "\n";
    }

    // Send this data to all clients
    for (const auto& [_, player] : players) {
        // Convert `otherKey` back to sockaddr_in (You'll need to store these properly)
        sendto(servSocket, data.c_str(), data.size(), 0, (sockaddr*)&player.cAddr, sizeof(player));
    }
}

// Unused for now - Function here will remove inactive players based on how the server has no received a message from a client
//                  Since UDP is connectionless, it will track based on client IP address.
void Server::removeInactivePlayers() {
    auto now = std::chrono::steady_clock::now();
    const std::chrono::seconds timeout(5); // 5 seconds timeout

    for (auto it = players.begin(); it != players.end();) {
        if (now - it->second.lastActive > timeout) {
            std::cout << "Removing inactive player ID: " << it->first << std::endl;
            it = players.erase(it); // Remove player and update iterator
        }
        else {
            ++it;
        }
    }
}

// Forward an echo message to another client
// DELETE
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
// Delete
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

void Server::broadcastBullets(SOCKET socket) {
    if (bullets.empty()) {
        return; // Don't send empty data
    }

    std::string data = "BULLETS";

    // Format: BULLETS|id1,x1,y1,velX1,velY1,dir1|id2,...
    for (const auto& [id, bullet] : bullets) {
        data += "|" + bullet.bulletID + "," +
            std::to_string(bullet.x) + "," +
            std::to_string(bullet.y) + "," +
            std::to_string(bullet.velX) + "," +
            std::to_string(bullet.velY) + "," +
            std::to_string(bullet.dir);
    }

    // Debug output - only occasionally to avoid spam
    static int counter = 0;
    if (++counter % 100 == 0) {
        std::cout << "Broadcasting " << bullets.size() << " bullets" << std::endl;
    }

    // Send this data to all clients
    for (const auto& [_, player] : players) {
        sendto(socket, data.c_str(), data.size(), 0, (sockaddr*)&player.cAddr, sizeof(player.cAddr));
    }
}


void Server::updateBullets() {
    auto now = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(now - lastBulletUpdate).count();

    // Cap deltaTime to prevent huge jumps
    if (deltaTime > 0.1f) {
        deltaTime = 0.1f;
    }

    // Debug counter
    static int counter = 0;
    counter++;

    // Update positions and remove expired bullets
    int expiredCount = 0;
    int remainingCount = 0;

    for (auto it = bullets.begin(); it != bullets.end();) {
        bulletData& bullet = it->second;

        // Update position based on velocity
        bullet.x += bullet.velX * deltaTime;
        bullet.y += bullet.velY * deltaTime;

        // Check if bullet has existed for too long
        auto bulletAge = std::chrono::duration<float>(now - bullet.creationTime).count();
        if (bulletAge > BULLET_LIFETIME) {
            it = bullets.erase(it);
            expiredCount++;
        }
        else {
            ++it;
            remainingCount++;
        }
    }

    // Debug output - occasionally to avoid spam
    //if (counter % 100 == 0) {
    //    std::cout << "updateBullets: Expired " << expiredCount << ", Remaining " << remainingCount << std::endl;
    //}

    lastBulletUpdate = now;
}
void Server::updatePlayerScore(const std::string& scoreUpdateData) {
    std::istringstream iss(scoreUpdateData);
    std::string playerID;
    int newScore;

    // Parse the player ID and new score from the message
    iss >> playerID >> newScore;

    // Find the player in the players map
    auto it = players.find(playerID);
    if (it != players.end()) {
        // Update the player's score
        it->second.score = newScore;

        // Broadcast the updated score to all clients
        broadcastScoreUpdate(it->second);
    }
}
void Server::broadcastScoreUpdate(const playerData& player) {
    std::string data = "SCORE_UPDATE|" + player.playerID + " " + std::to_string(player.score);

    for (const auto& [_, player] : players) {
        sendto(listenerSocket, data.c_str(), data.size(), 0, (sockaddr*)&player.cAddr, sizeof(player.cAddr));
    }
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