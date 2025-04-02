/******************************************************************************/
/*!
\file		GameState_Asteroids.h
\author		Ong Jun Han Benjamin, o.junhanbenjamin, 2301532
\par		o.junhanbenjamin\@digipen.edu
\date		Feb 08, 2024
\brief		This file contains the declarations for functions:	- GameStateAsteroidsLoad;
																- GameStateAsteroidsInit;
																- GameStateAsteroidsUpdate;
																- GameStateAsteroidsDraw;
																- GameStateAsteroidsFree;
																- GameStateAsteroidsUnload;

			Defintion and documentation for each is found in GameState_Asteroids.cpp

Copyright (C) 20xx DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
/******************************************************************************/

#ifndef CSD1130_GAME_STATE_PLAY_H_
#define CSD1130_GAME_STATE_PLAY_H_

#include <unordered_map>
#include <Client.h>

// ---------------------------------------------------------------------------

void GameStateAsteroidsLoad(void);
void GameStateAsteroidsInit(void);
void GameStateAsteroidsUpdate(void);
void GameStateAsteroidsDraw(void);
void GameStateAsteroidsFree(void);
void GameStateAsteroidsUnload(void);

// Player
static AEVec2 finalPlayerPosition;
static float playerRotate;
static int Playerscore;

AEVec2 returnPlayerPosition();
float returnPlayerRotation();
int returnPlayerScore();
//

// Bullet
static AEVec2 finalBulletPosition;
static float bulletRotate;

AEVec2 returnBulletPosition();
float returnBulletRotation();
//

void syncPlayers(std::unordered_map<int, playerData>& pData);
void RenderPlayerNames(std::unordered_map<int, playerData>& pData);
// ---------------------------------------------------------------------------

#endif // CSD1130_GAME_STATE_PLAY_H_


