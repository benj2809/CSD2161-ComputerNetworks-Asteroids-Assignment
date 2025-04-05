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

// Static member initialization
size_t Client::playerCount;
int Client::playerID = -1; // Client's player ID (-1 means unassigned)
std::mutex Client::playersMutex;
std::mutex Client::bulletsMutex;

// Shared data structures
std::unordered_map<int, PlayerData> players;
std::unordered_map<std::string, AsteroidData> asteroids;
std::mutex asteroidsMutex;
std::unordered_map<std::string, BulletData> bullets;

/* ======================= INITIALIZATION METHODS ======================= */

/**
 * @brief Initializes the client with server connection details
 * @param serverIP The IP address of the server
 * @param serverPort The port number of the server
 * @return true if initialization succeeds, false otherwise
 */
bool Client::initialize(const std::string& serverIP, uint16_t serverPort) {
    this->serverIP = serverIP;
    this->serverPort = serverPort;
    playerCount = 0;

    // Initialization sequence
    return setupWinsock() &&
        resolveAddress(serverIP, serverPort) &&
        createSocket();
}

/**
 * @brief Sets up the Winsock library
 * @return true if Winsock initializes successfully
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
 * @brief Resolves the server address using DNS
 * @param serverIP The server IP address
 * @param serverPort The server port number
 * @return true if address resolution succeeds
 */
bool Client::resolveAddress(const std::string& serverIP, uint16_t serverPort) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    addrinfo* addressInfo = nullptr;
    if (getaddrinfo(serverIP.c_str(), std::to_string(serverPort).c_str(), &hints, &addressInfo) != 0) {
        std::cerr << "getaddrinfo failed." << std::endl;
        WSACleanup();
        return false;
    }

    freeaddrinfo(addressInfo);
    return true;
}

/**
 * @brief Creates a UDP socket for communication
 * @return true if socket creation succeeds
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
 * @brief Connects the client to the server (UDP is connectionless)
 * @return true if server address information is retrieved successfully
 */
bool Client::connectToServer() {
    addrinfo hints{};
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

/* ======================= RUNTIME METHODS ======================= */

/**
 * @brief Runs the client in interactive mode
 * Starts network thread and sends initial message
 */
void Client::run() {
    sendToServerUdp();
    std::thread(&Client::handleNetwork, this).detach();
}

/**
 * @brief Runs the client in script mode
 * @param scriptPath Path to the script file containing commands
 */
void Client::runScript(const std::string& scriptPath) {
    std::thread(&Client::handleNetwork, this).detach();
    sendToServerUdp();
}

/**
 * @brief Retrieves server information from a script file
 * @param scriptPath Path to the script file
 * @param IP Output parameter for server IP
 * @param port Output parameter for server port
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

/* ======================= NETWORK COMMUNICATION METHODS ======================= */

/**
 * @brief Sends player data to the server via UDP
 * Includes position, rotation, and score
 */
void Client::sendToServerUdp() {
    // Get player data
    AEVec2 position = returnPlayerPosition();
    float rot = returnPlayerRotation();
    int score = returnPlayerScore();

    // Format message
    std::string message = std::to_string(position.x) + " " +
        std::to_string(position.y) + " " +
        std::to_string(rot) + " " +
        std::to_string(score);

    // Prepare UDP message
    UdpClientData udpMessage{};
    if (strncpy_s(udpMessage.data, sizeof(udpMessage.data), message.c_str(), message.length()) != 0) {
        std::cerr << "Error copying message to buffer." << std::endl;
        return;
    }

    // Resolve server address
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    addrinfo* result = nullptr;
    if (getaddrinfo(serverIP.c_str(), std::to_string(serverPort).c_str(), &hints, &result) != 0) {
        std::cerr << "getaddrinfo failed." << std::endl;
        return;
    }

    // Store server address and send
    udpMessage.clientAddr = *reinterpret_cast<sockaddr_in*>(result->ai_addr);
    if (sendto(clientSocket, udpMessage.data, strlen(udpMessage.data), 0,
        reinterpret_cast<sockaddr*>(&udpMessage.clientAddr),
        sizeof(udpMessage.clientAddr)) == SOCKET_ERROR) {
        std::cerr << "Send failed with error: " << WSAGetLastError() << std::endl;
    }

    freeaddrinfo(result);
}

/**
 * @brief Handles incoming network messages in a separate thread
 * Processes player updates, asteroid data, bullet data, and score updates
 */
void Client::handleNetwork() {
    sockaddr_in serverAddr{};
    int addrSize = sizeof(serverAddr);
    char receiveBuffer[1024];

    while (true) {
        int receivedBytes = recvfrom(clientSocket, receiveBuffer, sizeof(receiveBuffer), 0,
            (sockaddr*)&serverAddr, &addrSize);
        if (receivedBytes <= 0) continue;

        receiveBuffer[receivedBytes] = '\0';
        std::string receivedData(receiveBuffer, receivedBytes);

        // Handle player ID assignment
        if (receivedBytes <= 3 && playerID == -1) {
            playerID = std::stoi(receiveBuffer);
            std::cout << "Assigned Player ID: " << playerID << std::endl;
            continue;
        }

        // Process asteroid data
        if (receivedData.find("ASTEROIDS") == 0) {
            processAsteroidData(receivedData);
            continue;
        }

        // Process bullet data
        if (receivedData.find("BULLETS") == 0) {
            processBulletData(receivedData);
            continue;
        }

        // Process score updates
        if (receivedData.find("SCORE_UPDATE") == 0) {
            processScoreUpdate(receivedData);
            continue;
        }

        // Process general player data
        processPlayerData(receivedData);
    }
}

/**
 * @brief Processes asteroid data updates from server
 * @param data The received asteroid data string
 */
void Client::processAsteroidData(const std::string& data) {
    std::lock_guard<std::mutex> lock(asteroidsMutex);
    auto currentTime = std::chrono::steady_clock::now();
    std::vector<std::string> updatedIds;

    size_t pos = data.find('|');
    if (pos != std::string::npos) {
        while (pos < data.length()) {
            size_t nextPos = data.find('|', pos + 1);
            if (nextPos == std::string::npos) nextPos = data.length();

            std::string segment = data.substr(pos + 1, nextPos - pos - 1);
            if (!segment.empty()) {
                std::istringstream iss(segment);
                std::string id, value;

                if (std::getline(iss, id, ',')) {
                    updatedIds.push_back(id);
                    bool isNew = (asteroids.find(id) == asteroids.end());
                    AsteroidData& asteroid = asteroids[id];

                    // Parse asteroid ID
                    try {
                        size_t numStart = id.find_first_of("0123456789");
                        asteroid.asteroidID = (numStart != std::string::npos) ?
                            std::stoi(id.substr(numStart)) :
                            static_cast<int>(std::hash<std::string>{}(id));
                    }
                    catch (...) {
                        asteroid.asteroidID = static_cast<int>(std::hash<std::string>{}(id));
                    }

                    // Store old positions for interpolation
                    float oldTargetX = 0.0f, oldTargetY = 0.0f;
                    if (!isNew) {
                        oldTargetX = asteroid.targetX;
                        oldTargetY = asteroid.targetY;
                    }

                    // Parse asteroid properties
                    if (std::getline(iss, value, ',')) asteroid.X = (float)atof(value.c_str());
                    if (std::getline(iss, value, ',')) asteroid.Y = (float)atof(value.c_str());
                    if (std::getline(iss, value, ',')) asteroid.velocityX = (float)atof(value.c_str());
                    if (std::getline(iss, value, ',')) asteroid.velocityY = (float)atof(value.c_str());
                    if (std::getline(iss, value, ',')) asteroid.scaleX = (float)atof(value.c_str());
                    if (std::getline(iss, value, ',')) asteroid.scaleY = (float)atof(value.c_str());
                    if (std::getline(iss, value)) asteroid.isActive = (value == "1");

                    // Update positions
                    asteroid.targetX = asteroid.X;
                    asteroid.targetY = asteroid.Y;

                    if (isNew) {
                        asteroid.currentX = asteroid.targetX;
                        asteroid.currentY = asteroid.targetY;
                        asteroid.creationTime = currentTime;
                    }
                    else if (std::abs(oldTargetX - asteroid.targetX) > 100 ||
                        std::abs(oldTargetY - asteroid.targetY) > 100) {
                        asteroid.currentX = asteroid.targetX;
                        asteroid.currentY = asteroid.targetY;
                    }

                    asteroid.lastUpdateTime = currentTime;
                }
            }
            pos = nextPos;
        }
    }

    // Remove asteroids not in the update
    for (auto it = asteroids.begin(); it != asteroids.end();) {
        if (std::find(updatedIds.begin(), updatedIds.end(), it->first) == updatedIds.end()) {
            it = asteroids.erase(it);
        }
        else {
            ++it;
        }
    }
}

/**
 * @brief Processes bullet data updates from server
 * @param data The received bullet data string
 */
void Client::processBulletData(const std::string& data) {
    std::lock_guard<std::mutex> lock(bulletsMutex);
    static int debugCounter = 0;
    if (++debugCounter % 100 == 0) {
        std::cout << "Received BULLETS message, length: " << data.length() << std::endl;
    }

    std::unordered_set<std::string> updatedBulletIDs;
    int newBullets = 0, updatedBullets = 0;

    size_t pos = data.find('|');
    if (pos != std::string::npos) {
        while (pos < data.length()) {
            size_t nextPos = data.find('|', pos + 1);
            if (nextPos == std::string::npos) nextPos = data.length();

            std::string segment = data.substr(pos + 1, nextPos - pos - 1);
            if (!segment.empty()) {
                std::istringstream iss(segment);
                std::string id, value;

                if (std::getline(iss, id, ',')) {
                    updatedBulletIDs.insert(id);
                    bool isNewBullet = (bullets.find(id) == bullets.end());
                    BulletData& bullet = bullets[id];
                    bullet.bulletID = id;

                    if (isNewBullet) bullet.fromLocalPlayer = false;

                    if (std::getline(iss, value, ',')) bullet.X = (float)atof(value.c_str());
                    if (std::getline(iss, value, ',')) bullet.Y = (float)atof(value.c_str());
                    if (std::getline(iss, value, ',')) bullet.velocityX = (float)atof(value.c_str());
                    if (std::getline(iss, value, ',')) bullet.velocityY = (float)atof(value.c_str());
                    if (std::getline(iss, value)) bullet.direction = (float)atof(value.c_str());

                    if (isNewBullet) newBullets++;
                    else updatedBullets++;
                }
            }
            pos = nextPos;
        }
    }

    // Remove old bullets
    int removedBullets = 0;
    for (auto it = bullets.begin(); it != bullets.end();) {
        if (!it->second.fromLocalPlayer && updatedBulletIDs.find(it->first) == updatedBulletIDs.end()) {
            it = bullets.erase(it);
            removedBullets++;
        }
        else {
            ++it;
        }
    }

    if (debugCounter % 100 == 0) {
        std::cout << "Bullets: " << newBullets << " new, "
            << updatedBullets << " updated, "
            << removedBullets << " removed, "
            << "Total: " << bullets.size() << std::endl;
    }
}

/**
 * @brief Processes score updates from server
 * @param data The received score data string
 */
void Client::processScoreUpdate(const std::string& data) {
    size_t pos = data.find('|');
    if (pos != std::string::npos) {
        std::string scoreData = data.substr(pos + 1);
        std::istringstream str(scoreData);
        int id, score;
        while (str >> id >> score) {
            players[id].score = score;
        }
    }
}

/**
 * @brief Processes general player data updates
 * @param data The received player data string
 */
void Client::processPlayerData(const std::string& data) {
    std::istringstream str(data);
    PlayerData p;
    while (str >> p.playerID >> p.X >> p.Y >> p.rotation >> p.score >> p.clientIP) {
        players[p.playerID] = p;
    }
    playerCount = players.size();
}

/* ======================= GAME EVENT REPORTING METHODS ======================= */

/**
 * @brief Reports asteroid destruction to the server
 * @param asteroidID The ID of the destroyed asteroid
 */
void Client::reportAsteroidDestruction(const std::string& asteroidID) {
    std::string message = "DESTROY_ASTEROID|" + asteroidID;
    sendServerMessage(message);
}

/**
 * @brief Reports player score update to the server
 * @param playerID The ID of the player
 * @param score The new score value
 */
void Client::reportPlayerScore(const std::string& playerID, int score) {
    std::string message = "UPDATE_SCORE|" + playerID + " " + std::to_string(score);
    sendServerMessage(message);
}

/**
 * @brief Reports bullet creation to the server
 * @param pos The bullet's position
 * @param vel The bullet's velocity
 * @param dir The bullet's direction
 * @param bulletID Optional bullet identifier
 */
void Client::reportBulletCreation(const AEVec2& pos, const AEVec2& vel, float dir, const std::string& bulletID) {
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "ERROR: Cannot send bullet creation - socket is invalid!" << std::endl;
        return;
    }

    std::string finalBulletID = bulletID.empty() ?
        std::to_string(playerID) + "_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) :
        bulletID;

    std::string message = "BULLET_CREATE " +
        std::to_string(pos.x) + " " + std::to_string(pos.y) + " " +
        std::to_string(vel.x) + " " + std::to_string(vel.y) + " " +
        std::to_string(dir) + " " + finalBulletID;

    sendServerMessage(message);
    std::cout << "Sent bullet creation message to server with ID: " << finalBulletID << std::endl;
}

/**
 * @brief Helper method to send messages to server
 * @param message The message to send
 */
void Client::sendServerMessage(const std::string& message) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    addrinfo* result = nullptr;
    if (getaddrinfo(serverIP.c_str(), std::to_string(serverPort).c_str(), &hints, &result) != 0) {
        std::cerr << "getaddrinfo failed when sending message." << std::endl;
        return;
    }

    sockaddr_in serverAddress = *reinterpret_cast<sockaddr_in*>(result->ai_addr);
    if (sendto(clientSocket, message.c_str(), message.length(), 0,
        reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) == SOCKET_ERROR) {
        std::cerr << "Send failed with error: " << WSAGetLastError() << std::endl;
    }

    freeaddrinfo(result);
}

/* ======================= UTILITY METHODS ======================= */

/**
 * @brief Displays current player scores in a formatted table
 */
void Client::displayPlayerScores() {
    std::lock_guard<std::mutex> lock(playersMutex);

    std::cout << "\n================= PLAYER SCORES ==================" << std::endl;
    std::cout << "Player ID          Score          Position" << std::endl;
    std::cout << "--------------------------------------------------" << std::endl;

    for (const auto& pair : players) {
        const PlayerData& player = pair.second;
        std::cout << pair.first << "                  "
            << player.score << "              ("
            << std::fixed << std::setprecision(1) << player.X << ", "
            << std::fixed << std::setprecision(1) << player.Y << ")"
            << (pair.first == playerID ? " [YOU]" : "") << std::endl;
    }

    std::cout << "--------------------------------------------------" << std::endl;
}

/**
 * @brief Updates asteroid positions using interpolation for smooth movement
 */
void updateAsteroidInterpolation() {
    std::lock_guard<std::mutex> lock(asteroidsMutex);
    auto currentTime = std::chrono::steady_clock::now();

    for (auto& pair : asteroids) {
        auto& asteroid = pair.second;
        if (!asteroid.isActive) continue;

        float deltaTime = std::chrono::duration<float>(currentTime - asteroid.lastUpdateTime).count();
        const float interpolationDuration = 0.1f; // 100ms interpolation window

        if (deltaTime < interpolationDuration) {
            float interpolationFactor = deltaTime / interpolationDuration;
            asteroid.currentX += (asteroid.targetX - asteroid.currentX) * interpolationFactor;
            asteroid.currentY += (asteroid.targetY - asteroid.currentY) * interpolationFactor;
        }
        else {
            asteroid.currentX = asteroid.targetX;
            asteroid.currentY = asteroid.targetY;
        }
    }
}

/**
 * @brief Cleans up network resources
 */
void Client::cleanup() {
    if (clientSocket != INVALID_SOCKET) {
        closesocket(clientSocket);
    }
    WSACleanup();
}