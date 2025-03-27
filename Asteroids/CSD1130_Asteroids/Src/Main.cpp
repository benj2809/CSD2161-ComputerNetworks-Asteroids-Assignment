/******************************************************************************/
/*!
\file		Main.cpp
\author		Ong Jun Han Benjamin, o.junhanbenjamin, 2301532
\par		o.junhanbenjamin\@digipen.edu
\date		Feb 08, 2024
\brief		This file contains the main, which will trigger the game loop.

Copyright (C) 20xx DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
/******************************************************************************/

#include "main.h"
#include "iostream"
#include <string>           // For string manipulation
#include <memory>

// ---------------------------------------------------------------------------
// Globals
float	 g_dt;
double	 g_appTime;


/******************************************************************************/
/*!
	Starting point of the application
*/
/******************************************************************************/
int WINAPI WinMain(_In_ HINSTANCE instanceH, _In_opt_ HINSTANCE prevInstanceH, _In_ LPSTR command_line, _In_ int show)
{
	UNREFERENCED_PARAMETER(prevInstanceH);
	UNREFERENCED_PARAMETER(command_line);

	// Enable run-time memory check for debug builds.
	#if defined(DEBUG) | defined(_DEBUG)
		_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
	#endif

	//int * pi = new int;
	////delete pi;
	//Get server ip and port from the user.


	// Initialize the system
	AESysInit (instanceH, show, 800, 600, 1, 60, false, NULL);

	// Changing the window title
	AESysSetWindowTitle("Asteroids Demo!");

	//set background color
	AEGfxSetBackgroundColor(0.0f, 0.0f, 0.0f);

	GameStateMgrInit(GS_ASTEROIDS);

	std::string serverIP;
	std::string server_Port;
	uint16_t serverPort;
	std::string path = {"../Resources/ClientInfo/client.txt"};
	
	// Initialize and run the client
	Client client;
	client.getServerInfo(path, serverIP, server_Port);
	serverPort = static_cast<uint16_t>(std::stoul(server_Port));
	if (!client.initialize(serverIP, serverPort)) {
		std::cerr << "Client initialization failed." << std::endl;
		return RETURN_CODE_1; 
	}
	while(gGameStateCurr != GS_QUIT)
	{
		// reset the system modules
		AESysReset();

		// If not restarting, load the gamestate
		if(gGameStateCurr != GS_RESTART)
		{
			GameStateMgrUpdate();
			GameStateLoad();
		}
		else
			gGameStateNext = gGameStateCurr = gGameStatePrev;

		// Initialize the gamestate
		GameStateInit();

		while (gGameStateCurr == gGameStateNext)
		{
				AESysFrameStart();

				GameStateUpdate();
				client.run();

				GameStateDraw();

				AESysFrameEnd();

				// check if forcing the application to quit
				if ((AESysDoesWindowExist() == false) || AEInputCheckTriggered(AEVK_ESCAPE))
					gGameStateNext = GS_QUIT;

				g_dt = (f32)AEFrameRateControllerGetFrameTime();
				g_appTime += g_dt;
		}
		
		GameStateFree();

		if(gGameStateNext != GS_RESTART)
			GameStateUnload();

		gGameStatePrev = gGameStateCurr;
		gGameStateCurr = gGameStateNext;
	}

	// free the system
	AESysExit();
}