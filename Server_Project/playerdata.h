#pragma once

#include <unordered_map>
#include <string>

struct playerData {
	std::string playerID;
	float x, y;		// Position
	sockaddr_in cAddr;
	std::string cIP; // Client IP as a string
	std::chrono::steady_clock::time_point lastActive; // Last time the player sent data
};

std::unordered_map<std::string, playerData> players;
