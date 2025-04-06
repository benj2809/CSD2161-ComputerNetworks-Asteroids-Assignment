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
#include <functional>
#include <vector>

constexpr char ASTEROID_PREFIX[] = "ASTEROIDS";
constexpr char BULLET_PREFIX[] = "BULLETS";
constexpr char SCORE_UPDATE_PREFIX[] = "SCORE_UPDATE";

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
    int playerID;              ///< Unique player identifier
    float X, Y, rotation;      ///< Position and rotation
    std::string clientIP;      ///< Client IP address
    int score;                 ///< Current game score
};

/**
 * @brief Bullet data structure
 */
struct BulletData {
    std::string bulletID;       ///< Unique bullet identifier
    float X, Y;                 ///< Current position
    float velocityX, velocityY; ///< Velocity components
    float direction;            ///< Movement direction
    bool fromLocalPlayer;       ///< Flag for locally created bullets
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

/* ======================= CLIENT CLASS ======================= */

/**
 * @brief Network client for multiplayer game
 *
 * Handles all network communication with the game server,
 * manages player data synchronization, and provides thread-safe
 * access to game state.
 */
class GameClient {
public:
    GameClient() = default;
    ~GameClient() { cleanup(); }

    /* ========== PUBLIC INTERFACE ========== */

    // Initialization
    bool initialize(const std::string& serverIPAddress, uint16_t serverPortNumber);

    // Runtime control
    void run();
    void runScript();

    // Server information
    void getServerInfo(const std::string& scriptDirectory, std::string& IP, std::string& port);

    // Network operations
    void sendData();
    void sendBulletCreationEvent(const AEVec2& pos, const AEVec2& vel, float dir, const std::string& bulletID = "");
    void sendAsteroidDestructionEvent(const std::string& asteroidID);
    void sendScoreUpdateEvent(const std::string& pid, int score);

    // Thread synchronization
    struct ScopedPlayerLock {
        ScopedPlayerLock() { playersMutex.lock(); }
        ~ScopedPlayerLock() { playersMutex.unlock(); }
    };
    struct ScopedBulletLock {
        ScopedBulletLock() { bulletsMutex.lock(); }
        ~ScopedBulletLock() { bulletsMutex.unlock(); }
    };
    struct ScopedAsteroidLock {
        ScopedAsteroidLock() { asteroidsMutex.lock(); }
        ~ScopedAsteroidLock() { asteroidsMutex.unlock(); }
    };

    // Accessors
    static void displayPlayerScores();
    static int getPlayerCount() { return static_cast<int>(playerCount); }
    static int getPlayerID() { return playerID; }
    SOCKET getSocket() const { return clientSocket; }

    void cleanup() noexcept;

private:
    /* ========== PRIVATE IMPLEMENTATION ========== */

    // Network setup
    struct WinsockManager {
        WinsockManager() {
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != NO_ERROR) {
                throw std::runtime_error("WSAStartup failed");
            }
        }
        ~WinsockManager() {
            WSACleanup();
        }
        WSADATA wsaData;
    };
    std::unique_ptr<WinsockManager> winsock;
    bool setupWinsock();
    bool winsockInitialized = false;
    bool resolveAddress(const std::string& serverIPAddress, uint16_t serverPortNumber);
    bool createSocket();
    bool connectToServer();

    // Network handling
    void receiveNetworkMessages();

    // Data processing helpers
    void updateAsteroidsFromNetwork(const std::string& data);
    void updateBulletsFromNetwork(const std::string& data);
    void updateScoresFromNetwork(const std::string& data);
    void updatePlayersFromNetwork(const std::string& data);
    void sendRawMessage(const std::string& message);

    /* ========== MEMBER VARIABLES ========== */
    SOCKET clientSocket = INVALID_SOCKET;  ///< Network socket handle
    std::string serverIP;                  ///< Server IP address
    uint16_t serverPort;                   ///< Server port number

    sockaddr_in serverSockAddr;            ///< Server address for UDP communication

    // Static members
    static std::mutex playersMutex;        ///< Plauer data mutex
    static std::mutex bulletsMutex;        ///< Bullet data mutex
    static std::mutex asteroidsMutex;      ///< Asteroid data mutex
    static size_t playerCount;             ///< Player count
    static int playerID;                   ///< This client's player ID
};