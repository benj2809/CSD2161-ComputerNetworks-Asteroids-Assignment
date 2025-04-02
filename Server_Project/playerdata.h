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

std::unordered_map<std::string, playerData> players;
std::unordered_map<std::string, bulletData> bullets;