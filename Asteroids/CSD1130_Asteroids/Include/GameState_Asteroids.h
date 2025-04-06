/* Start Header
*******************************************************************/
/*!
\file GameState_Asteroids.h
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

#ifndef CSD1130_GAME_STATE_PLAY_H_
#define CSD1130_GAME_STATE_PLAY_H_

#include <unordered_map>
#include <Client.h>

// Game state management
void GameStateAsteroidsLoad();
void GameStateAsteroidsInit();
void GameStateAsteroidsUpdate();
void GameStateAsteroidsDraw();
void GameStateAsteroidsFree();
void GameStateAsteroidsUnload();

// Player state
static AEVec2 finalPlayerPosition;
static float playerRotate;
static int Playerscore;
AEVec2 fetchPlayerPosition();
float fetchPlayerRotation();
int fetchPlayerScore();

// Bullet state
static AEVec2 finalBulletPosition;
static float bulletRotate;
AEVec2 fetchBulletPosition();
float fetchBulletRotation();
void renderNetworkBullets();

// Asteroid state
void renderAsteroids();
void updateAsteroidInterpolation();
void checkBulletAsteroidCollisions();

// Multiplayer sync
void synchronizeShips(std::unordered_map<int, PlayerData>& pData);
void renderNames(std::unordered_map<int, PlayerData>& pData);
void displayScores();

// Others
void AEMtx33Zero(AEMtx33* mtx);

#endif