/* Start Header
*******************************************************************/
/*!
\file README.txt
\co author Ho Jing Rui
\co author Saminathan Aaron Nicholas
\co author Jay Lim Jun Xiang
\par emails: jingrui.ho@digipen.edu
\	     s.aaronnicholas@digipen.edu
\	     jayjunxiang.lim@digipen.edu
\date 28 March, 2025
\brief Copyright (C) 2025 DigiPen Institute of Technology.

Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
*******************************************************************/
Multiplayer Spaceships Game

Project Overview:
This project is an implementation of a multiplayer version of a classic spaceship shooter game, similar to Asteroids.
The implementation uses a UDP-based network engine to allow up to 4 players to play over a LAN connection.
The objective is to survive and score points by shooting asteroids and competing against other players.

[Client 1]                      [Client 2]                    [Client 3]                     [Client 4]
    |                                |                              |                               |
    |--------------------------------------UDP Packet Exchange--------------------------------------|				    
    |                                |                              |                               |
[Client Network Module]        [Client Network Module]      [Client Network Module]        [Client Network Module]
    |                                |                              |                               |
[Game State Manager]           [Game State Manager]         [Game State Manager]            [Game State Manager]
    |                                |                              |                               |
[Rendering & Input]             [Rendering & Input]          [Rendering & Input]             [Rendering & Input]

                        	|-----------------------------|
                        	|            Server           |
                        	|-----------------------------|
                        	|  [Connection Manager]       |
                        	|  [Game State Sync]          |
                        	|  [Event Handler]            |
                        	|  [Score Manager]            |
                        	--------------------------------
                                    		|
                        	     [UDP Packet Broadcaster]
                                    		|
                       		    [Clients receive updates]
                                    		|
                          	      [Game Sync Module]
                                    		|
                          	      [Lock-step Mechanism]

Description:
- Clients exchange UDP packets via a server (Client-Server architecture).
- Each client maintains a local game state that must be synchronised.
- The server handles connection management, game state synchronisation, event handling, and scoring.
- A lock-step mechanism ensures gameplay consistency by delaying rendering until acknowledgment packets are received.
---------------------------------------------------------------------------------------------------------------------------

How to Run:
1. Ensure all players are connected to the same LAN network.
2. Start the server by running:
	Ensure that the application is being run on debug mode.
   	server.exe
	Enter the port number which the client will be using.
	Obtain the IP address of the server.
3. Start the client game by running:
	Amend the IP address in client.txt with the IP address shown on the server's console.
	Ensure that the application is being run on debug mode.
   	client.exe
	Up to 4 clients can be run concurrently.
4. The game will begin automatically.

Port numbers and IP addresses are expected to be changed according to each user's devices.
Ensure that each device only runs 1 client.
---------------------------------------------------------------------------------------------------------------------------

Controls:
Up / Down arrows	- Move forward
Right / Left arrows	- Rotate left / right
Spacebar 		- Fire weapon
Esc			- Quit game
---------------------------------------------------------------------------------------------------------------------------

Networking Details:
Protocol 		- UDP
Architecture 		- Client-Server
Synchronisation 	- Lock-step mechanism with acknowledgment handling
Resilience 		- Handles minor packet loss and allows reconnections

Game Features:
Asteroid synchronisation		- All clients spawn and update asteroids consistently.  
Player movement synchronisation		- Smooth and minimal delay across clients.  
Bullet synchronisation			- Bullets are accurately rendered across all players.  
Score tracking				- Real-time score updates and a leaderboard displayed at the end.  
Reconnection support 			- Disconnected players can rejoin and continue.  

Known Issues & Troubleshooting
- If the game lags, check network stability and ensure low packet loss.
- If a player disconnects, they should be able to reconnect automatically.
- Ensure UDP ports are open and firewall settings allow traffic.