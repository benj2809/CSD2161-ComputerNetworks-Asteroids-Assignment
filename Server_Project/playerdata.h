#pragma once

#include <unordered_map>
#include <string>
#include <chrono>
#include <winsock2.h>
#include <ws2tcpip.h>

/**
 * @struct PlayerData
 * @brief Contains all data related to a connected player
 */
struct PlayerData {
    std::string id;         // Unique player identifier
    float positionX;        // X coordinate in game world
    float positionY;        // Y coordinate in game world
    float rotation;         // Player's rotation angle
    sockaddr_in clientAddress; // Network address information
    int score;              // Player's current score
    std::string ipAddress;  // Client's IP address as string
    std::chrono::steady_clock::time_point lastActivityTime; // Last time player was active
};

/**
 * @struct BulletData
 * @brief Contains data for active bullets in the game
 */
struct BulletData {
    std::string id;         // Unique bullet identifier
    float positionX;        // X coordinate in game world
    float positionY;        // Y coordinate in game world
    float velocityX;        // X velocity component
    float velocityY;        // Y velocity component
    float direction;        // Direction of movement
    std::chrono::steady_clock::time_point creationTime; // When bullet was fired
};

/**
 * @struct AsteroidData
 * @brief Contains data for asteroids in the game
 */
struct AsteroidData {
    std::string id;         // Unique asteroid identifier
    float positionX;        // X coordinate in game world
    float positionY;        // Y coordinate in game world
    float velocityX;        // X velocity component
    float velocityY;        // Y velocity component
    float scaleX;           // Horizontal scale/size
    float scaleY;           // Vertical scale/size
    bool isActive;          // Whether asteroid is active/destroyed
    std::chrono::steady_clock::time_point creationTime; // When asteroid was created
};

// Global game state containers
extern std::unordered_map<std::string, PlayerData> players;
extern std::unordered_map<std::string, BulletData> bullets;
extern std::unordered_map<std::string, AsteroidData> asteroids;
extern std::mutex asteroidsMutex; // Protects access to asteroids