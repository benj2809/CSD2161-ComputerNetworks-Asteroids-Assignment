#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <unordered_map>
#include <string>
#include <chrono> // Include the chrono header
struct playerData {
	std::string playerID;
	float x, y, rot;		// Position
	sockaddr_in cAddr;
	std::string cIP; // Client IP as a string
	std::chrono::steady_clock::time_point lastActive; // Last time the player sent data
};

struct aestroids {
	int id;
	float x, y, rot;
};
struct allAestroids
{
	int numAestroids;
	void setAestroids(int numAestroids);
	void applyMovementVector(float dx, float dy);
	std::vector<aestroids> aestroid;
};
extern std::unordered_map<std::string, playerData> players;

