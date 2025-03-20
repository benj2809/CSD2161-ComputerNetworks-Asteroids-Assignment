/******************************************************************************/
/*!
\file		GameStateMgr.cpp
\author		Ong Jun Han Benjamin, o.junhanbenjamin, 2301532
\par		o.junhanbenjamin\@digipen.edu
\date		Feb 08, 2024
\brief		This file contains the function pointers:	- GameStateLoad;
														- GameStateInit;
														- GameStateUpdate;
														- GameStateDraw;
														- GameStateFree;
														- GameStateUnload;

			Each of the function pointers will be set to point to their respective functions in GameState_Asteroids.cpp

Copyright (C) 20xx DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
/******************************************************************************/

#include "main.h"

// ---------------------------------------------------------------------------
// globals

// variables to keep track the current, previous and next game state
unsigned int	gGameStateInit;
unsigned int	gGameStateCurr;
unsigned int	gGameStatePrev;
unsigned int	gGameStateNext;

// pointer to functions for game state life cycles functions
void (*GameStateLoad)()		= 0;
void (*GameStateInit)()		= 0;
void (*GameStateUpdate)()	= 0;
void (*GameStateDraw)()		= 0;
void (*GameStateFree)()		= 0;
void (*GameStateUnload)()	= 0;

/******************************************************************************/
/*!
	Function to initialize gGameStateCurr, gGameStatePrev and gGameStateNext to gGameStateInit, which is assigned the input gameStateInit.
*/
/******************************************************************************/
void GameStateMgrInit(unsigned int gameStateInit)
{
	// set the initial game state
	gGameStateInit = gameStateInit;

	// reset the current, previous and next game
	gGameStateCurr = 
	gGameStatePrev = 
	gGameStateNext = gGameStateInit;

	// call the update to set the function pointers
	GameStateMgrUpdate();
}

/******************************************************************************/
/*!
	Function to update each function pointer to point to their respective functions in GameState_Asteroids.cpp.
*/
/******************************************************************************/
void GameStateMgrUpdate()
{
	if ((gGameStateCurr == GS_RESTART) || (gGameStateCurr == GS_QUIT)) // If the current game state is either GS_RESTART or GS_QUIT, return and don't assign the pointers.
		return;

	switch (gGameStateCurr) // Switch case
	{
	case GS_ASTEROIDS: // Case current game state is GS_ASTEROID (the game), point all function pointers to point at their respective functions in GameState_Asteroids.cpp.
		GameStateLoad = GameStateAsteroidsLoad;
		GameStateInit = GameStateAsteroidsInit;
		GameStateUpdate = GameStateAsteroidsUpdate;
		GameStateDraw = GameStateAsteroidsDraw;
		GameStateFree = GameStateAsteroidsFree;
		GameStateUnload = GameStateAsteroidsUnload;
		break;
	default:	// Should the state be invalid. (i.e. GS_NONE).
		AE_FATAL_ERROR("invalid state!!");
	}
}
