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
\brief This file declares functions
bool CollisionIntersection_RectRect(const AABB & aabb1,          //Input
                                    const AEVec2 & vel1,         //Input
                                    const AABB & aabb2,          //Input
                                    const AEVec2 & vel2,         //Input
                                    float& firstTimeOfCollision);

Copyright (C) 2024 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/* End Header **************************************************************************/
#pragma once

// Critical defines MUST come first
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define NOMINMAX

// EXACT inclusion order
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>  // Must come after Winsock2
#pragma comment(lib, "ws2_32.lib")

#include "AEEngine.h" // Include for AEVec2 type

// 3. Standard C++ headers
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <unordered_map>

// Return code constants for different exit conditions
constexpr int RETURN_CODE_1 = 1; // General failure
constexpr int RETURN_CODE_2 = 2; // Unused in current implementation
constexpr int RETURN_CODE_3 = 3; // Unused in current implementation
constexpr int RETURN_CODE_4 = 4; // Command format error

// Command ID enumeration - protocol definition for client-server communication
enum CMDID : unsigned char {
    UNKNOWN = 0x0,          // Unknown command
    REQ_QUIT = 0x1,         // Request to quit/disconnect
    REQ_ECHO = 0x2,         // Request to echo a message
    RSP_ECHO = 0x3,         // Response to an echo request
    REQ_LISTUSERS = 0x4,    // Request to list connected users
    RSP_LISTUSERS = 0x5,    // Response with list of users
    CMD_TEST = 0x20,        // Test command
    ECHO_ERROR = 0x30       // Error in echo operation
};
struct UdpClientData {
    sockaddr_in clientAddr;
    char data[1024];  // Buffer for incoming data
    int dataSize;     // Size of the incoming data
};
/**
 * Client class that encapsulates all client functionality.
 * Manages socket connection, network I/O, and user interaction.
 */
class Client {
public:
    // Default constructor
    Client() = default;

    // Destructor - ensures proper cleanup of resources
    ~Client() { cleanup(); }

    /**
     * Initialize the client with server connection details
     * @param serverIP The IP address of the server to connect to
     * @param serverPort The port number of the server
     * @return true if initialization successful, false otherwise
     */
    bool initialize(const std::string& serverIP, uint16_t serverPort);

    /**
     * Run the client in interactive mode - handles user input and network operations
     */
    void run();

    /**
     * Run the client in script mode - reads commands from a file
     * @param scriptPath Path to the script file containing commands
     */
    void runScript(const std::string& scriptPath);
    void getServerInfo(const std::string& scriptPath, std::string& IP, std::string& port);
    void sendToServerUdp();

    static void displayPlayerScores();

    // Get player count
    static const int getPlayerCount() { return pCount; }

    // Get player ID for this client
    static const int getPlayerID() { return playerID; }
    SOCKET getSocket() { return clientSocket; }
    // Get bullets data safely with locking
    static void lockBullets() { bulletsMutex.lock(); }
    static void unlockBullets() { bulletsMutex.unlock(); }

    void reportBulletCreation(const AEVec2& pos, const AEVec2& vel, float dir, const std::string& bulletID = "");
    void reportAsteroidDestruction(const std::string& asteroidID);
    void reportPlayerScore(const std::string& playerID, int score);
    void updateAsteroidInterpolation();

private:
    SOCKET clientSocket = INVALID_SOCKET;  // Socket handle for server connection
    static std::mutex mutex;                      // Mutex for thread synchronization
    std::string serverIP;                  // Server IP address
    uint16_t serverPort;                   // Server port number

    static std::mutex bulletsMutex;  // Mutex for thread-safe access to bullets

    // Player count
    static size_t pCount;

    // Player ID
    static int playerID;

    /**
     * Initialize the Winsock library
     * @return true if successful, false otherwise
     */
    bool setupWinsock();

    /**
     * Resolve server address using DNS
     * @param serverIP Server IP address
     * @param serverPort Server port number
     * @return true if address resolution successful, false otherwise
     */
    bool resolveAddress(const std::string& serverIP, uint16_t serverPort);

    /**
     * Create a TCP socket
     * @return true if socket creation successful, false otherwise
     */
    bool createSocket();

    /**
     * Connect to the server using the stored IP and port
     * @return true if connection successful, false otherwise
     */
    bool connectToServer();

    /**
     * Thread function to handle network operations
     * Continuously receives and processes data from the server
     */
    void handleNetwork();

    /**
     * Clean up resources (sockets, Winsock) on exit
     */
    void cleanup();
};

struct playerData {
    int playerID;
    float x, y, rot;		// Position
    std::string cIP;
    int score;
    static float gameTimer;
    // std::chrono::steady_clock::time_point lastActive; // Last time the player sent data
};

struct bulletData {
    std::string bulletID;
    float x, y;              // Position
    float velX, velY;        // Velocity
    float dir;               // Direction
    bool fromLocalPlayer;    // Flag to identify locally created bullets
};

struct asteroidData {
    int asteroidID;
    float x, y;          // Position
    float velX, velY;    // Velocity
    float scaleX, scaleY;// Scale
    bool active;         // Whether the asteroid is active
    float targetX, targetY;  // Target position from server 
    float currentX, currentY;// Interpolated position for rendering
    std::chrono::steady_clock::time_point lastUpdateTime; // Last update time
    std::chrono::steady_clock::time_point creationTime;   // When the asteroid was created
};


extern std::unordered_map<int, playerData> players;
extern std::unordered_map<std::string, bulletData> bullets;
extern std::unordered_map<std::string, asteroidData> asteroids;
extern std::mutex asteroidsMutex;