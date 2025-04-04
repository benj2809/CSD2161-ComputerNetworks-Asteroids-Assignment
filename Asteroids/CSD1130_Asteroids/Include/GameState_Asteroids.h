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
void renderNetworkBullets();
//

// Asteroid
void renderServerAsteroids();
void updateAsteroidInterpolation();
void checkBulletAsteroidCollisions();
//

void syncPlayers(std::unordered_map<int, playerData>& pData);
void RenderPlayerNames(std::unordered_map<int, playerData>& pData);
void DisplayScores(const std::unordered_map<int, playerData>& players, int playerID);

// ---------------------------------------------------------------------------

#endif // CSD1130_GAME_STATE_PLAY_H_