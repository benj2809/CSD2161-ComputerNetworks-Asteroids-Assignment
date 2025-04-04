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
 * Initialize the client with server connection details
 * @param serverIP The IP address of the server to connect to
 * @param serverPort The port number of the server
 * @return true if initialization successful, false otherwise
 */
bool Client::initialize(const std::string& serverIP, uint16_t serverPort) {
    this->serverIP = serverIP;     // Store server IP
    this->serverPort = serverPort; // Store server port

    pCount = 0;

    // Perform initialization steps in sequence
    if (!setupWinsock()) return false;
    if (!resolveAddress(serverIP, serverPort)) return false;
    if (!createSocket()) return false;
    // if (!connectToServer()) return false;

    return true;
}

/**
 * Run the client in interactive mode
 * Creates a separate thread for network operations and handles user input
 */
void Client::run() {
    // Create and detach network handling thread
    sendToServerUdp();//sends a message to the server.

    /*  std::thread idThread(&Client::handleID, this);
    idThread.detach();*/

    std::thread networkThread(&Client::handleNetwork, this);
    networkThread.detach();

    // Process user input on the main thread
    //handleUserInput();

}

/**
 * Run the client in script mode
 * @param scriptPath Path to the script file containing commands
 */
void Client::runScript(const std::string& scriptPath) {
    // Create and detach network handling thread
    std::thread networkThread(&Client::handleNetwork, this);
    networkThread.detach();

    // Process script commands
    //handleScript(scriptPath);
    sendToServerUdp();
}

void Client::getServerInfo(const std::string& scriptPath, std::string& IP, std::string& port)
{
    std::ifstream file(scriptPath);  // Open the file

    if (!file) {  // Check if the file was opened successfully
        std::cerr << "Error: Could not open file: " << scriptPath << std::endl;
        return;
    }

    std::getline(file, IP);   // Read the first line (IP)
    std::getline(file, port); // Read the second line (Port)

    file.close();  // Close the file
}

void Client::sendToServerUdp() {

    ////get position of th ship.
    AEVec2 position = returnPlayerPosition();

    // Direction ship is facing.
    float rot = returnPlayerRotation();

    int score = returnPlayerScore();

    // Bullet info

    // const std::string message = "Hello World";  // Message to send
    std::string message = std::to_string(position.x) + " " +
        std::to_string(position.y) + " " +
        std::to_string(rot) + " " +
        std::to_string(score);
    //std::cout << position_1.c_str() << std::endl;
    // Prepare the UdpClientData structure
    UdpClientData udpMessage;
    ZeroMemory(&udpMessage, sizeof(udpMessage));  // Clear the struct to avoid leftover data

    // Copy the message into the structure using strncpy_s
    errno_t err = strncpy_s(udpMessage.data, sizeof(udpMessage.data), message.c_str(), message.length());
    if (err != 0) {
        std::cerr << "Error copying message to buffer." << std::endl;
        return;
    }

    // Setup address hints structure
    addrinfo hints{};
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;        // IPv4
    hints.ai_socktype = SOCK_DGRAM;  // UDP (use SOCK_DGRAM instead of SOCK_STREAM)
    hints.ai_protocol = IPPROTO_UDP;  // UDP protocol

    // Get address information for the server
    addrinfo* result = nullptr;
    if (getaddrinfo(serverIP.c_str(), std::to_string(serverPort).c_str(), &hints, &result) != 0) {
        std::cerr << "getaddrinfo failed." << std::endl;
        return;
    }

    // Fill in the clientAddr in the structure
    udpMessage.clientAddr = *reinterpret_cast<sockaddr_in*>(result->ai_addr);

    // Send message to the server using the pre-existing socket
    int sendResult = sendto(clientSocket, udpMessage.data, strlen(udpMessage.data), 0,
        reinterpret_cast<sockaddr*>(&udpMessage.clientAddr), sizeof(udpMessage.clientAddr));

    if (sendResult == SOCKET_ERROR) {
        std::cerr << "Send failed with error: " << WSAGetLastError() << std::endl;
    }
    /*else {
        std::cout << "Message sent: " << udpMessage.data << std::endl;
    }*/

    // Clean up
    freeaddrinfo(result);  // Free the address info as it's no longer needed
}


void Client::displayPlayerScores() {

    std::lock_guard<std::mutex> lock(mutex); // Use your existing mutex to prevent concurrent access
    std::cout << "\n=== PLAYER SCORES ===" << std::endl;
    std::cout << "Player ID\tScore\tPosition" << std::endl;
    std::cout << "------------------------------------" << std::endl;

    // Fix: Replace structured binding with traditional iterator approach
    for (const auto& pair : players) {
        int id = pair.first;
        const playerData& player = pair.second;

        std::cout << id << "\t\t"
            << player.score << "\t"
            << "(" << std::fixed << std::setprecision(1) << player.x
            << ", " << std::fixed << std::setprecision(1) << player.y << ")"
            << (id == playerID ? " [YOU]" : "") << std::endl;
    }
    std::cout << "======================" << std::endl;
}


/**
 * Initialize the Winsock library
 * @return true if successful, false otherwise
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
 * Resolve server address using DNS
 * @param serverIP Server IP address
 * @param serverPort Server port number
 * @return true if address resolution successful, false otherwise
 */
bool Client::resolveAddress(const std::string& serverIP, uint16_t serverPort) {
    addrinfo hints{};
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;        // IPv4
    hints.ai_socktype = SOCK_DGRAM;  // UDP (use SOCK_DGRAM instead of SOCK_STREAM)
    hints.ai_protocol = IPPROTO_UDP;  // UDP protocol

    addrinfo* result = nullptr;
    if (getaddrinfo(serverIP.c_str(), std::to_string(serverPort).c_str(), &hints, &result) != 0) {
        std::cerr << "getaddrinfo failed." << std::endl;
        WSACleanup();
        return false;
    }

    // Free the address info as we're just testing resolution
    freeaddrinfo(result);
    return true;
}


/**
 * Create a TCP socket
 * @return true if socket creation successful, false otherwise
 */
bool Client::createSocket() {
    clientSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);  // Use SOCK_DGRAM for UDP
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed." << std::endl;
        WSACleanup();
        return false;
    }
    return true;
}

/**
 * Connect to the server using the stored IP and port
 * @return true if connection successful, false otherwise
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
 * Thread function to handle network operations
 * Continuously receives and processes data from the server
 */
 /**
  * Thread function to handle network operations
  * Continuously receives and processes data from the server
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

                            // Simply use the bullet ID to track what's already been processed
                            bool isNewBullet = (bullets.find(id) == bullets.end());

                            // Get or create bullet data
                            bulletData& bullet = bullets[id];
                            bullet.bulletID = id;
                            bullet.fromLocalPlayer = false;

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
                    << removedBullets << " removed" << std::endl;
            }

            unlockBullets();
            continue;
        }
        //hadling of the score.
		if (receivedData.find("SCORE_UPDATE") == 0)
		{
            //print message.
			std::cout << "Received SCORE_UPDATE message: " << receivedData << std::endl;
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


void updateAsteroidInterpolation() {
    std::lock_guard<std::mutex> lock(asteroidsMutex);
    auto currentTime = std::chrono::steady_clock::now();

    for (auto& pair : asteroids) {
        auto& asteroid = pair.second;
        if (!asteroid.active) continue;

        // Calculate time since last update
        float dt = std::chrono::duration<float>(currentTime - asteroid.lastUpdateTime).count();

        // Use shorter interpolation time for smoother movement
        const float INTERP_TIME = 0.1f; // 100ms

        // Interpolate position
        if (dt < INTERP_TIME) {
            float t = dt / INTERP_TIME;
            asteroid.currentX = asteroid.currentX + (asteroid.targetX - asteroid.currentX) * t;
            asteroid.currentY = asteroid.currentY + (asteroid.targetY - asteroid.currentY) * t;
        }
        else {
            // If enough time has passed, just use the target position
            asteroid.currentX = asteroid.targetX;
            asteroid.currentY = asteroid.targetY;
        }
    }
}

void Client::reportBulletCreation(const AEVec2& pos, const AEVec2& vel, float dir, const std::string& bulletID) {
    if (this->clientSocket == INVALID_SOCKET) {
        std::cerr << "ERROR: Cannot send bullet creation - socket is invalid!" << std::endl;
        return;
    }

    // Use the provided bulletID or generate a new one
    std::string finalBulletID = bulletID;
    if (finalBulletID.empty()) {
        finalBulletID = std::to_string(playerID) + "_" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    }

    // Format: "BULLET_CREATE posX posY velX velY dir bulletID"
    std::string message = "BULLET_CREATE " +
        std::to_string(pos.x) + " " +
        std::to_string(pos.y) + " " +
        std::to_string(vel.x) + " " +
        std::to_string(vel.y) + " " +
        std::to_string(dir) + " " +
        finalBulletID;  // Include bullet ID in message

    // Set up server address
    addrinfo hints{};
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;        // IPv4
    hints.ai_socktype = SOCK_DGRAM;   // UDP
    hints.ai_protocol = IPPROTO_UDP;  // UDP protocol

    // Get address information for the server
    addrinfo* result = nullptr;
    if (getaddrinfo(this->serverIP.c_str(), std::to_string(this->serverPort).c_str(), &hints, &result) != 0) {
        std::cerr << "getaddrinfo failed when reporting bullet creation." << std::endl;
        return;
    }

    // Extract the server address
    sockaddr_in tempServerAddr = *reinterpret_cast<sockaddr_in*>(result->ai_addr);

    // Send to the server
    int sendResult = sendto(this->clientSocket, message.c_str(), message.length(), 0,
        reinterpret_cast<sockaddr*>(&tempServerAddr), sizeof(tempServerAddr));

    if (sendResult == SOCKET_ERROR) {
        std::cerr << "Send bullet creation failed with error: " << WSAGetLastError() << std::endl;
    }
    else {
        std::cout << "Sent bullet creation message to server with ID: " << finalBulletID << std::endl;
    }

    // Clean up
    freeaddrinfo(result);
}

/**
 * Clean up resources (sockets, Winsock) on exit
 */
void Client::cleanup() {
    // Close socket if valid
    if (clientSocket != INVALID_SOCKET) {
        closesocket(clientSocket);
    }
    // Clean up Winsock
    WSACleanup();
}