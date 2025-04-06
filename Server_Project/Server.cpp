/* Start Header
*******************************************************************/
/*!
\file Server.cpp
\co author Ho Jing Rui
\co author Saminathan Aaron Nicholas
\co author Jay Lim Jun Xiang
\par emails: jingrui.ho@digipen.edu
\	         s.aaronnicholas@digipen.edu
\	         jayjunxiang.lim@digipen.edu
\date 28 March, 2025
\brief This file contains the implementation of the GameServer class which handles:
\      Network communication via UDP sockets
\      Game state management (players, asteroids, bullets)
\      Game timing and synchronization
\      Client message processing
\
\      Key Features:
\      Uses Windows Sockets 2.2 (Winsock) for networking
\      Thread-safe game state management
\      Efficient UDP message broadcasting
\      Scalable task queue architecture
\      Real-time game state synchronization
\
\      Network Protocol:
\      UDP-based communication
\      Custom text-based protocol for game messages
\      Supports player positions, asteroid states, bullet tracking
\      Includes score synchronization and game timing
\
\      Dependencies:
\      Windows SDK
\      Winsock 2.2 library
\      Standard C++17 libraries
\      Custom taskqueue.h and playerdata.h headers
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
#include <sstream>
#include <string>
#include <csignal>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <cmath>
#include "taskqueue.h"
#include "playerdata.h"

#pragma comment(lib, "ws2_32.lib")

// Network Constants
constexpr int WINSOCK_MAJOR_VERSION = 2;
constexpr int WINSOCK_MINOR_VERSION = 2;
constexpr int MAX_STRING_LENGTH = 1000;
constexpr int MAX_BUFFER_SIZE = 1024;
constexpr int MAX_ASTEROIDS = 12;
constexpr float GAME_DURATION_SECONDS = 60.0f;

std::unordered_map<std::string, PlayerData> players;
std::unordered_map<std::string, BulletData> bullets;
std::unordered_map<std::string, AsteroidData> asteroids;
std::mutex asteroidsMutex;

// Server Status Codes
enum ServerStatus {
    STATUS_SUCCESS = 0,
    STATUS_WINSOCK_FAILURE
};

/**
 * @struct UdpClientMessage
 * @brief Contains data received from a UDP client
 */
struct UdpClientMessage {
    sockaddr_in clientAddress;      // Client's network address
    char buffer[MAX_BUFFER_SIZE];   // Received data buffer
    int messageLength;              // Length of received data
};

/**
 * @class GameServer
 * @brief Main server class that handles game state and network communication
 */
class GameServer {
public:
    GameServer() = default;
    ~GameServer() { cleanupResources(); }

    /**
     * @brief Initializes the server with the specified port
     * @param port The port number to listen on
     * @return true if initialization succeeded, false otherwise
     */
    bool initialize(const std::string& port);

    /**
     * @brief Starts the main server loop
     */
    void run();

    /**
    * @brief Request shutdown
    */
    void shutdown();

private:
    // Network members
    SOCKET serverSocket = INVALID_SOCKET;                   // Main server socket
    std::string serverPort;                                 // Server port number
    std::unordered_map<std::string, SOCKET> connectedClients; // Connected clients
    std::mutex clientsMutex;                                // Thread protection for clients

    // Game state members
    float gameTimeRemaining = GAME_DURATION_SECONDS;        // Current game timer
    std::chrono::steady_clock::time_point lastAsteroidCreationTime;
    std::chrono::steady_clock::time_point lastAsteroidUpdateTime;
    std::chrono::steady_clock::time_point lastBulletUpdateTime;

    // Game constants
    const float MIN_ASTEROID_SCALE = 30.0f;
    const float MAX_ASTEROID_SCALE = 100.0f;
    const int ASTEROID_SPAWN_BOUND_MAX_X = 1500;
    const int ASTEROID_SPAWN_BOUND_MIN_X = 1000;
    const float ASTEROID_SPAWN_BOUND_MAX_Y = 400.0f;
    const float ASTEROID_SPAWN_BOUND_MIN_Y = -400.0f;
    const int ASTEROID_SPAWN_HBOUND_MAX_Y = 1000.0f;
    const int ASTEROID_SPAWN_HBOUND_MIN_Y = 500.0f;
    const int ASTEROID_SPEED_MIN = 110;
    const float ASTEROID_SPAWN_INTERVAL = 1.0f;
    const float ASTEROID_UPDATE_INTERVAL = 0.05f;
    const float BULLET_LIFETIME_SECONDS = 2.0f;

    // Player management
    std::unordered_map<std::string, int> ipToPlayerIdMap;   // Maps IPs to player IDs

    // Network initialization methods
    bool initializeWinsock();
    bool resolveServerAddress(const std::string& port);
    bool createServerSocket();
    bool bindServerSocket();

    // Game object management
    void spawnAsteroid();
    void broadcastAsteroidData(SOCKET socket);
    void handleAsteroidCollision(const std::string& asteroidId);
    void updateAsteroidPositions();
    void updatePlayerScore(const std::string& scoreUpdateData);
    void broadcastPlayerScore(const PlayerData& player);
    void broadcastBulletData(SOCKET socket);
    void updateBulletPositions();

    // Network message handling
    void processClientMessage(UdpClientMessage message);
    void sendPlayerStateToClients(SOCKET socket);
    void updateGameTimer(float deltaTime);
    void broadcastTimeUpdate();

    // Utility methods
    bool isGameActive;
    std::atomic<bool> shouldShutdown;  // Atomic flag for thread-safe shutdown checking
    float timeUpdateAccumulator;
    void cleanupResources();
};

int main() {
    std::string portNumber;
    std::cout << "Enter Server Port Number: ";
    std::getline(std::cin, portNumber);

    GameServer server;
    if (!server.initialize(portNumber)) {
        std::cerr << "Server initialization failed." << std::endl;
        return STATUS_WINSOCK_FAILURE;
    }

    server.run();
    return STATUS_SUCCESS;
}

/**
 * @brief Initializes the game server with the specified port
 * @param port The port number to listen on
 * @return true if initialization succeeded, false otherwise
 * @details Sets up Winsock, resolves server address, creates and binds socket
 */
bool GameServer::initialize(const std::string& port) {
    serverPort = port;

    if (!initializeWinsock()) return false;
    if (!resolveServerAddress(port)) return false;
    if (!createServerSocket()) return false;
    if (!bindServerSocket()) return false;

    players.clear();  // Clear players map if needed
    isGameActive = false;
    shouldShutdown = false;

    lastAsteroidCreationTime = std::chrono::steady_clock::now();
    lastAsteroidUpdateTime = std::chrono::steady_clock::now();
    lastBulletUpdateTime = std::chrono::steady_clock::now();
    timeUpdateAccumulator = 0.0f;

    return true;
}

/**
 * @brief Updates the game timer with the elapsed time
 * @param deltaTime Time elapsed since last update in seconds
 * @details Decrements the game timer and ensures it doesn't go below zero
 */
void GameServer::updateGameTimer(float deltaTime) {
    gameTimeRemaining -= deltaTime;
    if (gameTimeRemaining < 0.0f) {
        gameTimeRemaining = 0.0f;
    }
}

/**
 * @brief Broadcasts the current game time to all players
 * @details Sends a "TIME" message with remaining game time to all connected clients
 */
void GameServer::broadcastTimeUpdate() {
    std::string timeMessage = "TIME " + std::to_string(gameTimeRemaining) + "\n";

    for (auto& [_, player] : players) {
        sendto(serverSocket, timeMessage.c_str(), timeMessage.size(), 0,
            reinterpret_cast<sockaddr*>(&player.clientAddress), sizeof(player));
    }
}

/**
 * @brief Main server execution loop
 * @details Handles game timing, network messages, and game state updates
 */
void GameServer::run() {
    auto lastFrameTime = std::chrono::steady_clock::now();

    auto messageHandler = [this](UdpClientMessage message) {
        processClientMessage(message);
        return true;
    };

    const auto shutdownHandler = [&]() {
        closesocket(serverSocket);
        return true;
    };

    auto taskQueue = TaskQueue<UdpClientMessage, decltype(messageHandler), decltype(shutdownHandler)>{
        10, 20, messageHandler, shutdownHandler
    };

    while (!shouldShutdown) {
        auto currentTime = std::chrono::steady_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastFrameTime).count();
        lastFrameTime = currentTime;

        if (!isGameActive && players.size() >= 4) {
            isGameActive = true;
            std::cout << "Game Start" << std::endl;
        }

        if (isGameActive) {
            updateGameTimer(deltaTime);
            timeUpdateAccumulator += deltaTime;
            if (timeUpdateAccumulator >= 1.0f) {
                broadcastTimeUpdate();
                timeUpdateAccumulator = 0.0f;
            }
        }

        sockaddr_in clientAddress{};
        socklen_t clientAddressSize = sizeof(clientAddress);
        char receiveBuffer[MAX_BUFFER_SIZE];

        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration<float>(now - lastAsteroidCreationTime).count() > ASTEROID_SPAWN_INTERVAL) {
            spawnAsteroid();
            lastAsteroidCreationTime = now;
        }

        if (std::chrono::duration<float>(now - lastAsteroidUpdateTime).count() > ASTEROID_UPDATE_INTERVAL) {
            updateAsteroidPositions();
            lastAsteroidUpdateTime = now;
        }

        broadcastAsteroidData(serverSocket);
        updateBulletPositions();
        broadcastBulletData(serverSocket);

        int bytesReceived = recvfrom(serverSocket, receiveBuffer, sizeof(receiveBuffer), 0, (sockaddr*)&clientAddress, &clientAddressSize);

        if (bytesReceived == SOCKET_ERROR) {
            continue;
        }

        UdpClientMessage clientMessage;
        clientMessage.clientAddress = clientAddress;
        memcpy(clientMessage.buffer, receiveBuffer, bytesReceived);
        clientMessage.messageLength = bytesReceived;

        taskQueue.produce(clientMessage);
        sendPlayerStateToClients(serverSocket);
    }
    shutdown();
    shutdownHandler();
}

/**
 * @brief Creates a new asteroid in the game world
 * @details Generates a new asteroid with random position, velocity, and scale
 */
void GameServer::spawnAsteroid() {
    std::lock_guard<std::mutex> lock(asteroidsMutex);
    if (asteroids.size() >= MAX_ASTEROIDS) {
        return;
    }

    std::string asteroidId = "ast_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());

    float positionX, positionY;
    int spawnSide = rand() % 4;

    switch (spawnSide) {
    case 0: // Top
        positionX = (float)(rand() % ASTEROID_SPAWN_BOUND_MAX_X - ASTEROID_SPAWN_BOUND_MIN_X);
        positionY = ASTEROID_SPAWN_BOUND_MAX_Y;
        break;
    case 1: // Right
        positionX = ASTEROID_SPAWN_BOUND_MIN_X;
        positionY = (float)(rand() % ASTEROID_SPAWN_HBOUND_MAX_Y - ASTEROID_SPAWN_HBOUND_MIN_Y);
        break;
    case 2: // Bottom
        positionX = (float)(rand() % ASTEROID_SPAWN_BOUND_MAX_X - ASTEROID_SPAWN_BOUND_MIN_X);
        positionY = ASTEROID_SPAWN_BOUND_MIN_Y;
        break;
    default: // Left
        positionX = -ASTEROID_SPAWN_BOUND_MIN_X;
        positionY = (float)(rand() % ASTEROID_SPAWN_HBOUND_MAX_Y - ASTEROID_SPAWN_HBOUND_MIN_Y);
    }

    float speed = (float)(rand() % ASTEROID_SPEED_MIN + ASTEROID_SPEED_MIN) / 1000.0f;
    float directionX = 0.0f - positionX;
    float directionY = 0.0f - positionY;

    float length = sqrt(directionX * directionX + directionY * directionY);
    if (length > 0) {
        directionX /= length;
        directionY /= length;
    }

    float velocityX = directionX * speed;
    float velocityY = directionY * speed;

    float baseScale = (float)(rand() % (int)(MAX_ASTEROID_SCALE - MIN_ASTEROID_SCALE + 1) + MIN_ASTEROID_SCALE);
    float scaleX = baseScale * (0.8f + (float)(rand() % 40) / 100.0f);
    float scaleY = baseScale * (0.8f + (float)(rand() % 40) / 100.0f);

    AsteroidData newAsteroid;
    newAsteroid.id = asteroidId;
    newAsteroid.positionX = positionX;
    newAsteroid.positionY = positionY;
    newAsteroid.velocityX = velocityX;
    newAsteroid.velocityY = velocityY;
    newAsteroid.scaleX = scaleX;
    newAsteroid.scaleY = scaleY;
    newAsteroid.isActive = true;
    newAsteroid.creationTime = std::chrono::steady_clock::now();

    asteroids[asteroidId] = newAsteroid;
}

/**
 * @brief Updates positions of all active asteroids
 * @details Moves asteroids based on their velocity and handles boundary wrapping
 */
void GameServer::updateAsteroidPositions() {
    std::lock_guard<std::mutex> lock(asteroidsMutex);

    auto now = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(now - lastAsteroidUpdateTime).count();
    deltaTime = std::fmin(deltaTime, 0.1f); // Cap delta time

    for (auto& [id, asteroid] : asteroids) {
        if (!asteroid.isActive) continue;

        asteroid.positionX += asteroid.velocityX * deltaTime * 120.0f;
        asteroid.positionY += asteroid.velocityY * deltaTime * 120.0f;

        if (std::abs(asteroid.positionX) > 1200 || std::abs(asteroid.positionY) > 800) {
            int respawnSide = rand() % 4;
            switch (respawnSide) {
            case 0: // Top
                asteroid.positionX = (float)(rand() % 1600 - 800);
                asteroid.positionY = 400.0f;
                break;
            case 1: // Right
                asteroid.positionX = 800.0f;
                asteroid.positionY = (float)(rand() % 1200 - 600);
                break;
            case 2: // Bottom
                asteroid.positionX = (float)(rand() % 1600 - 800);
                asteroid.positionY = -400.0f;
                break;
            default: // Left
                asteroid.positionX = -800.0f;
                asteroid.positionY = (float)(rand() % 1200 - 600);
            }

            float speed = (float)(rand() % 90 + 90) / 1000.0f;
            float directionX = 0.0f - asteroid.positionX;
            float directionY = 0.0f - asteroid.positionY;

            float length = sqrt(directionX * directionX + directionY * directionY);
            if (length > 0) {
                directionX /= length;
                directionY /= length;
            }

            asteroid.velocityX = directionX * speed;
            asteroid.velocityY = directionY * speed;
        }
    }

    lastAsteroidUpdateTime = now;
}

/**
 * @brief Sends asteroid data to all connected clients
 * @param socket The network socket to use for sending data
 * @details Formats asteroid information into a network message and broadcasts it
 */
void GameServer::broadcastAsteroidData(SOCKET socket) {
    std::string asteroidData = "ASTEROIDS";

    {
        std::lock_guard<std::mutex> lock(asteroidsMutex);
        for (const auto& [id, asteroid] : asteroids) {
            if (!asteroid.isActive) continue;

            asteroidData += "|" + asteroid.id + "," +
                std::to_string(asteroid.positionX) + "," +
                std::to_string(asteroid.positionY) + "," +
                std::to_string(asteroid.velocityX) + "," +
                std::to_string(asteroid.velocityY) + "," +
                std::to_string(asteroid.scaleX) + "," +
                std::to_string(asteroid.scaleY) + "," +
                (asteroid.isActive ? "1" : "0");
        }
    }

    for (const auto& [_, player] : players) {
        sendto(socket, asteroidData.c_str(), asteroidData.size(), 0,
            (sockaddr*)&player.clientAddress, sizeof(player.clientAddress));
    }
}

/**
 * @brief Handles asteroid destruction
 * @param asteroidId The ID of the asteroid to destroy
 * @details Marks asteroid as inactive and notifies all clients
 */
void GameServer::handleAsteroidCollision(const std::string& asteroidId) {
    std::lock_guard<std::mutex> lock(asteroidsMutex);

    auto asteroid = asteroids.find(asteroidId);
    if (asteroid != asteroids.end()) {
        asteroid->second.isActive = false;

        std::string destructionMessage = "DESTROY_ASTEROID|" + asteroidId;
        for (const auto& [_, player] : players) {
            sendto(serverSocket, destructionMessage.c_str(), destructionMessage.size(), 0,
                (sockaddr*)&player.clientAddress, sizeof(player.clientAddress));
        }

        asteroids.erase(asteroid);
    }
}

/**
 * @brief Initializes Windows Sockets API (Winsock)
 * @return true if initialization succeeded, false otherwise
 * @details Must be called before any other socket operations
 */
bool GameServer::initializeWinsock() {
    WSADATA wsaData{};
    int result = WSAStartup(MAKEWORD(WINSOCK_MAJOR_VERSION, WINSOCK_MINOR_VERSION), &wsaData);
    if (result != NO_ERROR) {
        std::cerr << "WSAStartup failed with error: " << result << std::endl;
        return false;
    }
    return true;
}

/**
 * @brief Resolves the server's network address
 * @param port The port number to use
 * @return true if address resolution succeeded, false otherwise
 * @details Gets server IP and port information for binding
 */
bool GameServer::resolveServerAddress(const std::string& port) {
    addrinfo hints{};
    SecureZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = AI_PASSIVE;

    char hostname[MAX_STRING_LENGTH];
    gethostname(hostname, MAX_STRING_LENGTH);

    addrinfo* addressInfo = nullptr;
    int result = getaddrinfo(hostname, port.c_str(), &hints, &addressInfo);
    if (result != NO_ERROR || addressInfo == nullptr) {
        std::cerr << "getaddrinfo failed with error: " << result << std::endl;
        WSACleanup();
        return false;
    }

    char ipAddress[MAX_STRING_LENGTH];
    sockaddr_in* serverAddress = reinterpret_cast<sockaddr_in*>(addressInfo->ai_addr);
    inet_ntop(AF_INET, &(serverAddress->sin_addr), ipAddress, INET_ADDRSTRLEN);
    std::cout << "Server IP: " << ipAddress << std::endl;
    std::cout << "Server Port: " << port << std::endl;

    freeaddrinfo(addressInfo);
    return true;
}

/**
 * @brief Creates the server's UDP socket
 * @return true if socket creation succeeded, false otherwise
 * @details Creates a non-blocking UDP socket for server communications
 */
bool GameServer::createServerSocket() {
    serverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "socket creation failed with error: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return false;
    }
    return true;
}

/**
 * @brief Binds the server socket to the network address
 * @return true if binding succeeded, false otherwise
 * @details Associates the socket with the server's IP and port
 */
bool GameServer::bindServerSocket() {
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(std::stoi(serverPort));

    if (bind(serverSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
        std::cerr << "bind failed with error: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return false;
    }
    return true;
}

/**
 * @brief Processes incoming client messages
 * @param message The received UDP message and client information
 * @details Handles different message types (position updates, score changes, etc.)
 */
void GameServer::processClientMessage(UdpClientMessage message) {
    message.buffer[message.messageLength] = '\0';
    const char* messageData = message.buffer;
    sockaddr_in clientAddress = message.clientAddress;

    char clientIp[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddress.sin_addr, clientIp, sizeof(clientIp));
    int clientPort = ntohs(clientAddress.sin_port);

    float positionX, positionY, rotation;
    int score;
    std::string clientIpStr(clientIp);
    std::string clientKey = clientIpStr + ":" + std::to_string(clientPort);

    std::string messageStr(messageData);
    if (messageStr.find("DESTROY_ASTEROID|") == 0) {
        std::string asteroidId = messageStr.substr(17);
        handleAsteroidCollision(asteroidId);
        return;
    }

    if (messageStr.find("BULLET_CREATE ") == 0) {
        float bulletX, bulletY, bulletVelX, bulletVelY, bulletDir;
        std::string bulletId;

        std::istringstream iss(messageStr.substr(14));
        if (iss >> bulletX >> bulletY >> bulletVelX >> bulletVelY >> bulletDir >> bulletId) {
            BulletData bullet;
            bullet.id = bulletId;
            bullet.positionX = bulletX;
            bullet.positionY = bulletY;
            bullet.velocityX = bulletVelX;
            bullet.velocityY = bulletVelY;
            bullet.direction = bulletDir;
            bullet.creationTime = std::chrono::steady_clock::now();

            bullets[bulletId] = bullet;
        }
        return;
    }

    if (messageStr.find("UPDATE_SCORE|") == 0) {
        std::string scoreUpdateData = messageStr.substr(13);
        updatePlayerScore(scoreUpdateData);
        return;
    }

    if (sscanf_s(messageData, "%f %f %f %d", &positionX, &positionY, &rotation, &score) != 4) {
        std::cerr << "Invalid message format: " << messageData << std::endl;
        return;
    }

    bool isLocalClient = (clientIpStr == "127.0.0.1" || clientIpStr == "::1");

    auto playerIt = players.find(clientKey);
    if (playerIt != players.end()) {
        playerIt->second.positionX = positionX;
        playerIt->second.positionY = positionY;
        playerIt->second.rotation = rotation;
        playerIt->second.score = std::fmax(score, playerIt->second.score);
        playerIt->second.lastActivityTime = std::chrono::steady_clock::now();
    }
    else {
        int playerId = -1;
        int savedScore = 0;

        if (!isLocalClient) {
            for (const auto& entry : players) {
                size_t colonPos = entry.first.find(':');
                std::string entryIp = entry.first.substr(0, colonPos);

                if (entryIp == clientIpStr) {
                    playerId = std::stoi(entry.second.id);
                    savedScore = entry.second.score;
                    ipToPlayerIdMap[clientIpStr] = playerId;
                    break;
                }
            }

            if (playerId == -1 && ipToPlayerIdMap.find(clientIpStr) != ipToPlayerIdMap.end()) {
                playerId = ipToPlayerIdMap[clientIpStr];
            }
        }

        if (playerId == -1) {
            static int nextPlayerId = 1;
            playerId = nextPlayerId++;

            if (!isLocalClient) {
                ipToPlayerIdMap[clientIpStr] = playerId;
            }
        }

        PlayerData newPlayer;
        newPlayer.id = std::to_string(playerId);
        newPlayer.positionX = positionX;
        newPlayer.positionY = positionY;
        newPlayer.rotation = rotation;
        newPlayer.score = std::fmax(savedScore, score);
        newPlayer.clientAddress = clientAddress;
        newPlayer.ipAddress = clientIp;
        newPlayer.lastActivityTime = std::chrono::steady_clock::now();

        players[clientKey] = newPlayer;

        sendto(serverSocket, newPlayer.id.c_str(), newPlayer.id.size(), 0,
            (sockaddr*)&newPlayer.clientAddress, sizeof(newPlayer));

        std::string scoreUpdate = "SCORE_UPDATE|" + newPlayer.id + " " + std::to_string(newPlayer.score);
        for (const auto& [_, player] : players) {
            sendto(serverSocket, scoreUpdate.c_str(), scoreUpdate.size(), 0,
                (sockaddr*)&player.clientAddress, sizeof(player.clientAddress));
        }
    }
}

/**
 * @brief Sends current player states to all clients
 * @param socket The network socket to use for sending
 * @details Broadcasts position, rotation and score of all players
 */
void GameServer::sendPlayerStateToClients(SOCKET socket) {
    std::string playerData;

    for (const auto& [playerId, player] : players) {
        playerData += player.id + " " +
            std::to_string(player.positionX) + " " +
            std::to_string(player.positionY) + " " +
            std::to_string(player.rotation) + " " +
            std::to_string(player.score) + " " +
            player.ipAddress + "\n";
    }

    for (const auto& [_, player] : players) {
        sendto(socket, playerData.c_str(), playerData.size(), 0,
            (sockaddr*)&player.clientAddress, sizeof(player));
    }
}

/**
 * @brief Broadcasts bullet data to all clients
 * @param socket The network socket to use for sending
 * @details Sends position and velocity of all active bullets
 */
void GameServer::broadcastBulletData(SOCKET socket) {
    if (bullets.empty()) return;

    std::string bulletData = "BULLETS";

    for (const auto& [id, bullet] : bullets) {
        bulletData += "|" + bullet.id + "," +
            std::to_string(bullet.positionX) + "," +
            std::to_string(bullet.positionY) + "," +
            std::to_string(bullet.velocityX) + "," +
            std::to_string(bullet.velocityY) + "," +
            std::to_string(bullet.direction);
    }

    for (const auto& [_, player] : players) {
        sendto(socket, bulletData.c_str(), bulletData.size(), 0,
            (sockaddr*)&player.clientAddress, sizeof(player.clientAddress));
    }
}

/**
 * @brief Updates positions of all active bullets
 * @details Moves bullets based on velocity and removes expired ones
 */
void GameServer::updateBulletPositions() {
    auto now = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(now - lastBulletUpdateTime).count();
    deltaTime = std::fmin(deltaTime, 0.1f);

    for (auto it = bullets.begin(); it != bullets.end();) {
        BulletData& bullet = it->second;
        bullet.positionX += bullet.velocityX * deltaTime;
        bullet.positionY += bullet.velocityY * deltaTime;

        auto bulletAge = std::chrono::duration<float>(now - bullet.creationTime).count();
        if (bulletAge > BULLET_LIFETIME_SECONDS) {
            it = bullets.erase(it);
        }
        else {
            ++it;
        }
    }

    lastBulletUpdateTime = now;
}

/**
 * @brief Updates a player's score
 * @param scoreUpdateData String containing player ID and new score
 * @details Parses score update message and broadcasts changes
 */
void GameServer::updatePlayerScore(const std::string& scoreUpdateData) {
    std::istringstream iss(scoreUpdateData);
    std::string playerId;
    int newScore;

    iss >> playerId >> newScore;

    auto player = players.find(playerId);
    if (player != players.end()) {
        player->second.score = newScore;
        broadcastPlayerScore(player->second);
    }
}

/**
 * @brief Broadcasts a player's score to all clients
 * @param player The player whose score to broadcast
 * @details Sends score update message to maintain synchronization
 */
void GameServer::broadcastPlayerScore(const PlayerData& player) {
    std::string scoreMessage = "SCORE_UPDATE|" + player.id + " " + std::to_string(player.score);

    for (const auto& [_, player] : players) {
        sendto(serverSocket, scoreMessage.c_str(), scoreMessage.size(), 0,
            (sockaddr*)&player.clientAddress, sizeof(player.clientAddress));
    }
}

/**
 * @brief Cleans up server resources
 * @details Closes sockets and performs Winsock cleanup
 */
void GameServer::cleanupResources() {

    {
        std::lock_guard<std::mutex> lock(asteroidsMutex);
        asteroids.clear();
    }
    bullets.clear();
    players.clear();

    if (serverSocket != INVALID_SOCKET) {
        closesocket(serverSocket);
        serverSocket = INVALID_SOCKET;
    }

    WSACleanup();
}

void GameServer::shutdown() {
    shouldShutdown = true;

    // Notify all connected clients
    std::string shutdownMsg = "SERVER_SHUTDOWN";
    std::lock_guard<std::mutex> lock(clientsMutex);

    for (const auto& [_, player] : players) {
        sendto(serverSocket, shutdownMsg.c_str(), shutdownMsg.size(), 0, (sockaddr*)&player.clientAddress, sizeof(player.clientAddress));
    }
}