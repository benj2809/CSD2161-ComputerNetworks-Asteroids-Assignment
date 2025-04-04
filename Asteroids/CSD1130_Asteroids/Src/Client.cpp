/* Start Header
*******************************************************************/
/*!
\file Client.cpp
\co author Ho Jing Rui
\co author Saminathan Aaron Nicholas
\co author Jay Lim Jun Xiang
\par emails: jingrui.ho@digipen.edu
\	         s.aaronnicholas@digipen.edu
\	         jayjunxiang.lim@digipen.edu
\date 28 March, 2025
\brief Copyright (C) 2025 DigiPen Institute of Technology.

Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
*******************************************************************/

#include "Client.h"
#include "AEEngine.h"
#include "GameState_Asteroids.h"

size_t Client::pCount;

int Client::playerID = -1; // Store client's player ID

std::unordered_map<int, playerData> players;
std::mutex Client::mutex;

std::unordered_map<std::string, asteroidData> asteroids;
std::mutex asteroidsMutex;

std::mutex Client::bulletsMutex;  // Definition of the static mutex
std::unordered_map<std::string, bulletData> bullets;  // Global bullets map

/**
 * @brief Initializes the client with server connection details.
 *
 * @param serverIP The IP address of the server to connect to.
 * @param serverPort The port number of the server.
 * @return true if initialization is successful, false otherwise.
 */
bool Client::initialize(const std::string& serverIP, uint16_t serverPort) {
    this->serverIP = serverIP;
    this->serverPort = serverPort;
    pCount = 0;

    // Perform initialization steps in sequence
    if (!setupWinsock()) return false;
    if (!resolveAddress(serverIP, serverPort)) return false;
    if (!createSocket()) return false;

    return true;
}

/**
 * @brief Runs the client in interactive mode.
 *
 * This function starts a network thread for handling communication
 * and processes user input on the main thread.
 */
void Client::run() {
    // Send an initial message to the server
    sendToServerUdp();

    // Start network handling in a separate thread
    std::thread networkThread(&Client::handleNetwork, this);
    networkThread.detach();

    // Handle user input (currently disabled)
    // handleUserInput();
}

/**
 * @brief Runs the client in script mode.
 *
 * This function reads and executes commands from a script file.
 *
 * @param scriptPath Path to the script file containing commands.
 */
void Client::runScript(const std::string& scriptPath) {
    // Start network handling in a separate thread
    std::thread networkThread(&Client::handleNetwork, this);
    networkThread.detach();

    // Process script commands (currently disabled)
    // handleScript(scriptPath);

    // Send an initial message to the server
    sendToServerUdp();
}

/**
 * @brief Retrieves server information (IP and port) from a script file.
 *
 * @param scriptPath Path to the script file.
 * @param IP Reference to a string to store the extracted IP address.
 * @param port Reference to a string to store the extracted port number.
 */
void Client::getServerInfo(const std::string& scriptPath, std::string& IP, std::string& port) {
    std::ifstream file(scriptPath);
    if (!file) {
        std::cerr << "Error: Could not open file: " << scriptPath << std::endl;
        return;
    }

    std::getline(file, IP);
    std::getline(file, port);

    file.close();
}

/**
 * @brief Sends player data (position, rotation, score) to the server via UDP.
 */
void Client::sendToServerUdp() {
    // Retrieve player data
    AEVec2 position = returnPlayerPosition();
    float rot = returnPlayerRotation();
    int score = returnPlayerScore();

    // Construct message
    std::string message = std::to_string(position.x) + " " +
        std::to_string(position.y) + " " +
        std::to_string(rot) + " " +
        std::to_string(score);

    // Prepare UDP message structure
    UdpClientData udpMessage;
    ZeroMemory(&udpMessage, sizeof(udpMessage));

    // Copy message into the structure
    errno_t err = strncpy_s(udpMessage.data, sizeof(udpMessage.data), message.c_str(), message.length());
    if (err != 0) {
        std::cerr << "Error copying message to buffer." << std::endl;
        return;
    }

    // Setup address hints
    addrinfo hints{};
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    // Resolve server address
    addrinfo* result = nullptr;
    if (getaddrinfo(serverIP.c_str(), std::to_string(serverPort).c_str(), &hints, &result) != 0) {
        std::cerr << "getaddrinfo failed." << std::endl;
        return;
    }

    // Store resolved address in the message structure
    udpMessage.clientAddr = *reinterpret_cast<sockaddr_in*>(result->ai_addr);

    // Send data to the server
    int sendResult = sendto(clientSocket, udpMessage.data, strlen(udpMessage.data), 0,
        reinterpret_cast<sockaddr*>(&udpMessage.clientAddr), sizeof(udpMessage.clientAddr));

    if (sendResult == SOCKET_ERROR) {
        std::cerr << "Send failed with error: " << WSAGetLastError() << std::endl;
    }

    // Cleanup
    freeaddrinfo(result);
}

/**
 * @brief Displays the current player scores.
 *
 * This function prints the scores and positions of all players in a formatted table.
 */
void Client::displayPlayerScores() {
    std::lock_guard<std::mutex> lock(mutex);

    std::cout << "\n================= PLAYER SCORES ==================" << std::endl;
    std::cout << "Player ID          Score          Position" << std::endl;
    std::cout << "--------------------------------------------------" << std::endl;

    for (const auto& pair : players) {
        int id = pair.first;
        const playerData& player = pair.second;

        std::cout << id << "                  "
            << player.score << "              "
            << "(" << std::fixed << std::setprecision(1) << player.x
            << ", " << std::fixed << std::setprecision(1) << player.y << ")"
            << (id == playerID ? " [YOU]" : "") << std::endl;
    }

    std::cout << "--------------------------------------------------" << std::endl;
}

/**
 * @brief Initializes the Winsock library.
 *
 * @return true if Winsock is successfully initialized, false otherwise.
 */
bool Client::setupWinsock() {
    WSADATA wsaData{};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != NO_ERROR) {
        std::cerr << "WSAStartup failed." << std::endl;
        return false;
    }
    return true;
}

/**
 * @brief Resolves the server address using DNS.
 *
 * @param serverIP The IP address of the server.
 * @param serverPort The port number of the server.
 * @return true if address resolution is successful, false otherwise.
 */
bool Client::resolveAddress(const std::string& serverIP, uint16_t serverPort) {
    addrinfo hints{};
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    addrinfo* result = nullptr;
    if (getaddrinfo(serverIP.c_str(), std::to_string(serverPort).c_str(), &hints, &result) != 0) {
        std::cerr << "getaddrinfo failed." << std::endl;
        WSACleanup();
        return false;
    }

    freeaddrinfo(result);
    return true;
}

/**
 * @brief Creates a UDP socket for communication.
 *
 * @return true if the socket is successfully created, false otherwise.
 */
bool Client::createSocket() {
    clientSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed." << std::endl;
        WSACleanup();
        return false;
    }
    return true;
}

/**
 * @brief Connects the client to the server using the stored IP and port.
 *
 * This function initializes a UDP socket and retrieves the server's address information.
 * Since UDP is connectionless, there is no explicit connection step.
 *
 * @return true if the server address information is retrieved successfully, false otherwise.
 */
bool Client::connectToServer() {
    // Setup address hints structure (same as before)
    addrinfo hints{};
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;        // IPv4
    hints.ai_socktype = SOCK_DGRAM;  // UDP (use SOCK_DGRAM instead of SOCK_STREAM)
    hints.ai_protocol = IPPROTO_UDP;  // UDP protocol

    // Get address information
    addrinfo* result = nullptr;
    if (getaddrinfo(serverIP.c_str(), std::to_string(serverPort).c_str(), &hints, &result) != 0) {
        std::cerr << "getaddrinfo failed." << std::endl;
        WSACleanup();
        return false;
    }

    // Normally no need for connect() in UDP, so we skip it
    // But we may want to store the server's address for later communication
    //serverAddr = *reinterpret_cast<sockaddr_in*>(result->ai_addr);

    freeaddrinfo(result);  // Free the address info as it's no longer needed
    return true;
}


/**
 * @brief Handles incoming network messages in a separate thread.
 *
 * This function continuously listens for messages from the server, processes game updates,
 * and updates asteroid, bullet, and player data accordingly. It also processes score updates.
 */
void Client::handleNetwork() {
    std::vector<uint8_t> recvQueue;
    sockaddr_in serverAddr;
    int addrSize = sizeof(serverAddr);

    while (true) {
        char recvBuffer[1024];
        int receivedBytes = recvfrom(clientSocket, recvBuffer, sizeof(recvBuffer), 0,
            (sockaddr*)&serverAddr, &addrSize);

        if (receivedBytes <= 0) continue;

        // Null-terminate the received data
        recvBuffer[receivedBytes] = '\0';
        std::string receivedData(recvBuffer, receivedBytes);

        // Check if this is player ID assignment
        if (receivedBytes <= 3 && playerID == -1) {
            playerID = std::stoi(recvBuffer);
            std::cout << "Assigned Player ID: " << playerID << std::endl;
            continue;
        }

        // Check if this is asteroid data (format: "ASTEROIDS|id1,x1,y1,velX1,velY1,scaleX1,scaleY1,active1|id2,...")
        if (receivedData.find("ASTEROIDS") == 0) {
            std::lock_guard<std::mutex> lock(asteroidsMutex);

            // Store current time for interpolation
            auto currentTime = std::chrono::steady_clock::now();

            // Keep track of updated asteroid IDs
            std::vector<std::string> updatedIds;

            // Skip the "ASTEROIDS" prefix
            size_t pos = receivedData.find('|');
            if (pos != std::string::npos) {
                while (pos < receivedData.length()) {
                    size_t nextPos = receivedData.find('|', pos + 1);
                    if (nextPos == std::string::npos) {
                        nextPos = receivedData.length();
                    }

                    // Extract asteroid data segment
                    std::string segment = receivedData.substr(pos + 1, nextPos - pos - 1);

                    // Skip empty segments
                    if (!segment.empty()) {
                        // Parse segment
                        std::istringstream iss(segment);
                        std::string id, value;

                        // Get asteroid ID
                        if (std::getline(iss, id, ',')) {
                            // Add to updated IDs
                            updatedIds.push_back(id);

                            // Check if asteroid exists in our map
                            bool isNew = (asteroids.find(id) == asteroids.end());

                            // Get or create asteroid
                            asteroidData& asteroid = asteroids[id];

                            // Try to extract numeric part from the id string (like "ast_12345")
                            try {
                                // Find first digit in the string
                                size_t numStart = id.find_first_of("0123456789");
                                if (numStart != std::string::npos) {
                                    // Extract the numeric part and convert to int
                                    asteroid.asteroidID = std::stoi(id.substr(numStart));
                                }
                                else {
                                    // Fallback: use a hash of the string as the ID
                                    asteroid.asteroidID = static_cast<int>(std::hash<std::string>{}(id));
                                }
                            }
                            catch (...) {
                                // If conversion fails, use a hash of the string
                                asteroid.asteroidID = static_cast<int>(std::hash<std::string>{}(id));
                            }

                            // Store old position values (for existing asteroids)
                            float oldTargetX = 0.0f;
                            float oldTargetY = 0.0f;

                            if (!isNew) {
                                oldTargetX = asteroid.targetX;
                                oldTargetY = asteroid.targetY;
                            }

                            // Parse position X
                            if (std::getline(iss, value, ',')) {
                                asteroid.x = (float)atof(value.c_str());
                                asteroid.targetX = asteroid.x;  // Update target position
                            }

                            // Parse position Y
                            if (std::getline(iss, value, ',')) {
                                asteroid.y = (float)atof(value.c_str());
                                asteroid.targetY = asteroid.y;  // Update target position
                            }

                            // Parse velocity X
                            if (std::getline(iss, value, ',')) {
                                asteroid.velX = (float)atof(value.c_str());
                            }

                            // Parse velocity Y
                            if (std::getline(iss, value, ',')) {
                                asteroid.velY = (float)atof(value.c_str());
                            }

                            // Parse scale X
                            if (std::getline(iss, value, ',')) {
                                asteroid.scaleX = (float)atof(value.c_str());
                            }

                            // Parse scale Y
                            if (std::getline(iss, value, ',')) {
                                asteroid.scaleY = (float)atof(value.c_str());
                            }

                            // Parse active
                            if (std::getline(iss, value)) {
                                asteroid.active = (value == "1");
                            }

                            // For new asteroids, initialize current position to target
                            if (isNew) {
                                asteroid.currentX = asteroid.targetX;
                                asteroid.currentY = asteroid.targetY;
                                asteroid.creationTime = currentTime;
                            }
                            // If there's a large position jump, snap to it (teleportation)
                            else if (std::abs(oldTargetX - asteroid.targetX) > 100 ||
                                std::abs(oldTargetY - asteroid.targetY) > 100) {
                                asteroid.currentX = asteroid.targetX;
                                asteroid.currentY = asteroid.targetY;
                            }

                            // Update timestamp
                            asteroid.lastUpdateTime = currentTime;
                        }
                    }

                    pos = nextPos;
                }
            }

            // Remove asteroids that weren't in the update
            for (auto it = asteroids.begin(); it != asteroids.end();) {
                bool found = false;
                for (const auto& id : updatedIds) {
                    if (it->first == id) {
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    it = asteroids.erase(it);
                }
                else {
                    ++it;
                }
            }

            continue;
        }

        // Check if this is bullet data (format: "BULLETS|id1,x1,y1,velX1,velY1,dir1|id2,...")
        if (receivedData.find("BULLETS") == 0) {
            lockBullets();

            // Debug output - occasionally to avoid spam
            static int counter = 0;
            if (++counter % 100 == 0) {
                std::cout << "Received BULLETS message, length: " << receivedData.length() << std::endl;
            }

            // Keep a list of updated bullet IDs to track which ones to keep
            std::unordered_set<std::string> updatedBulletIDs;
            int newBullets = 0;
            int updatedBullets = 0;

            // Skip the "BULLETS" prefix
            size_t pos = receivedData.find('|');
            if (pos != std::string::npos) {
                while (pos < receivedData.length()) {
                    size_t nextPos = receivedData.find('|', pos + 1);
                    if (nextPos == std::string::npos) {
                        nextPos = receivedData.length();
                    }

                    // Extract bullet data segment
                    std::string segment = receivedData.substr(pos + 1, nextPos - pos - 1);

                    // Skip empty segments
                    if (!segment.empty()) {
                        // Parse segment
                        std::istringstream iss(segment);
                        std::string id, value;

                        // Get bullet ID
                        if (std::getline(iss, id, ',')) {
                            // Add this ID to our updated set
                            updatedBulletIDs.insert(id);

                            // Check if bullet exists in our map
                            bool isNewBullet = (bullets.find(id) == bullets.end());

                            // Get or create bullet data
                            bulletData& bullet = bullets[id];
                            bullet.bulletID = id;

                            // Only set this flag for newly created bullets
                            // This is important - do not change fromLocalPlayer flag for existing bullets
                            if (isNewBullet) {
                                bullet.fromLocalPlayer = false;
                            }

                            // Parse position X
                            if (std::getline(iss, value, ',')) {
                                bullet.x = (float)atof(value.c_str());
                            }

                            // Parse position Y
                            if (std::getline(iss, value, ',')) {
                                bullet.y = (float)atof(value.c_str());
                            }

                            // Parse velocity X
                            if (std::getline(iss, value, ',')) {
                                bullet.velX = (float)atof(value.c_str());
                            }

                            // Parse velocity Y
                            if (std::getline(iss, value, ',')) {
                                bullet.velY = (float)atof(value.c_str());
                            }

                            // Parse direction
                            if (std::getline(iss, value)) {
                                bullet.dir = (float)atof(value.c_str());
                            }

                            // Track stats
                            if (isNewBullet) newBullets++;
                            else updatedBullets++;
                        }
                    }

                    pos = nextPos;
                }
            }

            // Now remove bullets that weren't updated (they've probably expired on the server)
            int removedBullets = 0;
            for (auto it = bullets.begin(); it != bullets.end();) {
                // Keep local bullets no matter what, and keep updated remote bullets
                if (it->second.fromLocalPlayer || updatedBulletIDs.find(it->first) != updatedBulletIDs.end()) {
                    ++it;
                }
                else {
                    it = bullets.erase(it);
                    removedBullets++;
                }
            }

            // Debug output - occasionally to avoid spam
            if (counter % 100 == 0) {
                std::cout << "Bullets: " << newBullets << " new, "
                    << updatedBullets << " updated, "
                    << removedBullets << " removed, "
                    << "Total: " << bullets.size() << std::endl;
            }

            unlockBullets();
            continue;
        }
        //hadling of the score.
		if (receivedData.find("SCORE_UPDATE") == 0)
		{
            //print message.
			//std::cout << "Received SCORE_UPDATE message: " << receivedData << std::endl;
            // Skip the "SCORE_UPDATE|" prefix
			size_t pos = receivedData.find('|');
			if (pos != std::string::npos) {
				std::string scoreData = receivedData.substr(pos + 1);
				std::istringstream str(scoreData);
				// Parse player ID and score
				int id;
				int score;
				while (str >> id >> score) {
					players[id].score = score;  // Update player score
				}
			}
			continue;
		}
        // Handle player data updates
        std::string pData = std::string(recvBuffer, recvBuffer + receivedBytes);
        std::istringstream str(pData);

        playerData p;

        // Updated parsing to include score
        while (str >> p.playerID >> p.x >> p.y >> p.rot >> p.score >> p.cIP) {
            players[p.playerID] = p;  // Store new player data
        }

        pCount = players.size();
    }
}

/**
 * @brief Reports the destruction of an asteroid to the server.
 *
 * Sends a message to the server containing the asteroid ID to indicate its destruction.
 *
 * @param asteroidID The ID of the destroyed asteroid.
 */
void Client::reportAsteroidDestruction(const std::string& asteroidID) {
    // Create the message to send to server
    std::string message = "DESTROY_ASTEROID|" + asteroidID;

    // Set up server address
    addrinfo hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;        // IPv4
    hints.ai_socktype = SOCK_DGRAM;   // UDP
    hints.ai_protocol = IPPROTO_UDP;  // UDP protocol

    // Get address information for the server
    addrinfo* result = nullptr;
    if (getaddrinfo(serverIP.c_str(), std::to_string(serverPort).c_str(), &hints, &result) != 0) {
        std::cerr << "getaddrinfo failed when reporting asteroid destruction." << std::endl;
        return;
    }

    // Extract the server address
    sockaddr_in tempServerAddr = *reinterpret_cast<sockaddr_in*>(result->ai_addr);

    // Send to the server
    sendto(clientSocket, message.c_str(), message.length(), 0,
        reinterpret_cast<sockaddr*>(&tempServerAddr), sizeof(tempServerAddr));

    // Clean up
    freeaddrinfo(result);
}

/**
 * @brief Reports a player's updated score to the server.
 *
 * Sends a message to the server with the player's ID and new score.
 *
 * @param playerID The ID of the player whose score is being updated.
 * @param score The new score of the player.
 */
void Client::reportPlayerScore(const std::string& playerID, int score)
{// Create the message to send to server
	std::string message = "UPDATE_SCORE|" + playerID + " " + std::to_string(score);
	// Set up server address
	addrinfo hints;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;        // IPv4
	hints.ai_socktype = SOCK_DGRAM;   // UDP
	hints.ai_protocol = IPPROTO_UDP;  // UDP protocol
	// Get address information for the server
	addrinfo* result = nullptr;
	if (getaddrinfo(serverIP.c_str(), std::to_string(serverPort).c_str(), &hints, &result) != 0) {
		std::cerr << "getaddrinfo failed when reporting player score." << std::endl;
		return;
	}
	// Extract the server address
	sockaddr_in tempServerAddr = *reinterpret_cast<sockaddr_in*>(result->ai_addr);
	// Send to the server
	sendto(clientSocket, message.c_str(), message.length(), 0,
		reinterpret_cast<sockaddr*>(&tempServerAddr), sizeof(tempServerAddr));
	// Clean up
	freeaddrinfo(result);
}


/**
 * Updates asteroid positions using interpolation.
 * Ensures smooth movement between received position updates.
 */
void updateAsteroidInterpolation() {
    std::lock_guard<std::mutex> lock(asteroidsMutex);
    auto currentTime = std::chrono::steady_clock::now();

    for (auto& pair : asteroids) {
        auto& asteroid = pair.second;
        if (!asteroid.active) continue;

        float dt = std::chrono::duration<float>(currentTime - asteroid.lastUpdateTime).count();
        const float INTERP_TIME = 0.1f; // 100ms for smooth interpolation

        if (dt < INTERP_TIME) {
            float t = dt / INTERP_TIME;
            asteroid.currentX += (asteroid.targetX - asteroid.currentX) * t;
            asteroid.currentY += (asteroid.targetY - asteroid.currentY) * t;
        }
        else {
            asteroid.currentX = asteroid.targetX;
            asteroid.currentY = asteroid.targetY;
        }
    }
}

/**
 * Reports the creation of a bullet to the server.
 *
 * @param pos The position of the bullet.
 * @param vel The velocity of the bullet.
 * @param dir The direction the bullet is traveling.
 * @param bulletID Optional bullet identifier; if empty, a new ID is generated.
 */
void Client::reportBulletCreation(const AEVec2& pos, const AEVec2& vel, float dir, const std::string& bulletID) {
    if (this->clientSocket == INVALID_SOCKET) {
        std::cerr << "ERROR: Cannot send bullet creation - socket is invalid!" << std::endl;
        return;
    }

    // Generate a unique bullet ID if not provided
    std::string finalBulletID = bulletID.empty() ?
        std::to_string(playerID) + "_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())
        : bulletID;

    // Construct the message to send
    std::string message = "BULLET_CREATE " +
        std::to_string(pos.x) + " " + std::to_string(pos.y) + " " +
        std::to_string(vel.x) + " " + std::to_string(vel.y) + " " +
        std::to_string(dir) + " " + finalBulletID;

    // Set up server address information
    addrinfo hints{};
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    addrinfo* result = nullptr;
    if (getaddrinfo(this->serverIP.c_str(), std::to_string(this->serverPort).c_str(), &hints, &result) != 0) {
        std::cerr << "getaddrinfo failed when reporting bullet creation." << std::endl;
        return;
    }

    sockaddr_in tempServerAddr = *reinterpret_cast<sockaddr_in*>(result->ai_addr);

    // Send message to the server
    int sendResult = sendto(this->clientSocket, message.c_str(), message.length(), 0,
        reinterpret_cast<sockaddr*>(&tempServerAddr), sizeof(tempServerAddr));

    if (sendResult == SOCKET_ERROR) {
        std::cerr << "Send bullet creation failed with error: " << WSAGetLastError() << std::endl;
    }
    else {
        std::cout << "Sent bullet creation message to server with ID: " << finalBulletID << std::endl;
    }

    freeaddrinfo(result);
}

/**
 * Cleans up network resources by closing the socket and cleaning up Winsock.
 */
void Client::cleanup() {
    if (clientSocket != INVALID_SOCKET) {
        closesocket(clientSocket);
    }
    WSACleanup();
}