#pragma once

#include <unordered_map>
#include <string>

struct playerData {
	std::string playerID;
	float x, y, rot;		// Position
	sockaddr_in cAddr;
	int score;
	std::string cIP; // Client IP as a string
	std::chrono::steady_clock::time_point lastActive; // Last time the player sent data
};

struct bulletData {
	std::string bulletID;
	float x, y, dir;
};

struct asteroidData {
	std::string asteroidID;
	float x, y;              // Position
	float velX, velY;        // Velocity
	float scaleX, scaleY;    // Scale
	bool active;             // Whether the asteroid is active
	std::chrono::steady_clock::time_point creationTime; // When the asteroid was created
};

std::unordered_map<std::string, playerData> players;
std::unordered_map<std::string, bulletData> bullets;
std::unordered_map<std::string, asteroidData> asteroids;
std::mutex asteroidsMutex; // For thread-safe access