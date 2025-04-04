/* Start Header
*******************************************************************/
/*!
\file playerdata.h
\author Ho Jing Rui
\author Saminathan Aaron Nicholas
\author Jay Lim Jun Xiang
\par emails: jingrui.ho@digipen.edu
\	         s.aaronnicholas@digipen.edu
\	         jayjunxiang.lim@digipen.edu
\date 28 March, 2025
\brief This file contains the data structures for:
\      - Player information (position, score, connection details)
\      - Bullet tracking (position, velocity, lifetime)
\      - Asteroid properties (position, scale, movement)
\
\      Global Containers:
\      - players: Map of all connected players (key: "IP:Port")
\      - bullets: Map of active bullets (key: bulletID)
\      - asteroids: Map of active asteroids (key: asteroidID)
\
\      Thread Safety:
\      - asteroidsMutex protects asteroid data access
\
\      Data Structures:
\      - PlayerData: Tracks player state and network info
\      - BulletData: Manages bullet physics and lifetime
\      - AsteroidData: Handles asteroid properties and state
\
Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
*******************************************************************/
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