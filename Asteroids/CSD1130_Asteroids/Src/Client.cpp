#include "Client.h"
#include "AEEngine.h"
#include "GameState_Asteroids.h"

size_t Client::pCount;

int Client::playerID = -1; // Store client's player ID

std::unordered_map<int, playerData> players;
std::mutex Client::mutex;

std::unordered_map<std::string, asteroidData> asteroids;
std::mutex asteroidsMutex;

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
            continue;
        }

        // Check if this is asteroid data (format: "A|id,x,y,vx,vy,sx,sy,active;")
        if (receivedData.find("A|") == 0) {
            std::lock_guard<std::mutex> lock(asteroidsMutex);

            // Clear existing asteroids
            asteroids.clear();

            // Skip the "A|" prefix
            size_t pos = 2;
            while (pos < receivedData.length()) {
                size_t endPos = receivedData.find(';', pos);
                if (endPos == std::string::npos) break;

                std::string asteroidStr = receivedData.substr(pos, endPos - pos);
                pos = endPos + 1;

                // Parse the asteroid data
                size_t commaPos = asteroidStr.find(',');
                if (commaPos == std::string::npos) continue;

                std::string id = asteroidStr.substr(0, commaPos);
                std::string restData = asteroidStr.substr(commaPos + 1);

                // Create asteroid struct
                asteroidData asteroid;

                // Parse position, velocity, scale, and active state
                std::istringstream ss(restData);
                std::string token;

                // Parse x
                if (std::getline(ss, token, ',')) asteroid.x = (float)atof(token.c_str());

                // Parse y
                if (std::getline(ss, token, ',')) asteroid.y = (float)atof(token.c_str());

                // Parse velX
                if (std::getline(ss, token, ',')) asteroid.velX = (float)atof(token.c_str());

                // Parse velY
                if (std::getline(ss, token, ',')) asteroid.velY = (float)atof(token.c_str());

                // Parse scaleX
                if (std::getline(ss, token, ',')) asteroid.scaleX = (float)atof(token.c_str());

                // Parse scaleY
                if (std::getline(ss, token, ',')) asteroid.scaleY = (float)atof(token.c_str());

                // Parse active
                if (std::getline(ss, token, ',')) asteroid.active = (token == "1");

                // Add to map
                asteroids[id] = asteroid;
            }

            continue;
        }

        // Check if this is asteroid destruction notification
        if (receivedData.find("D|") == 0) {
            std::string asteroidID = receivedData.substr(2);

            // Remove the asteroid from the map
            std::lock_guard<std::mutex> lock(asteroidsMutex);
            asteroids.erase(asteroidID);

            continue;
        }

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

// Remove
void Client::handleID() {
    // std::vector<uint8_t> recvQueue;  // Buffer for incoming data
    sockaddr_in serverAddr{};        // Store server's address
    int addrSize = sizeof(serverAddr);

    char recvBuffer[1024];  // Temporary buffer for received data
    int receivedBytes = recvfrom(clientSocket, recvBuffer, sizeof(recvBuffer), 0,
        (sockaddr*)&serverAddr, &addrSize);

    if (receivedBytes > 0 && playerID == -1) { // Only set once
        playerID = std::stoi(recvBuffer); // Convert received ID to integer
        std::cout << "Received My Player ID: " << playerID << std::endl;
    }
}

/**
 * Process commands from a script file
 * @param scriptPath Path to the script file
 */
void Client::handleScript(const std::string& scriptPath) {
    std::ifstream file(scriptPath);
    if (!file.is_open()) {
        std::cerr << "Failed to open script file." << std::endl;
        return;
    }

    // Skip the first two lines (IP and port) as they're already processed
    std::string dummyLine;
    std::getline(file, dummyLine);
    std::getline(file, dummyLine);

    // Process each command in the script file
    std::string message;
    while (std::getline(file, message)) {
        if (message.empty()) continue;

        std::vector<uint8_t> msg;

        // Handle list users command
        if (message.substr(0, 2) == "/l") {
            msg.push_back(REQ_LISTUSERS);
        }
        // Handle test command - sends raw hex data
        else if (message.substr(0, 2) == "/t") {
            std::string hexData = message.substr(3);
            // Convert hex string to bytes
            for (size_t i = 0; i < hexData.length(); i += 2) {
                std::string byteString = hexData.substr(i, 2);
                if (byteString.length() != 2 || !isxdigit(byteString[0]) || !isxdigit(byteString[1])) {
                    std::cerr << "Error: invalid command" << std::endl;
                    closesocket(clientSocket);
                    WSACleanup();
                    exit(RETURN_CODE_4);
                }
                unsigned char byte = static_cast<unsigned char>(strtol(byteString.c_str(), nullptr, 16));
                msg.push_back(byte);
            }
        }
        // Handle echo command - sends message to specific IP:port
        else if (message.substr(0, 3) == "/e ") {
            size_t pos = message.find(' ');
            if (pos != std::string::npos) {
                std::string target = message.substr(3, pos - 3);
                std::string text = message.substr(pos + 1);

                size_t colonPos = target.find(':');
                if (colonPos != std::string::npos) {
                    // Parse IP:port target
                    std::string ipStr = target.substr(0, colonPos);
                    std::string portStr = target.substr(colonPos + 1);

                    uint16_t port = static_cast<uint16_t>(std::stoi(portStr));
                    struct in_addr addr;
                    if (inet_pton(AF_INET, ipStr.c_str(), &addr) <= 0) {
                        std::cerr << "Invalid IP address" << std::endl;
                        continue;
                    }

                    // Construct echo request packet
                    msg.push_back(REQ_ECHO);
                    uint32_t ipNet = addr.s_addr;
                    uint16_t portNet = htons(port);

                    // Add target IP and port to message
                    msg.insert(msg.end(), reinterpret_cast<uint8_t*>(&ipNet), reinterpret_cast<uint8_t*>(&ipNet) + 4);
                    msg.insert(msg.end(), reinterpret_cast<uint8_t*>(&portNet), reinterpret_cast<uint8_t*>(&portNet) + 2);

                    // Add message length and payload
                    uint32_t textLen = static_cast<uint32_t>(text.length());
                    uint32_t textLenNet = htonl(textLen);
                    msg.insert(msg.end(), reinterpret_cast<uint8_t*>(&textLenNet), reinterpret_cast<uint8_t*>(&textLenNet) + 4);

                    msg.insert(msg.end(), text.begin(), text.end());
                }
            }
        }
        // Handle quit command
        else if (message.substr(0, 2) == "/q") {
            msg.push_back(REQ_QUIT);
            sendMessage(msg);
            break;
        }

        // Send the message if not empty
        if (!msg.empty()) {
            sendMessage(msg);
        }
    }

    file.close();
    closesocket(clientSocket);
    WSACleanup();
    exit(0); // Exit successfully after script completes
}

/**
 * Process interactive user input from the console
 */
void Client::handleUserInput() {
    std::string input;
    while (std::getline(std::cin, input)) {
        if (input.empty()) continue;

        std::vector<uint8_t> msg;

        // Handle quit command
        if (input.substr(0, 2) == "/q") {
            msg.push_back(REQ_QUIT);
            sendMessage(msg);
            break;
        }
        // Handle list users command
        else if (input.substr(0, 2) == "/l") {
            msg.push_back(REQ_LISTUSERS);
        }
        // Handle echo command - sends message to specific IP:port
        else if (input.substr(0, 3) == "/e ") {
            size_t pos = input.find(' ');
            if (pos != std::string::npos) {
                std::string target = input.substr(3, pos - 3);
                std::string text = input.substr(pos + 1);

                size_t colonPos = target.find(':');
                if (colonPos != std::string::npos) {
                    // Parse IP:port target
                    std::string ipStr = target.substr(0, colonPos);
                    std::string portStr = target.substr(colonPos + 1);

                    uint16_t port = static_cast<uint16_t>(std::stoi(portStr));
                    struct in_addr addr;
                    if (inet_pton(AF_INET, ipStr.c_str(), &addr) <= 0) {
                        std::cerr << "Invalid IP address" << std::endl;
                        continue;
                    }

                    // Construct echo request packet
                    msg.push_back(REQ_ECHO);
                    uint32_t ipNet = addr.s_addr;
                    uint16_t portNet = htons(port);

                    // Add target IP and port to message
                    msg.insert(msg.end(), reinterpret_cast<uint8_t*>(&ipNet), reinterpret_cast<uint8_t*>(&ipNet) + 4);
                    msg.insert(msg.end(), reinterpret_cast<uint8_t*>(&portNet), reinterpret_cast<uint8_t*>(&portNet) + 2);

                    // Add message length and payload
                    uint32_t textLen = static_cast<uint32_t>(text.length());
                    uint32_t textLenNet = htonl(textLen);
                    msg.insert(msg.end(), reinterpret_cast<uint8_t*>(&textLenNet), reinterpret_cast<uint8_t*>(&textLenNet) + 4);

                    msg.insert(msg.end(), text.begin(), text.end());
                }
            }
        }
        // Handle test command - sends raw hex data
        else if (input.substr(0, 2) == "/t") {
            std::string hexData = input.substr(3);
            // Convert hex string to bytes
            for (size_t i = 0; i < hexData.length(); i += 2) {
                std::string byteString = hexData.substr(i, 2);
                if (byteString.length() != 2 || !isxdigit(byteString[0]) || !isxdigit(byteString[1])) {
                    std::cerr << "Error: invalid command" << std::endl;
                    closesocket(clientSocket);
                    WSACleanup();
                    exit(RETURN_CODE_4);
                }
                unsigned char byte = static_cast<unsigned char>(strtol(byteString.c_str(), nullptr, 16));
                msg.push_back(byte);
            }
        }

        // Send the message if not empty
        if (!msg.empty()) {
            sendMessage(msg);
        }
    }
}

/**
 * Send a message to the server
 * @param message The message to send as a vector of bytes
 */
void Client::sendMessage(const std::vector<uint8_t>& message) {
    send(clientSocket, reinterpret_cast<const char*>(message.data()), static_cast<int>(message.size()), 0);
}

/**
 * Process received data from the server
 * Handles different command types and updates the receive queue
 * @param recvQueue Vector containing received data that needs processing
 */
void Client::processReceivedData(std::vector<uint8_t>& recvQueue) {
    while (!recvQueue.empty()) {
        if (recvQueue.size() < 1) break;

        // Get command ID (first byte)
        uint8_t commandId = recvQueue[0];

        // Handle echo request from another client
        if (commandId == REQ_ECHO) {
            // Check if we have enough data to process (header + at least part of message)
            if (recvQueue.size() >= 11) {
                // Extract source IP, port, and message length
                uint32_t sourceIp = *reinterpret_cast<uint32_t*>(&recvQueue[1]);
                uint16_t sourcePort = ntohs(*reinterpret_cast<uint16_t*>(&recvQueue[5])); // Convert to host order
                uint32_t messageLength = ntohl(*reinterpret_cast<uint32_t*>(&recvQueue[7])); // Convert to host order
                size_t packetSize = 11 + messageLength;

                // Check if the complete message has been received
                if (recvQueue.size() >= packetSize) {
                    // Extract message content
                    std::string message(recvQueue.begin() + 11, recvQueue.begin() + 11 + messageLength);

                    // Convert source IP to string representation
                    char sourceIpStr[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &sourceIp, sourceIpStr, INET_ADDRSTRLEN);

                    // Display received message
                    std::lock_guard<std::mutex> lock(mutex);
                    std::cout << "==========RECV START==========" << std::endl;
                    std::cout << sourceIpStr << ":" << sourcePort << std::endl; // Sender's IP:port
                    std::cout << message << std::endl; // Raw message
                    std::cout << "==========RECV END==========" << std::endl;

                    // Send RSP_ECHO with sender's IP:port and original message
                    std::vector<uint8_t> responsePacket;
                    responsePacket.push_back(RSP_ECHO);
                    responsePacket.insert(responsePacket.end(), reinterpret_cast<uint8_t*>(&sourceIp), reinterpret_cast<uint8_t*>(&sourceIp) + 4);
                    uint16_t sourcePortNet = htons(sourcePort); // Convert to network order
                    responsePacket.insert(responsePacket.end(), reinterpret_cast<uint8_t*>(&sourcePortNet), reinterpret_cast<uint8_t*>(&sourcePortNet) + 2);
                    uint32_t messageLengthNet = htonl(messageLength);
                    responsePacket.insert(responsePacket.end(), reinterpret_cast<uint8_t*>(&messageLengthNet), reinterpret_cast<uint8_t*>(&messageLengthNet) + 4);
                    responsePacket.insert(responsePacket.end(), message.begin(), message.end());

                    // Send the response
                    send(clientSocket, reinterpret_cast<const char*>(responsePacket.data()), static_cast<int>(responsePacket.size()), 0);

                    // Remove processed data from queue
                    recvQueue.erase(recvQueue.begin(), recvQueue.begin() + packetSize);
                    continue;
                }
            }
            break; // Not enough data yet, wait for more
        }

        // Handle echo response from server or another client
        else if (commandId == RSP_ECHO) {
            // Check if we have enough data to process
            if (recvQueue.size() >= 11) {
                // Extract source IP, port, and message length
                uint32_t sourceIp = *reinterpret_cast<uint32_t*>(&recvQueue[1]);
                uint16_t sourcePort = ntohs(*reinterpret_cast<uint16_t*>(&recvQueue[5])); // Convert to host order
                uint32_t messageLength = ntohl(*reinterpret_cast<uint32_t*>(&recvQueue[7])); // Convert to host order
                size_t packetSize = 11 + messageLength;

                // Check if the complete message has been received
                if (recvQueue.size() >= packetSize) {
                    // Extract message content
                    std::string message(recvQueue.begin() + 11, recvQueue.begin() + 11 + messageLength);

                    // Convert source IP to string representation
                    char sourceIpStr[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &sourceIp, sourceIpStr, INET_ADDRSTRLEN);

                    // Display received echo response
                    std::lock_guard<std::mutex> lock(mutex);
                    std::cout << "==========RECV START==========" << std::endl;
                    std::cout << sourceIpStr << ":" << sourcePort << std::endl; // Responder's IP:port
                    std::cout << message << std::endl; // Raw message
                    std::cout << "==========RECV END==========" << std::endl;

                    // Remove processed data from queue
                    recvQueue.erase(recvQueue.begin(), recvQueue.begin() + packetSize);
                    continue;
                }
            }
            break; // Not enough data yet, wait for more
        }

        // Handle response with list of connected users
        else if (commandId == RSP_LISTUSERS) {
            // Check if we have the header (command + user count)
            if (recvQueue.size() >= 3) {
                // Extract number of users
                uint16_t totalUsers = ntohs(*reinterpret_cast<uint16_t*>(&recvQueue[1]));
                size_t expectedPacketSize = 3 + (totalUsers * 6); // Header + (IP+port)*users

                // Check if we have the complete packet
                if (recvQueue.size() >= expectedPacketSize) {
                    // Display list of users
                    std::lock_guard<std::mutex> lock(mutex);
                    std::cout << "==========RECV START==========" << std::endl;
                    std::cout << "Users:" << std::endl;

                    // Process each user entry (IP + port)
                    for (size_t i = 0; i < totalUsers; ++i) {
                        uint32_t userIp = *reinterpret_cast<uint32_t*>(&recvQueue[3 + i * 6]);
                        uint16_t userPort = ntohs(*reinterpret_cast<uint16_t*>(&recvQueue[7 + i * 6]));

                        // Convert IP to string representation
                        char userIpStr[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &userIp, userIpStr, INET_ADDRSTRLEN);

                        std::cout << userIpStr << ":" << userPort << std::endl;
                    }

                    std::cout << "==========RECV END==========" << std::endl;

                    // Remove processed data from queue
                    recvQueue.erase(recvQueue.begin(), recvQueue.begin() + expectedPacketSize);
                    continue;
                }
            }
            break; // Not enough data yet, wait for more
        }

        // Handle echo error response
        else if (commandId == ECHO_ERROR) {
            std::lock_guard<std::mutex> lock(mutex);
            std::cout << "Echo error" << std::endl;
            recvQueue.erase(recvQueue.begin(), recvQueue.begin() + 1);
            continue;
        }

        // Handle unknown command - discard the byte
        else {
            recvQueue.erase(recvQueue.begin(), recvQueue.begin() + 1);
            continue;
        }
    }
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