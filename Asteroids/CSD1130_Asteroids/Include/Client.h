/* Start Header ************************************************************************/
/*!
\file Client.h
\co author Ho Jing Rui
\co author Saminathan Aaron Nicholas
\co author Jay Lim Jun Xiang
\par emails: jingrui.ho@digipen.edu
\	         s.aaronnicholas@digipen.edu
\	         jayjunxiang.lim@digipen.edu
\date 28 March, 2025
\brief Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/* End Header **************************************************************************/
#pragma once

// Windows headers configuration
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define NOMINMAX

// System includes (grouped and ordered logically)
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

// Engine includes
#include "AEEngine.h"

// Standard library includes
#include <atomic>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/* ======================= DATA STRUCTURES ======================= */

/**
 * @brief Structure for UDP communication data
 */
struct UdpClientData {
    sockaddr_in clientAddr;  ///< Client address information
    char data[1024];        ///< Data buffer
    int dataSize;           ///< Size of received data
};

/**
 * @brief Player data structure
 */
struct PlayerData {
    int playerID;           ///< Unique player identifier
    float X, Y, rotation;       ///< Position and rotation
    std::string clientIP;       ///< Client IP address
    int score;             ///< Current game score
    static float gameTimer; ///< Shared game timer
};

/**
 * @brief Bullet data structure
 */
struct BulletData {
    std::string bulletID;      ///< Unique bullet identifier
    float X, Y;                     ///< Current position
    float velocityX, velocityY;         ///< Velocity components
    float direction;                ///< Movement direction
    bool fromLocalPlayer;     ///< Flag for locally created bullets
};

/**
 * @brief Asteroid data structure with interpolation support
 */
struct AsteroidData {
    int asteroidID;                          ///< Unique asteroid identifier
    float X, Y;                             ///< Server-reported position
    float velocityX, velocityY;                       ///< Velocity components
    float scaleX, scaleY;                   ///< Size scaling factors
    bool isActive;                            ///< Active state flag
    float targetX, targetY;                 ///< Target position from server
    float currentX, currentY;               ///< Interpolated render position
    std::chrono::steady_clock::time_point lastUpdateTime; ///< Last update timestamp
    std::chrono::steady_clock::time_point creationTime;   ///< Creation timestamp
};

// Global game state declarations
extern std::unordered_map<int, PlayerData> players;
extern std::unordered_map<std::string, BulletData> bullets;
extern std::unordered_map<std::string, AsteroidData> asteroids;
extern std::mutex asteroidsMutex;

/* ======================= CLIENT CLASS ======================= */

/**
 * @brief Network client for multiplayer game
 *
 * Handles all network communication with the game server,
 * manages player data synchronization, and provides thread-safe
 * access to game state.
 */
class Client {
public:
    Client() = default;
    ~Client() { cleanup(); }

    /* ========== PUBLIC INTERFACE ========== */

    // Initialization
    bool initialize(const std::string& serverIP, uint16_t serverPort);

    // Runtime control
    void run();
    void runScript(const std::string& scriptPath);

    // Server information
    void getServerInfo(const std::string& scriptPath, std::string& IP, std::string& port);

    // Network operations
    void sendToServerUdp();
    void reportBulletCreation(const AEVec2& pos, const AEVec2& vel, float dir,
        const std::string& bulletID = "");
    void reportAsteroidDestruction(const std::string& asteroidID);
    void reportPlayerScore(const std::string& playerID, int score);

    // Thread synchronization
    static void lockBullets() { bulletsMutex.lock(); }
    static void unlockBullets() { bulletsMutex.unlock(); }

    // Accessors
    static void displayPlayerScores();
    static int getPlayerCount() { return playerCount; }
    static int getPlayerID() { return playerID; }
    SOCKET getSocket() const { return clientSocket; }

private:
    /* ========== PRIVATE IMPLEMENTATION ========== */

    // Network setup
    bool setupWinsock();
    bool resolveAddress(const std::string& serverIP, uint16_t serverPort);
    bool createSocket();
    bool connectToServer();

    // Network handling
    void handleNetwork();
    void cleanup();

    // Data processing helpers
    void processAsteroidData(const std::string& data);
    void processBulletData(const std::string& data);
    void processScoreUpdate(const std::string& data);
    void processPlayerData(const std::string& data);
    void sendServerMessage(const std::string& message);

    /* ========== MEMBER VARIABLES ========== */
    SOCKET clientSocket = INVALID_SOCKET;  ///< Network socket handle
    std::string serverIP;                  ///< Server IP address
    uint16_t serverPort;                   ///< Server port number

    // Static members
    static std::mutex playersMutex;               ///< General-purpose mutex
    static std::mutex bulletsMutex;        ///< Bullet data mutex
    static size_t playerCount;                  ///< Player count
    static int playerID;                   ///< This client's player ID
};