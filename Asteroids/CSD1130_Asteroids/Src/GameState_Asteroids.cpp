/* Start Header
*******************************************************************/
/*!
\file GameState_Asteroids.cpp
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
#include <iostream>

/******************************************************************************/
/*!
	Defines
*/
/******************************************************************************/
const unsigned int	GAME_OBJ_NUM_MAX = 32;			// The total number of different objects (Shapes)
const unsigned int	GAME_OBJ_INST_NUM_MAX = 2048;			// The total number of different game object instances


const unsigned int	SHIP_INITIAL_NUM = 3;			// initial number of ship lives
const float			SHIP_SCALE_X = 16.0f;		// ship scale x
const float			SHIP_SCALE_Y = 16.0f;		// ship scale y
const float			BULLET_SCALE_X = 20.0f;		// bullet scale x
const float			BULLET_SCALE_Y = 3.0f;			// bullet scale y
const float			ASTEROID_MIN_SCALE_X = 10.0f;		// asteroid minimum scale x
const float			ASTEROID_MAX_SCALE_X = 60.0f;		// asteroid maximum scale x
const float			ASTEROID_MIN_SCALE_Y = 10.0f;		// asteroid minimum scale y
const float			ASTEROID_MAX_SCALE_Y = 60.0f;		// asteroid maximum scale y

const float			WALL_SCALE_X = 64.0f;		// wall scale x
const float			WALL_SCALE_Y = 164.0f;		// wall scale y

const float			SHIP_ACCEL_FORWARD = 100.0f;		// ship forward acceleration (in m/s^2)
const float			SHIP_ACCEL_BACKWARD = 100.0f;		// ship backward acceleration (in m/s^2)
const float			SHIP_ROT_SPEED = (2.0f * PI);	// ship rotation speed (degree/second)

const float			BULLET_SPEED = 400.0f;		// bullet speed (m/s)

const float         BOUNDING_RECT_SIZE = 1.0f;         // this is the normalized bounding rectangle (width and height) sizes - AABB collision data

static bool onValueChange{ true };
float PlayerData::gameTimer = 60.0f;
bool gameOver = false;
bool winnerAnnounced = false;
static bool spaceDebounce = false;

int playerCount = 1;

// -----------------------------------------------------------------------------
enum TYPE
{
	// list of game object types
	TYPE_SHIP = 0,
	TYPE_BULLET,
	TYPE_ASTEROID,
	TYPE_WALL,

	TYPE_NUM
};

// -----------------------------------------------------------------------------
// object flag definition

const unsigned long FLAG_ACTIVE = 0x00000001;

/******************************************************************************/
/*!
	Struct/Class Definitions
*/
/******************************************************************************/

//Game object structure
struct GameObj
{
	unsigned long		type;		// object type
	AEGfxVertexList* pMesh;		// This will hold the triangles which will form the shape of the object
};

// ---------------------------------------------------------------------------

//Game object instance structure
struct GameObjInst
{
	GameObj* pObject;	// pointer to the 'original' shape
	unsigned long		flag;		// bit flag or-ed together
	AEVec2				scale;		// scaling value of the object instance
	AEVec2				posCurr;	// object current position

	AEVec2				posPrev;	// object previous position -> it's the position calculated in the previous loop

	AEVec2				velCurr;	// object current velocity

	float				dirCurr;	// object current direction
	AABB				boundingBox;// object bouding box that encapsulates the object
	AEMtx33				transform;	// object transformation matrix: Each frame, 
	// calculate the object instance's transformation matrix and save it here

};

/******************************************************************************/
/*!
	Static Variables
*/
/******************************************************************************/

// list of original object
static GameObj				sGameObjList[GAME_OBJ_NUM_MAX];				// Each element in this array represents a unique game object (shape)
static unsigned long		sGameObjNum;								// The number of defined game objects

// list of object instances
static GameObjInst			sGameObjInstList[GAME_OBJ_INST_NUM_MAX];	// Each element in this array represents a unique game object instance (sprite)
static unsigned long		sGameObjInstNum;							// The number of used game object instances

// pointer to the ship object
static GameObjInst* spShip;										// Pointer to the "Ship" game object instance

// pointer to the wall object
static GameObjInst* spWall;										// Pointer to the "Wall" game object instance

// number of ship available (lives 0 = game over)
static long					sShipLives;									// The number of lives left

// the score = number of asteroid destroyed
static unsigned long		sScore;										// Current score

// ---------------------------------------------------------------------------

// functions to create/destroy a game object instance
GameObjInst* gameObjInstCreate(unsigned long type, AEVec2* scale,
	AEVec2* pPos, AEVec2* pVel, float dir);
void				gameObjInstDestroy(GameObjInst* pInst);

void				Helper_Wall_Collision();

std::unordered_map<int, GameObjInst*> pShips; // Player ships
std::vector<GameObjInst*> pBullets; // Bullets fired by this player

extern std::mutex asteroidsMutex;
extern std::unordered_map<std::string, AsteroidData> asteroids;

extern std::unordered_map<std::string, BulletData> bullets;
extern Client globalClient;

/******************************************************************************/
/*!
	"Load" function of this state
*/
/******************************************************************************/
void GameStateAsteroidsLoad(void)
{
	// zero the game object array
	memset(sGameObjList, 0, sizeof(GameObj) * GAME_OBJ_NUM_MAX);
	// No game objects (shapes) at this point
	sGameObjNum = 0;

	// zero the game object instance array
	memset(sGameObjInstList, 0, sizeof(GameObjInst) * GAME_OBJ_INST_NUM_MAX);
	// No game object instances (sprites) at this point
	sGameObjInstNum = 0;

	// The ship object instance hasn't been created yet, so this "spShip" pointer is initialized to 0
	spShip = nullptr;
	spWall = nullptr;

	// load/create the mesh data (game objects / Shapes)
	GameObj* pObj;

	// =====================
	// create the ship shape
	// =====================
	pObj = sGameObjList + sGameObjNum++;
	pObj->type = TYPE_SHIP;

	AEGfxMeshStart();
	AEGfxTriAdd(
		-1.0f, 1.0f, 0xFFFF0000, 0.0f, 0.0f,
		-1.0f, -1.0f, 0xFFFF0000, 0.0f, 0.0f,
		1.5f, 0.0f, 0xFFFF0000, 0.0f, 0.0f);

	pObj->pMesh = AEGfxMeshEnd();
	AE_ASSERT_MESG(pObj->pMesh, "fail to create object!!");


	// =======================
	// create the bullet shape
	// =======================
	pObj = sGameObjList + sGameObjNum++;
	pObj->type = TYPE_BULLET;

	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFFFFFF00, 0.0f, 0.0f,
		0.5f, 0.5f, 0xFFFFFF00, 0.0f, 0.0f,
		-0.5f, 0.5f, 0xFFFFFF00, 0.0f, 0.0f);
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFFFFFF00, 0.0f, 0.0f,
		0.5f, -0.5f, 0xFFFFFF00, 0.0f, 0.0f,
		0.5f, 0.5f, 0xFFFFFF00, 0.0f, 0.0f);

	pObj->pMesh = AEGfxMeshEnd();
	AE_ASSERT_MESG(pObj->pMesh, "fail to create object!!");

	// =========================
	// create the asteroid shape
	// =========================
	pObj = sGameObjList + sGameObjNum++;
	pObj->type = TYPE_ASTEROID;

	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFF808080, 0.0f, 0.0f,
		0.5f, 0.5f, 0xFF808080, 0.0f, 0.0f,
		-0.5f, 0.5f, 0xFF808080, 0.0f, 0.0f);
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFF808080, 0.0f, 0.0f,
		0.5f, -0.5f, 0xFF808080, 0.0f, 0.0f,
		0.5f, 0.5f, 0xFF808080, 0.0f, 0.0f);

	pObj->pMesh = AEGfxMeshEnd();
	AE_ASSERT_MESG(pObj->pMesh, "fail to create object!!");

	// =========================
	// create the wall shape
	// =========================
	pObj = sGameObjList + sGameObjNum++;
	pObj->type = TYPE_WALL;

	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f, -0.5f, 0x6600FF00, 0.0f, 0.0f,
		0.5f, 0.5f, 0x6600FF00, 0.0f, 0.0f,
		-0.5f, 0.5f, 0x6600FF00, 0.0f, 0.0f);
	AEGfxTriAdd(
		-0.5f, -0.5f, 0x6600FF00, 0.0f, 0.0f,
		0.5f, -0.5f, 0x6600FF00, 0.0f, 0.0f,
		0.5f, 0.5f, 0x6600FF00, 0.0f, 0.0f);

	pObj->pMesh = AEGfxMeshEnd();
	AE_ASSERT_MESG(pObj->pMesh, "fail to create object!!");
	// 0   - Ship
	// 1   - Bullet
	// 2   - Asteroid
	// 3   - Wall
}

/******************************************************************************/
/*!
	"Initialize" function of this state
*/
/******************************************************************************/
void GameStateAsteroidsInit(void)
{
	// Clear any existing player ships from previous sessions
	for (auto it = pShips.begin(); it != pShips.end(); ++it) {
		if (it->second) {
			gameObjInstDestroy(it->second);
		}
	}
	pShips.clear();

	// Clear any bullets from previous sessions
	for (auto it = pBullets.begin(); it != pBullets.end(); ++it) {
		if (*it) {
			gameObjInstDestroy(*it);
		}
	}
	pBullets.clear();

	// create the main ship
	AEVec2 scale;
	AEVec2Set(&scale, SHIP_SCALE_X, SHIP_SCALE_Y);
	spShip = gameObjInstCreate(TYPE_SHIP, &scale, nullptr, nullptr, 0.0f);
	AE_ASSERT(spShip);

	// reset the score and the number of ships
	sScore = 0;
	sShipLives = SHIP_INITIAL_NUM;
}

/******************************************************************************/
/*!
	"Update" function of this state
*/
/******************************************************************************/
void GameStateAsteroidsUpdate(void)
{
	// Track previous scores to detect changes
	static std::unordered_map<int, int> previousScores;
	static bool scoresChanged = false;

	// Check if any scores have changed
	scoresChanged = false;
	for (const auto& pair : players) {
		int id = pair.first;
		int currentScore = pair.second.score;

		// Check if this player's score is different from last time
		if (previousScores.find(id) == previousScores.end() || previousScores[id] != currentScore) {
			previousScores[id] = currentScore;
			scoresChanged = true;
		}

		// Check if any player has reached 1000 points (new game over condition)
		if (currentScore >= 1000 && !gameOver) {
			gameOver = true;
			std::cout << "\n=== GAME OVER ===\n";
			std::cout << "Player " << id << " has reached 1000 points!" << std::endl;
		}
	}

	// Also check if the number of players changed
	if (previousScores.size() != players.size()) {
		scoresChanged = true;
	}

	// Display scores only when they've changed
	if (scoresChanged) {
		Client::displayPlayerScores();
	}

	// Clean up scores for players who are no longer present
	for (auto it = previousScores.begin(); it != previousScores.end();) {
		if (players.find(it->first) == players.end()) {
			it = previousScores.erase(it);
		}
		else {
			++it;
		}
	}

	// Calculate delta time
	float deltaTime = (float)AEFrameRateControllerGetFrameTime();

	// If the game is over, you might want to return early to skip further game logic
	if (gameOver && !winnerAnnounced) {
		// Announce the winner when the game is over
		// Find the highest score
		int maxScore = -1;
		std::unordered_map<int, std::string> winnerIDs;  // Store player IDs if there are ties
		for (const auto& pair : players) {
			if (pair.second.score > maxScore) {
				maxScore = pair.second.score;
				winnerIDs.clear();  // Clear previous ties, as we found a new max score
				winnerIDs[pair.first] = "";  // Store player ID
			}
			else if (pair.second.score == maxScore) {
				winnerIDs[pair.first] = "";  // Add the tied player ID
			}
		}

		// Output the winner(s) only once
		if (winnerIDs.size() == 1) {
			std::cout << "\n=== WINNER ===\n";
			std::cout << "The winner is player with ID: " << winnerIDs.begin()->first << " with " << maxScore << " points!" << std::endl;
		}
		else {
			std::cout << "\n=== IT'S A TIE ===\n";
			std::cout << "The following players tied with " << maxScore << " points:\n";
			for (const auto& winner : winnerIDs) {
				std::cout << "Player ID: " << winner.first << std::endl;
			}
		}
		winnerAnnounced = true;

		return;  // Exit early to skip the rest of the game logic
	}

	if (!(sScore >= 5000)) {
		if (!(sShipLives < 0)) {
			if (AEInputCheckCurr(AEVK_UP)) // Moving forward
			{
				AEVec2 added, newVel{}, newPos{};
				AEVec2Set(&added, cosf(spShip->dirCurr), sinf(spShip->dirCurr)); // Creating vector of the direction of the acceleration.
				AEVec2Normalize(&added, &added); // Normalizing the vector
				added.x *= SHIP_ACCEL_FORWARD; // * Acceleration
				added.y *= SHIP_ACCEL_FORWARD; // * Acceleration
				AEVec2Scale(&added, &added, (float)AEFrameRateControllerGetFrameTime()); // * deltaTime - Makes time-based movement, which will cover the same amount of distance regardless of frame rate.
				AEVec2Add(&newVel, &added, &spShip->velCurr); // * Adding everything to vector newVal
				AEVec2Scale(&newVel, &newVel, 0.99f); // * 0.99 - This acts as 'friction', for each frame, the percentage decrease gets bigger, until it reachs 100%, which 'limits' our speed.
				spShip->velCurr = newVel; // Assigning vector newVal to the ship's velocity.
			}

			if (AEInputCheckCurr(AEVK_DOWN)) // Moving backwards - The steps below are similar to the steps for forward movement.
			{								 //                    One change is that we negate the acceleration, which results in moving the opposite direction.
				AEVec2 added{}, newVel{};
				AEVec2Set(&added, cosf(spShip->dirCurr), sinf(spShip->dirCurr));
				AEVec2Normalize(&added, &added);
				added.x *= -SHIP_ACCEL_BACKWARD;
				added.y *= -SHIP_ACCEL_BACKWARD;
				AEVec2Scale(&added, &added, (float)AEFrameRateControllerGetFrameTime());
				AEVec2Add(&newVel, &added, &spShip->velCurr);
				AEVec2Scale(&newVel, &newVel, 0.99f);
				spShip->velCurr = newVel;
			}

			if (AEInputCheckCurr(AEVK_LEFT)) // Rotating the ship anti-clockwise.
			{
				spShip->dirCurr += SHIP_ROT_SPEED * (float)(AEFrameRateControllerGetFrameTime());
				spShip->dirCurr = AEWrap(spShip->dirCurr, -PI, PI);
			}

			if (AEInputCheckCurr(AEVK_RIGHT)) // Rotating the ship clockwise.
			{
				spShip->dirCurr -= SHIP_ROT_SPEED * (float)(AEFrameRateControllerGetFrameTime());
				spShip->dirCurr = AEWrap(spShip->dirCurr, -PI, PI);
			}


			// Shoot a bullet if space is triggered (Create a new object instance)
// Shoot a bullet if space is triggered (Create a new object instance)
			if (AEInputCheckTriggered(AEVK_SPACE) && !spaceDebounce) {
				spaceDebounce = true; // Set debounce flag to prevent multiple bullets

				// Create a bullet in the direction the ship is facing
				AEVec2 bulletDir;
				AEVec2Set(&bulletDir, cosf(spShip->dirCurr), sinf(spShip->dirCurr));

				// Calculate bullet velocity
				AEVec2 bulletVel;
				AEVec2Scale(&bulletVel, &bulletDir, BULLET_SPEED);

				// Calculate bullet starting position (at the front of the ship)
				AEVec2 bulletPos;
				AEVec2 shipFront;
				AEVec2Scale(&shipFront, &bulletDir, SHIP_SCALE_X * 0.5f);
				AEVec2Add(&bulletPos, &spShip->posCurr, &shipFront);

				// Create bullet scale
				AEVec2 bulletScale;
				AEVec2Set(&bulletScale, BULLET_SCALE_X, BULLET_SCALE_Y);

				// Create the bullet game object
				GameObjInst* bullet = gameObjInstCreate(TYPE_BULLET, &bulletScale, &bulletPos, &bulletVel, spShip->dirCurr);

				if (bullet) {
					// Generate a unique ID for the bullet
					std::string bulletID = std::to_string(Client::getPlayerID()) + "_" +
						std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());

					// Add to local bullet list
					pBullets.push_back(bullet);

					// Add to global bullets map with fromLocalPlayer flag to avoid duplication
					Client::lockBullets();
					BulletData bData;
					bData.bulletID = bulletID;
					bData.X = bulletPos.x;
					bData.X = bulletPos.y;
					bData.velocityX = bulletVel.x;
					bData.velocityY = bulletVel.y;
					bData.direction = spShip->dirCurr;
					bData.fromLocalPlayer = true; // This flag is critical!
					bullets[bulletID] = bData;
					Client::unlockBullets();

					// Report to server
					globalClient.sendBulletCreationEvent(bulletPos, bulletVel, spShip->dirCurr, bulletID);

					std::cout << "Created local bullet with ID: " << bulletID << std::endl;
				}
			}
			else if (!AEInputCheckCurr(AEVK_SPACE)) {
				// Reset debounce flag when space is released
				spaceDebounce = false;
			}
		}
	}

	// ======================================================================
	// Save previous positions
	//  -- For all instances
	// [DO NOT UPDATE THIS PARAGRAPH'S CODE]
	// ======================================================================
	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjInst* pInst = sGameObjInstList + i;

		// skip non-active object
		if ((pInst->flag & FLAG_ACTIVE) == 0)
			continue;

		pInst->posPrev.x = pInst->posCurr.x;
		pInst->posPrev.y = pInst->posCurr.y;
	}

	// ======================================================================
	// update physics of all active game object instances
	//  -- Calculate the AABB bounding rectangle of the active instance, using the starting position:
	//		boundingRect_min = -(BOUNDING_RECT_SIZE/2.0f) * instance->scale + instance->posPrev
	//		boundingRect_max = +(BOUNDING_RECT_SIZE/2.0f) * instance->scale + instance->posPrev
	//
	//	-- New position of the active instance is updated here with the velocity calculated earlier
	// ======================================================================
	for (int i = 0; i < GAME_OBJ_INST_NUM_MAX; ++i) { // For loop to iterate all object instances and creating a bounding box for each of them, this bounding box will be used for collision detection.
		GameObjInst* pInst = sGameObjInstList + i;

		pInst->boundingBox.min.x = -(BOUNDING_RECT_SIZE / 2.f) * pInst->scale.x + pInst->posPrev.x;
		pInst->boundingBox.min.y = -(BOUNDING_RECT_SIZE / 2.f) * pInst->scale.y + pInst->posPrev.y;
		pInst->boundingBox.max.x = (BOUNDING_RECT_SIZE / 2.f) * pInst->scale.x + pInst->posPrev.x;
		pInst->boundingBox.max.y = (BOUNDING_RECT_SIZE / 2.f) * pInst->scale.y + pInst->posPrev.y;

		pInst->posCurr.x += pInst->velCurr.x * (float)AEFrameRateControllerGetFrameTime(); // Updating position of the object instance.
		pInst->posCurr.y += pInst->velCurr.y * (float)AEFrameRateControllerGetFrameTime();
	}

	// ======================================================================
	// check for dynamic-static collisions (one case only: Ship vs Wall)
	// [DO NOT UPDATE THIS PARAGRAPH'S CODE]
	// ======================================================================
	// Helper_Wall_Collision(); // More information can be found below, at the definition of the function. TLDR; calls function CollisionIntersection_RectRect, definition found in Collision.cpp.

	//======================================================================
	//check for dynamic-dynamic collisions [AA-BB] 
	//======================================================================
	if (!(sScore >= 5000)) { // Just like movement, collision only takes place when the game is active.
		if (!(sShipLives < 0)) {
			for (int i = 0; i < GAME_OBJ_INST_NUM_MAX; ++i) { // For loop 1: This will iterate for each game object instance.
				GameObjInst* pInst = sGameObjInstList + i; // To access each game object instance...
				if ((pInst->flag & FLAG_ACTIVE) == 0) continue; // If the instance is not active (indicated by pInst->flag, which is 1 when the instance is active), continue to the next iteration.
				if (pInst->pObject->type == TYPE_ASTEROID) { // Checking if the current instance is of type ASTEROID, which is the instance we are checking for collision for.
					for (int j = 0; j < GAME_OBJ_INST_NUM_MAX; ++j) { // For loop 2: To iterate through all the other instances to check for collision with the ASTEROID.
						GameObjInst* NewpInst = sGameObjInstList + j; // To access the other instances...
						if ((NewpInst->flag & FLAG_ACTIVE) == 0) continue; // If the current instance is not active, skip.
						if (NewpInst->pObject->type == TYPE_ASTEROID) continue; // If the current instance is of type ASTEROID as well, skip, since there is no ASTEROID - ASTEROID collision.

						float tFirst = 0.0f;

						if (NewpInst->pObject->type == TYPE_SHIP) { // If the current instance is the ship, check for collision.
							if (CollisionIntersection_RectRect(pInst->boundingBox, pInst->velCurr,
								spShip->boundingBox, spShip->velCurr,
								tFirst)) { // This if condition checks between the min & max for the SHIP x/y coordinates against the ASTEROID																																																										// It is important to check for both ASTEROID - SHIP and SHIP - ASTEROID collisions, this confirms there is a collision b/w both objects																																																						// If either one of the checks is to be omitted, the will be a case where no collision is detected when either object is WITHIN the other.
								gameObjInstDestroy(pInst); // Destroy the ASTEROID if there is collision																																									
								gameObjInstDestroy(spShip); // Likewise destroy the ship
								AEVec2 scale{ SHIP_SCALE_X, SHIP_SCALE_Y }; // The following vectors are the initial vectors for the ship.
								spShip = gameObjInstCreate(TYPE_SHIP, &scale, nullptr, nullptr, 0.0f);
								AEVec2 scale2{ (f32)(rand() % (60 - 10 + 1) + 10) , (f32)(rand() % (60 - 10 + 1) + 10) }, pos{ (f32)(rand() % (900 - (-500) + 1) + (-500)), 400.f }, vel{ (f32)(rand() % (100 - (-100) + 1) + (-100)),  (f32)(rand() % (100 - (-100) + 1) + (-100)) };
								gameObjInstCreate(TYPE_ASTEROID, &scale2, &pos, &vel, 0.0f); // Creating a new ship at the the center of the screen.
								--sShipLives; // Decrement ship lives
								onValueChange = true; // Setting this to true to print to the user their current score and amount of ship lives left.
							}
						}
						else if (NewpInst->pObject->type == TYPE_BULLET) { // If the current instance is a bullet, eheck for collision.
							if (CollisionIntersection_RectRect(pInst->boundingBox, pInst->velCurr,
								NewpInst->boundingBox, NewpInst->velCurr,
								tFirst)) { // ASTEROID - BULLET and BULLET - ASTEROID collision checks are account for to prevent false collisions.

								// Removing pointer of destroyed bullet from pBullets container.
								pBullets.erase(std::remove_if(pBullets.begin(), pBullets.end(),
									[pInst](GameObjInst* bullet) {
										return bullet = pInst;
									}), pBullets.end());

								gameObjInstDestroy(pInst); // Destroy the ASTEROID if there is collision.
								gameObjInstDestroy(NewpInst); // Likewise destroy the bullet.
								AEVec2 scale{ (f32)(rand() % (60 - 10 + 1) + 10), (f32)(rand() % (60 - 10 + 1) + 10) }, pos{ (f32)(rand() % (900 - (-500) + 1) + (-500)), 400.f }, vel{ (f32)(rand() % (100 - (-100) + 1) + (-100)), (f32)(rand() % (100 - (-100) + 1) + (-100)) }; // Setting random values for the new ASTEROID to be created.
								gameObjInstCreate(TYPE_ASTEROID, &scale, &pos, &vel, 0.0f); // Creating a new ASTEROID instance with random values.
								AEVec2Scale(&pos, &pos, -1.f); // Modifying position for the 2nd ASTEROID
								AEVec2Scale(&vel, &vel, -1.3f); // Modifying velocity for the 2nd ASTEROID
								gameObjInstCreate(TYPE_ASTEROID, &scale, &pos, &vel, 0.0f); // Creating a new ASTEROID with modified values. 
								sScore += 100; // Incrementing the score.
								onValueChange = true; // Setting to print the current score and number of ship lives left.
							}
						}
					}
				}
			}
		}
	}

	// ===================================================================
	// update active game object instances
	// Example:
	//		-- Wrap specific object instances around the world (Needed for the assignment)
	//		-- Removing the bullets as they go out of bounds (Needed for the assignment)
	//		-- If you have a homing missile for example, compute its new orientation 
	//			(Homing missiles are not required for the Asteroids project)
	//		-- Update a particle effect (Not required for the Asteroids project)
	// ===================================================================
	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjInst* pInst = sGameObjInstList + i;

		// skip non-active object
		if ((pInst->flag & FLAG_ACTIVE) == 0)
			continue;

		// check if the object is a ship
		if (pInst->pObject->type == TYPE_SHIP) // Checking for ship.
		{
			// Wrap the ship from one end of the screen to the other
			pInst->posCurr.x = AEWrap(pInst->posCurr.x, AEGfxGetWinMinX() - SHIP_SCALE_X,	// Wrapping the x-coordinates of the ship to the opposite end of the screen, should it go out of bounds.
				AEGfxGetWinMaxX() + SHIP_SCALE_X);
			pInst->posCurr.y = AEWrap(pInst->posCurr.y, AEGfxGetWinMinY() - SHIP_SCALE_Y,   // Likewise, wrapping the y-coordinates of the ship to the opposite end of the screen, should it go out of bounds.
				AEGfxGetWinMaxY() + SHIP_SCALE_Y);
			//update ship position
		}

		// Wrap asteroids here

		if (pInst->pObject->type == TYPE_ASTEROID) { // Checking for asteroid.
			pInst->posCurr.x = AEWrap(pInst->posCurr.x, AEGfxGetWinMinX() - ASTEROID_MAX_SCALE_X,	// Wrapping the x-coordinate of the asteroid to the opposite end of the screen, should it go out of bounds.
				AEGfxGetWinMaxX() + ASTEROID_MAX_SCALE_X);
			pInst->posCurr.y = AEWrap(pInst->posCurr.y, AEGfxGetWinMinY() - ASTEROID_MAX_SCALE_Y,	// Likewise, wrapping the y-coordinates of the asteroid to the opposite end of the screen, should it go out of bounds.
				AEGfxGetWinMaxY() + ASTEROID_MAX_SCALE_Y);
		}

		// Remove bullets that go out of bounds
		if (pInst->pObject->type == TYPE_BULLET) { // Checking for bullet.
			if (pInst->posCurr.x > (AEGfxGetWindowWidth() / 2.f) || pInst->posCurr.x < -(AEGfxGetWindowWidth() / 2.f)    // Checking if the bullets position falls out of bounds, in our case the bound anything within the screen width and height.
				|| pInst->posCurr.y >(AEGfxGetWindowHeight() / 2.f) || pInst->posCurr.y < -(AEGfxGetWindowHeight() / 2.f))
				gameObjInstDestroy(pInst);																				 // Destroying the bullet should it go out of bounds.
		}
	}

	// =====================================================================
	// calculate the matrix for all objects
	// =====================================================================

	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjInst* pInst = sGameObjInstList + i; // Iterate through each instance.
		AEMtx33		 trans{}, rot{}, scale{}; // Vectors for matrices.

		/*UNREFERENCED_PARAMETER(trans);
		UNREFERENCED_PARAMETER(rot);
		UNREFERENCED_PARAMETER(scale);*/

		// skip non-active object
		if ((pInst->flag & FLAG_ACTIVE) == 0) // Skip if instance is not active.
			continue;

		// Compute the scaling matrix
		AEMtx33Scale(&scale, pInst->scale.x, pInst->scale.y); // The scaling matrix consists of the instances scale x/y.

		// Compute the rotation matrix 
		AEMtx33Rot(&rot, pInst->dirCurr); // The rotation matrix consists of the instances current directon.

		// Compute the translation matrix
		AEMtx33Trans(&trans, pInst->posCurr.x, pInst->posCurr.y); // The translation matrix consists of the instances current position x/y.

		// Concatenate the 3 matrix in the correct order in the object instance's "transform" matrix, the correct order being Scale * Rotate * Translate = Transform.
		AEMtx33Concat(&pInst->transform, &rot, &scale);
		AEMtx33Concat(&pInst->transform, &trans, &pInst->transform);
	}

	for (const auto& pair : players) {

		if (pair.second.playerID != Client::getPlayerID()) {
			continue;
		}
		finalPlayerPosition = { spShip->posCurr.x, spShip->posCurr.y };
		playerRotate = spShip->dirCurr;
		break;
	}

	// Update bullet positions
	for (auto it = pBullets.begin(); it != pBullets.end();) {
		GameObjInst* bullet = *it;
		if (!bullet || !(bullet->flag & FLAG_ACTIVE)) {
			it = pBullets.erase(it);
			continue;
		}

		// Update bullet position based on velocity
		bullet->posCurr.x += bullet->velCurr.x * globalDeltaTime;
		bullet->posCurr.y += bullet->velCurr.y * globalDeltaTime;

		// Check if bullet is out of bounds
		if (bullet->posCurr.x < AEGfxGetWinMinX() - BULLET_SCALE_X ||
			bullet->posCurr.x > AEGfxGetWinMaxX() + BULLET_SCALE_X ||
			bullet->posCurr.y < AEGfxGetWinMinY() - BULLET_SCALE_Y ||
			bullet->posCurr.y > AEGfxGetWinMaxY() + BULLET_SCALE_Y) {

			gameObjInstDestroy(bullet);
			it = pBullets.erase(it);
		}
		else {
			// Update bullet transformation
			AEMtx33 scale, rot, trans;
			AEMtx33Scale(&scale, bullet->scale.x, bullet->scale.y);
			AEMtx33Rot(&rot, bullet->dirCurr);
			AEMtx33Trans(&trans, bullet->posCurr.x, bullet->posCurr.y);

			AEMtx33Concat(&bullet->transform, &rot, &scale);
			AEMtx33Concat(&bullet->transform, &trans, &bullet->transform);

			++it;
		}
	}

	// Update network bullets
	Client::lockBullets();
	for (auto& pair : bullets) {
		// Update bullet position
		pair.second.X += pair.second.velocityX * globalDeltaTime;
		pair.second.Y += pair.second.velocityY * globalDeltaTime;
	}
	Client::unlockBullets();

	// Update asteroids interpolation
	updateAsteroidInterpolation();

	// Check collisions between bullets and asteroids
	checkBulletAsteroidCollisions();

	syncPlayers(players);

	// Make sure our local player's ship is properly tracked
	for (const auto& pair : players) {
		if (pair.second.playerID == Client::getPlayerID()) {
			// Update our local position reference but keep using spShip for local control
			finalPlayerPosition = { spShip->posCurr.x, spShip->posCurr.y };
			playerRotate = spShip->dirCurr;

			// Get the current score from the server data
			Playerscore = pair.second.score;
			break;
		}
	}
}

/******************************************************************************/
/*!
	Draw function for all the instances.
	Namely the ship, bullets, asteroids and the static wall.
*/
/******************************************************************************/
void GameStateAsteroidsDraw(void)
{
	char strBuffer[1024]; // To store the score and ship lives to print the user whenever there is an update to either values.

	AEGfxSetRenderMode(AE_GFX_RM_COLOR); // Render mode.
	AEGfxTextureSet(NULL, 0, 0); // No texture.

	// Set blend mode to AE_GFX_BM_BLEND.
	// This will allow transparency.
	AEGfxSetBlendMode(AE_GFX_BM_BLEND);
	AEGfxSetTransparency(1.0f);

	// draw all object instances in the list
	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjInst* pInst = sGameObjInstList + i; // Accessing each instance...

		// skip non-active object
		if ((pInst->flag & FLAG_ACTIVE) == 0) // Skip...
			continue;

		// Set the current object instance's transform matrix using "AEGfxSetTransform".
		AEGfxSetTransform(pInst->transform.m);

		// Draw the shape used by the current object instance using "AEGfxMeshDraw".
		AEGfxMeshDraw(pInst->pObject->pMesh, AE_GFX_MDM_TRIANGLES);
	}

	renderServerAsteroids();
	renderNetworkBullets();

	// Displaying ship lives and score values to user should there be an update to either values.
	if (onValueChange)
	{
		//sprintf_s(strBuffer, "Ship Left: %d", sShipLives >= 0 ? sShipLives : 0);
		//printf("%s \n", strBuffer);
		onValueChange = false;
		// display the game over message
		if (sShipLives < 0)
		{
			printf("       GAME OVER       \n");
		}
		if (sScore == 5000) {
			printf("       YOU ROCK!       \n");
		}
	}

	// Display GAME OVER on screen when the game is over
	if (gameOver)
	{
		// Draw a large red "GAME OVER" text in the center of the screen
		AEGfxSetRenderMode(AE_GFX_RM_COLOR);
		AEGfxSetBlendMode(AE_GFX_BM_BLEND);
		AEGfxTextureSet(NULL, 0, 0);
		AEGfxSetTransparency(1.0f);

		// Red color: RGB(1.0, 0.0, 0.0)
		AEGfxPrint(fontId, "GAME OVER", -0.2f, 0.0f, 2.0f, 1.0f, 0.0f, 0.0f, 1.0f);
	}

	// Don't display timer anymore - removed timer display

	RenderPlayerNames(players);

	for (auto p : players)
	{
		int id = p.first;
		DisplayScores(players, id);
	}
}
/******************************************************************************/
/*!
	Function to free all instances, essentially 'killing' them.
*/
/******************************************************************************/
void GameStateAsteroidsFree(void)
{
	// kill all object instances in the array using "gameObjInstDestroy" - by iterating through the whole array using a for loop.
	for (int i = 0; i < GAME_OBJ_INST_NUM_MAX; ++i) {
		GameObjInst* pInst = sGameObjInstList + i;
		if (pInst->flag & FLAG_ACTIVE) {
			gameObjInstDestroy(pInst);
		}
	}
}

/******************************************************************************/
/*!
	Function to unload all meshes created at the start.
*/
/******************************************************************************/
void GameStateAsteroidsUnload(void)
{
	for (unsigned long i = 0; i < sGameObjNum; ++i) { // Iterating through all meshes - sGameObjNum contains the number meshes created.
		GameObj* pObject = sGameObjList + i;
		AEGfxMeshFree(pObject->pMesh); // Freeing the mesh.
		pObject->pMesh = NULL; // Setting it to NULL after freeing.
	}
}

void checkBulletAsteroidCollisions() {
	// Lock the asteroids collection for thread safety
	std::lock_guard<std::mutex> lockAsteroids(asteroidsMutex);
	Client::lockBullets();

	// For each asteroid, check collision with each bullet
	for (auto& asteroidPair : asteroids) {
		if (!asteroidPair.second.isActive) continue;

		// Create asteroid AABB
		AABB asteroidAABB;
		float halfSizeX = asteroidPair.second.scaleX / 2.0f;
		float halfSizeY = asteroidPair.second.scaleY / 2.0f;
		asteroidAABB.min.x = asteroidPair.second.currentX - halfSizeX;
		asteroidAABB.min.y = asteroidPair.second.currentY - halfSizeY;
		asteroidAABB.max.x = asteroidPair.second.currentX + halfSizeX;
		asteroidAABB.max.y = asteroidPair.second.currentY + halfSizeY;

		// Create asteroid velocity vector
		AEVec2 asteroidVel;
		asteroidVel.x = asteroidPair.second.velocityX * 120.0f; // Match server velocity scale
		asteroidVel.y = asteroidPair.second.velocityY * 120.0f;

		// Check against all bullets
		for (auto bulletIt = bullets.begin(); bulletIt != bullets.end();) {
			const auto& bulletPair = *bulletIt;

			// Skip inactive bullets
			if (!bulletPair.second.fromLocalPlayer) {
				++bulletIt;
				continue;
			}

			// Create bullet AABB
			AABB bulletAABB;
			float bulletHalfSizeX = BULLET_SCALE_X / 2.0f;
			float bulletHalfSizeY = BULLET_SCALE_Y / 2.0f;
			bulletAABB.min.x = bulletPair.second.X - bulletHalfSizeX;
			bulletAABB.min.y = bulletPair.second.Y - bulletHalfSizeY;
			bulletAABB.max.x = bulletPair.second.X + bulletHalfSizeX;
			bulletAABB.max.y = bulletPair.second.Y + bulletHalfSizeY;

			// Create bullet velocity vector
			AEVec2 bulletVel;
			bulletVel.x = bulletPair.second.velocityX;
			bulletVel.y = bulletPair.second.velocityY;

			// Check for collision
			float tFirst = 0.0f;
			if (CollisionIntersection_RectRect(
				bulletAABB, bulletVel,
				asteroidAABB, asteroidVel,
				tFirst)) {

				// Collision detected!
				//std::cout << "Collision detected between bullet " << bulletPair.first
				//	<< " and asteroid " << asteroidPair.first << std::endl;

				// Report asteroid destruction to server
				globalClient.sendAsteroidDestructionEvent(asteroidPair.first);

				// Update the player's score
				int playerID = Client::getPlayerID();
				if (players.find(playerID) != players.end()) {
					players[playerID].score += 100; // Increment the player's score by 100

					globalClient.sendScoreUpdateEvent(players[playerID].clientIP, players[playerID].score);
				}

				// Remove the bullet from the local list
				bulletIt = bullets.erase(bulletIt);

				// Mark the asteroid as inactive locally (server will confirm)
				asteroidPair.second.isActive = false;

				// Break out of bullet loop as this asteroid is now destroyed
				goto nextAsteroid;  // Using goto to break out of nested loop
			}
			else {
				++bulletIt;
			}
		}

		// Also check against local game object bullets
		for (auto it = pBullets.begin(); it != pBullets.end();) {
			GameObjInst* bullet = *it;

			if (!bullet || !(bullet->flag & FLAG_ACTIVE)) {
				it = pBullets.erase(it);
				continue;
			}

			// Create bullet AABB
			AABB bulletAABB;
			bulletAABB.min.x = bullet->posCurr.x - bullet->scale.x / 2.0f;
			bulletAABB.min.y = bullet->posCurr.y - bullet->scale.y / 2.0f;
			bulletAABB.max.x = bullet->posCurr.x + bullet->scale.x / 2.0f;
			bulletAABB.max.y = bullet->posCurr.y + bullet->scale.y / 2.0f;

			// Check for collision
			float tFirst = 0.0f;
			if (CollisionIntersection_RectRect(
				bulletAABB, bullet->velCurr,
				asteroidAABB, asteroidVel,
				tFirst)) {

				// Collision detected!
				//std::cout << "Collision detected between local bullet and asteroid "
				//	<< asteroidPair.first << std::endl;

				// Report asteroid destruction to server
				globalClient.sendAsteroidDestructionEvent(asteroidPair.first);
				// Update the player's score
				int playerID = Client::getPlayerID();
				//Get the port number of the client 
				SOCKET clientSocket = globalClient.getSocket();
				sockaddr_in addr;
				int addrLen = sizeof(addr);
				uint16_t port_out = 0;
				if (getsockname(clientSocket, (sockaddr*)&addr, &addrLen) == 0) {
					// Convert the port number from network byte order to host byte order
					uint16_t port = ntohs(addr.sin_port);
					port_out = port;
					//std::cout << "Port number: " << port << std::endl;
				}
				else {
					std::cerr << "Failed to get socket name" << std::endl;
				}
				if (players.find(playerID) != players.end()) {
					Playerscore += 100; // Increment the player's score by 100
					globalClient.sendScoreUpdateEvent(players[playerID].clientIP +":"+ std::to_string(port_out), Playerscore);
				}
				// Destroy the bullet game object
				gameObjInstDestroy(bullet);
				it = pBullets.erase(it);

				// Mark the asteroid as inactive locally (server will confirm)
				asteroidPair.second.isActive = false;

				// Break out of bullet loop as this asteroid is now destroyed
				goto nextAsteroid;
			}
			else {
				++it;
			}
		}

	nextAsteroid:
		continue;
	}

	Client::unlockBullets();
}

AEVec2 returnPlayerPosition()
{
	return finalPlayerPosition;
}

float returnPlayerRotation() {
	return playerRotate;
}

int returnPlayerScore() {
	return Playerscore;
}

AEVec2 returnBulletPosition() {
	return finalBulletPosition;
}

float returnBulletRotation() {
	return bulletRotate;
}

/******************************************************************************/
/*!
	Function to aid in creating an instance for a object. Essentially initializing the new instance with the appropriate values.
*/
/******************************************************************************/
GameObjInst* gameObjInstCreate(unsigned long type,
	AEVec2* scale,
	AEVec2* pPos,
	AEVec2* pVel,
	float dir)
{
	AEVec2 zero;
	AEVec2Zero(&zero); // Zero'd out vector to assign data members assigned to nullptr.

	AE_ASSERT_PARM(type < sGameObjNum); // Error message if the type of instance to be created is not of the correct type.

	// loop through the object instance list to find a non-used object instance
	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjInst* pInst = sGameObjInstList + i;

		// check if current instance is not used
		if (pInst->flag == 0)
		{
			// it is not used => use it to create the new instance
			pInst->pObject = sGameObjList + type;
			pInst->flag = FLAG_ACTIVE;
			pInst->scale = *scale;
			pInst->posCurr = pPos ? *pPos : zero;
			pInst->velCurr = pVel ? *pVel : zero;
			pInst->dirCurr = dir;
			// return the newly created instance
			return pInst;
		}
	}
	// cannot find empty slot => return 0
	return 0;
}

/******************************************************************************/
/*!
	Function to destroy the instance of the object, by simply setting its flag to be 0.
*/
/******************************************************************************/
void gameObjInstDestroy(GameObjInst* pInst)
{
	// if instance is destroyed before, just return
	if (pInst->flag == 0)
		return;

	// zero out the flag
	pInst->flag = 0;
}



/******************************************************************************/
/*!
	check for collision between Ship and Wall and apply physics response on the Ship
		-- Apply collision response only on the "Ship" as we consider the "Wall" object is always stationary
		-- We'll check collision only when the ship is moving towards the wall!
	[DO NOT UPDATE THIS PARAGRAPH'S CODE]
*/
/******************************************************************************/
void Helper_Wall_Collision()
{
	//calculate the vectors between the previous position of the ship and the boundary of wall
	AEVec2 vec1{};
	vec1.x = spShip->posPrev.x - spWall->boundingBox.min.x;
	vec1.y = spShip->posPrev.y - spWall->boundingBox.min.y;
	AEVec2 vec2{};
	vec2.x = 0.0f;
	vec2.y = -1.0f;
	AEVec2 vec3{};
	vec3.x = spShip->posPrev.x - spWall->boundingBox.max.x;
	vec3.y = spShip->posPrev.y - spWall->boundingBox.max.y;
	AEVec2 vec4{};
	vec4.x = 1.0f;
	vec4.y = 0.0f;
	AEVec2 vec5{};
	vec5.x = spShip->posPrev.x - spWall->boundingBox.max.x;
	vec5.y = spShip->posPrev.y - spWall->boundingBox.max.y;
	AEVec2 vec6{};
	vec6.x = 0.0f;
	vec6.y = 1.0f;
	AEVec2 vec7{};
	vec7.x = spShip->posPrev.x - spWall->boundingBox.min.x;
	vec7.y = spShip->posPrev.y - spWall->boundingBox.min.y;
	AEVec2 vec8{};
	vec8.x = -1.0f;
	vec8.y = 0.0f;
	if (
		(AEVec2DotProduct(&vec1, &vec2) >= 0.0f) && (AEVec2DotProduct(&spShip->velCurr, &vec2) <= 0.0f) ||
		(AEVec2DotProduct(&vec3, &vec4) >= 0.0f) && (AEVec2DotProduct(&spShip->velCurr, &vec4) <= 0.0f) ||
		(AEVec2DotProduct(&vec5, &vec6) >= 0.0f) && (AEVec2DotProduct(&spShip->velCurr, &vec6) <= 0.0f) ||
		(AEVec2DotProduct(&vec7, &vec8) >= 0.0f) && (AEVec2DotProduct(&spShip->velCurr, &vec8) <= 0.0f)
		)
	{
		float firstTimeOfCollision = 0.0f;
		if (CollisionIntersection_RectRect(spShip->boundingBox,
			spShip->velCurr,
			spWall->boundingBox,
			spWall->velCurr,
			firstTimeOfCollision)) // Documentation for CollisionIntersection_RectRect found in Collision.cpp.
		{
			//re-calculating the new position based on the collision's intersection time
			spShip->posCurr.x = spShip->velCurr.x * (float)firstTimeOfCollision + spShip->posPrev.x;
			spShip->posCurr.y = spShip->velCurr.y * (float)firstTimeOfCollision + spShip->posPrev.y;

			//reset ship velocity
			spShip->velCurr.x = 0.0f;
			spShip->velCurr.y = 0.0f;
		}
	}
}

// Sync players
void syncPlayers(std::unordered_map<int, PlayerData>& pData) {
	// Clean up any player ships that aren't in the current player data
	for (auto it = pShips.begin(); it != pShips.end();) {
		if (pData.find(it->first) == pData.end()) {
			// Player no longer exists in the data, destroy their ship and remove from the map
			if (it->second) {
				gameObjInstDestroy(it->second);
			}
			it = pShips.erase(it);
		}
		else {
			++it;
		}
	}

	for (const auto& pair : pData) {
		// Skip our own player ID - we'll handle our own ship separately
		if (pair.second.playerID == Client::getPlayerID()) {
			// If we somehow have our own player ID in pShips, remove it
			auto it = pShips.find(pair.first);
			if (it != pShips.end()) {
				if (it->second) {
					gameObjInstDestroy(it->second);
				}
				pShips.erase(it);
			}
			continue;
		}

		// If player does not have a ship, create one
		if (pShips.find(pair.first) == pShips.end()) {
			AEVec2 scale;
			AEVec2Set(&scale, SHIP_SCALE_X, SHIP_SCALE_Y);
			AEVec2 position;
			AEVec2Set(&position, pData[pair.first].X, pData[pair.first].Y);
			pShips[pair.first] = gameObjInstCreate(TYPE_SHIP, &scale, &position, nullptr, 0.0f);
		}
		else {
			// Update existing ship position
			AEVec2 position;
			AEVec2Set(&position, pData[pair.first].X, pData[pair.first].Y);
			pShips[pair.first]->posCurr = position;
			pShips[pair.first]->dirCurr = pair.second.rotation;
		}
	}
}

void RenderPlayerNames(std::unordered_map<int, PlayerData>& pData) {
	for (const auto& pair : pData) {
		const PlayerData& player = pair.second; // Get player data

		AEVec2 position;
		f32 normalizedX, normalizedY;
		AEVec2Set(&position, player.X, player.Y);

		// Wrap the position to the window bounds
		f32 wrappedX = AEWrap(position.x, AEGfxGetWinMinX() - SHIP_SCALE_X, AEGfxGetWinMaxX() + SHIP_SCALE_X);
		f32 wrappedY = AEWrap(position.y, AEGfxGetWinMinY() - SHIP_SCALE_Y, AEGfxGetWinMaxY() + SHIP_SCALE_Y);

		// Normalize the wrapped position from window bounds to [-1, 1]
		normalizedX = (wrappedX - AEGfxGetWinMinX()) / (AEGfxGetWinMaxX() - AEGfxGetWinMinX()) * 2.0f - 1.0f;
		normalizedY = (wrappedY - AEGfxGetWinMinY()) / (AEGfxGetWinMaxY() - AEGfxGetWinMinY()) * 2.0f - 1.0f;

		normalizedX -= 0.2f;
		normalizedY += 0.1f;

		// Convert player ID to a string
		char nameBuffer[100];
		snprintf(nameBuffer, sizeof(nameBuffer), "Player %d", pair.first);

		// Debug: Output the player position (normalized)
		//printf("Rendering Player %d at normalized position: (%.2f, %.2f)\n", pair.first, normalizedX, normalizedY);

		// Set rendering settings
		AEGfxSetRenderMode(AE_GFX_RM_COLOR);
		AEGfxSetBlendMode(AE_GFX_BM_BLEND);
		AEGfxTextureSet(NULL, 0, 0);
		AEGfxSetTransparency(1.0f);

		// Draw the name at the normalized position
		AEGfxPrint(fontId, nameBuffer, normalizedX, normalizedY, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
	}
}

void renderServerAsteroids() {
	// First update all asteroid interpolation
	updateAsteroidInterpolation();

	std::lock_guard<std::mutex> lock(asteroidsMutex);

	// Debug information - uncomment if needed
	// std::cout << "Rendering " << asteroids.size() << " asteroids" << std::endl;

	// Iterate through all asteroids
	for (const auto& pair : asteroids) {
		const std::string& id = pair.first;
		const auto& asteroid = pair.second;

		// Debug print for asteroid positions - uncomment if needed
		// std::cout << "Asteroid " << id << " at (" << asteroid.currentX << ", " << asteroid.currentY << ")" << std::endl;

		if (!asteroid.isActive) continue;

		// Create a temporary asteroid instance for rendering
		AEVec2 scale;
		AEVec2 pos;
		AEVec2 vel;

		// Use the interpolated position for rendering
		AEVec2Set(&scale, asteroid.scaleX, asteroid.scaleY);
		AEVec2Set(&pos, asteroid.currentX, asteroid.currentY);
		AEVec2Set(&vel, asteroid.velocityX, asteroid.velocityY);

		// Create the asteroid object instance
		GameObjInst* pInst = gameObjInstCreate(TYPE_ASTEROID, &scale, &pos, &vel, 0.0f);

		// Skip if creation failed
		if (!pInst) continue;

		// Set the position and velocity explicitly
		pInst->posCurr.x = pos.x;
		pInst->posCurr.y = pos.y;
		pInst->velCurr.x = vel.x;
		pInst->velCurr.y = vel.y;

		// Calculate the transform matrix for rendering
		AEMtx33 trans, rot, scaleMtx;
		AEMtx33Scale(&scaleMtx, pInst->scale.x, pInst->scale.y);
		AEMtx33Rot(&rot, pInst->dirCurr);
		AEMtx33Trans(&trans, pInst->posCurr.x, pInst->posCurr.y);
		AEMtx33Concat(&pInst->transform, &rot, &scaleMtx);
		AEMtx33Concat(&pInst->transform, &trans, &pInst->transform);

		// Render the asteroid
		AEGfxSetTransform(pInst->transform.m);
		AEGfxMeshDraw(pInst->pObject->pMesh, AE_GFX_MDM_TRIANGLES);

		// Destroy the instance after rendering
		gameObjInstDestroy(pInst);
	}
}

void DisplayScores(const std::unordered_map<int, PlayerData>& gamePlayers, int playerID) {
	for (const auto& pair : gamePlayers) {
		const PlayerData& player = pair.second; // Get player data

		// Positioning
		AEVec2 position;
		f32 normalizedX, normalizedY;
		AEVec2Set(&position, player.X, player.Y);

		// Wrap the position to the window bounds
		f32 wrappedX = AEWrap(position.x, AEGfxGetWinMinX() - SHIP_SCALE_X, AEGfxGetWinMaxX() + SHIP_SCALE_X);
		f32 wrappedY = AEWrap(position.y, AEGfxGetWinMinY() - SHIP_SCALE_Y, AEGfxGetWinMaxY() + SHIP_SCALE_Y);

		// Normalize the wrapped position from window bounds to [-1, 1]
		normalizedX = (wrappedX - AEGfxGetWinMinX()) / (AEGfxGetWinMaxX() - AEGfxGetWinMinX()) * 2.0f - 1.0f;
		normalizedY = (wrappedY - AEGfxGetWinMinY()) / (AEGfxGetWinMaxY() - AEGfxGetWinMinY()) * 2.0f - 1.0f;

		normalizedX -= 0.2f;
		normalizedY += 0.1f;

		// Format the player's score text
		char scoreText[256];
		int id = pair.first;
		int score = pair.second.score;
		snprintf(scoreText, sizeof(scoreText), "Player %d: %d points", id, score);

		// Set rendering settings (just like the reference)
		AEGfxSetRenderMode(AE_GFX_RM_COLOR);
		AEGfxSetBlendMode(AE_GFX_BM_BLEND);
		AEGfxTextureSet(NULL, 0, 0);
		AEGfxSetTransparency(1.0f);

		// Define the color for the text (green for the player, white for others)
		u32 color = (id == playerID) ? 0x00FF00 : 0xFFFFFF;

		// Draw the score at the normalized position (same as name rendering)
		AEGfxPrint(fontId, scoreText, normalizedX, normalizedY, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
	}
}

void sendGameScoreToServer(int playerID, int score)
{

}

void renderNetworkBullets() {
	// Use Client's lock/unlock methods instead of trying to access the mutex directly
	Client::lockBullets();

	// Debugging: Print how many bullets we have
	//std::cout << "Rendering " << bullets.size() << " network bullets" << std::endl;

	// Access the bullets map directly (it should be an extern variable)
	for (const auto& pair : bullets) {
		const BulletData& bullet = pair.second;

		// Skip bullets from the local player (already rendered locally)
		if (bullet.fromLocalPlayer) {
			continue;
		}

		// Create a temporary bullet instance for rendering
		AEVec2 scale;
		AEVec2 pos;
		AEVec2 vel;

		AEVec2Set(&scale, BULLET_SCALE_X, BULLET_SCALE_Y);
		AEVec2Set(&pos, bullet.X, bullet.Y);
		AEVec2Set(&vel, bullet.velocityX, bullet.velocityY);

		// Create the bullet object instance
		GameObjInst* pInst = gameObjInstCreate(TYPE_BULLET, &scale, &pos, &vel, bullet.direction);

		// Skip if creation failed
		if (!pInst) continue;

		// Set the position and direction explicitly
		pInst->posCurr.x = pos.x;
		pInst->posCurr.y = pos.y;
		pInst->dirCurr = bullet.direction;

		// Calculate the transform matrix for rendering
		AEMtx33 trans, rot, scaleMtx;
		AEMtx33Scale(&scaleMtx, pInst->scale.x, pInst->scale.y);
		AEMtx33Rot(&rot, pInst->dirCurr);
		AEMtx33Trans(&trans, pInst->posCurr.x, pInst->posCurr.y);
		AEMtx33Concat(&pInst->transform, &rot, &scaleMtx);
		AEMtx33Concat(&pInst->transform, &trans, &pInst->transform);

		// Render the bullet
		AEGfxSetTransform(pInst->transform.m);
		AEGfxMeshDraw(pInst->pObject->pMesh, AE_GFX_MDM_TRIANGLES);

		// Destroy the instance after rendering
		gameObjInstDestroy(pInst);
	}

	// Don't forget to unlock when done
	Client::unlockBullets();
}