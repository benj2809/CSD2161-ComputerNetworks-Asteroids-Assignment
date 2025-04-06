/* Start Header
*******************************************************************/
/*!
\file Main.cpp
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

#include "main.h"
#include "iostream"
#include <string>           // For string manipulation
#include <memory>

// ---------------------------------------------------------------------------
// Globals
float	 globalDeltaTime;
double	 globalAppTime;
s8       fontId;
GameClient   globalClient;  // Global client object


/******************************************************************************/
/*!
	Starting application
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

	// Initializing the system
	AESysInit (instanceH, show, 800, 600, 1, 60, false, NULL);

	// Add the window title
	AESysSetWindowTitle("Assignment 4");

	// Set background color
	AEGfxSetBackgroundColor(0.0f, 0.0f, 0.0f);

	GameStateMgrInit(GS_ASTEROIDS);

	std::string serverIP;
	std::string server_Port;
	uint16_t serverPort;
	std::string path = {"../Resources/ClientInfo/client.txt"};
	fontId = AEGfxCreateFont("../Resources/Fonts/Arial Italic.ttf", 20);
	
	// Initialize & run client
	globalClient.getServerInfo(path, serverIP, server_Port);
	serverPort = static_cast<uint16_t>(std::stoul(server_Port));
	if (!globalClient.initialize(serverIP, serverPort)) {
		std::cerr << "Client initialization failed." << std::endl;
		return 1;
	}
	while(gGameStateCurr != GS_QUIT)
	{
		// Reset system
		AESysReset();

		// If not restarting, load the gamestate
		if (gGameStateCurr != GS_RESTART)
		{
			GameStateMgrUpdate();
			GameStateLoad();
		}
		else {
			gGameStateNext = gGameStateCurr = gGameStatePrev;
		}

		// Initialize the gamestate
		GameStateInit();

		while (gGameStateCurr == gGameStateNext)
		{
				AESysFrameStart();

				GameStateUpdate();
				globalClient.run();

				GameStateDraw();

				AESysFrameEnd();

				// check if forcing the application to quit
				if ((AESysDoesWindowExist() == false) || AEInputCheckTriggered(AEVK_ESCAPE)) {
					gGameStateNext = GS_QUIT;
				}

				globalDeltaTime = (f32)AEFrameRateControllerGetFrameTime();
				globalAppTime += globalDeltaTime;
		}
		
		GameStateFree();

		if (gGameStateNext != GS_RESTART) {
			GameStateUnload();
		}

		gGameStatePrev = gGameStateCurr;
		gGameStateCurr = gGameStateNext;
	}

	AEGfxDestroyFont(fontId);

	// Free system
	AESysExit();
}