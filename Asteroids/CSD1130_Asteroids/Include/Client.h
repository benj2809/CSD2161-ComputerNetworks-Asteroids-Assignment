#pragma once

// Critical defines MUST come first
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define NOMINMAX

// EXACT inclusion order
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>  // Must come after Winsock2
#pragma comment(lib, "ws2_32.lib")

// 3. Standard C++ headers
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <fstream>

// Return code constants for different exit conditions
constexpr int RETURN_CODE_1 = 1; // General failure
constexpr int RETURN_CODE_2 = 2; // Unused in current implementation
constexpr int RETURN_CODE_3 = 3; // Unused in current implementation
constexpr int RETURN_CODE_4 = 4; // Command format error

// Command ID enumeration - protocol definition for client-server communication
enum CMDID : unsigned char {
    UNKNOWN = 0x0,          // Unknown command
    REQ_QUIT = 0x1,         // Request to quit/disconnect
    REQ_ECHO = 0x2,         // Request to echo a message
    RSP_ECHO = 0x3,         // Response to an echo request
    REQ_LISTUSERS = 0x4,    // Request to list connected users
    RSP_LISTUSERS = 0x5,    // Response with list of users
    CMD_TEST = 0x20,        // Test command
    ECHO_ERROR = 0x30       // Error in echo operation
};
struct UdpClientData {
    sockaddr_in clientAddr;
    char data[1024];  // Buffer for incoming data
    int dataSize;     // Size of the incoming data
};
/**
 * Client class that encapsulates all client functionality.
 * Manages socket connection, network I/O, and user interaction.
 */
class Client {
public:
    // Default constructor
    Client() = default;

    // Destructor - ensures proper cleanup of resources
    ~Client() { cleanup(); }

    /**
     * Initialize the client with server connection details
     * @param serverIP The IP address of the server to connect to
     * @param serverPort The port number of the server
     * @return true if initialization successful, false otherwise
     */
    bool initialize(const std::string& serverIP, uint16_t serverPort);

    /**
     * Run the client in interactive mode - handles user input and network operations
     */
    void run();

    /**
     * Run the client in script mode - reads commands from a file
     * @param scriptPath Path to the script file containing commands
     */
    void runScript(const std::string& scriptPath);
    void getServerInfo(const std::string& scriptPath, std::string& IP, std::string& port);
    void sendToServerUdp();
private:
    SOCKET clientSocket = INVALID_SOCKET;  // Socket handle for server connection
    std::mutex mutex;                      // Mutex for thread synchronization
    std::string serverIP;                  // Server IP address
    uint16_t serverPort;                   // Server port number

    /**
     * Initialize the Winsock library
     * @return true if successful, false otherwise
     */
    bool setupWinsock();

    /**
     * Resolve server address using DNS
     * @param serverIP Server IP address
     * @param serverPort Server port number
     * @return true if address resolution successful, false otherwise
     */
    bool resolveAddress(const std::string& serverIP, uint16_t serverPort);

    /**
     * Create a TCP socket
     * @return true if socket creation successful, false otherwise
     */
    bool createSocket();

    /**
     * Connect to the server using the stored IP and port
     * @return true if connection successful, false otherwise
     */
    bool connectToServer();

    /**
     * Thread function to handle network operations
     * Continuously receives and processes data from the server
     */
    void handleNetwork();

    /**
     * Process commands from a script file
     * @param scriptPath Path to the script file
     */
    void handleScript(const std::string& scriptPath);

    /**
     * Process interactive user input from the console
     */
    void handleUserInput();

    /**
     * Send a message to the server
     * @param message The message to send as a vector of bytes
     */
    void sendMessage(const std::vector<uint8_t>& message);

    /**
     * Process received data from the server
     * Handles different command types and updates the receive queue
     * @param recvQueue Vector containing received data that needs processing
     */
    void processReceivedData(std::vector<uint8_t>& recvQueue);

    /**
     * Clean up resources (sockets, Winsock) on exit
     */
    void cleanup();
};