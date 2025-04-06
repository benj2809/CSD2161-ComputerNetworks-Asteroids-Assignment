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

// Game constants
const unsigned int GAME_OBJ_NUM_MAX = 32;
const unsigned int GAME_OBJ_INST_NUM_MAX = 2048;
const unsigned int SHIP_INITIAL_NUM = 3;

// Object scales
const float SHIP_SCALE_X = 16.0f;
const float SHIP_SCALE_Y = 16.0f;
const float BULLET_SCALE_X = 20.0f;
const float BULLET_SCALE_Y = 3.0f;
const float ASTEROID_MAX_SCALE_X = 60.0f;
const float ASTEROID_MAX_SCALE_Y = 60.0f;

// Movement parameters
const float SHIP_ACCEL_FORWARD = 100.0f;
const float SHIP_ACCEL_BACKWARD = 100.0f;
const float SHIP_ROT_SPEED = (2.0f * PI);
const float BULLET_SPEED = 500.0f;
const float BOUNDING_RECT_SIZE = 1.0f;

// Game state
static bool onValueChange{ true };
bool gameOver = false;
bool winnerAnnounced = false;
int playerCount = 1;

enum TYPE {
	TYPE_SHIP = 0,
	TYPE_BULLET,
	TYPE_ASTEROID,
	TYPE_WALL,
	TYPE_NUM
};

// Object structures
struct GameObj {
	unsigned long type;
	AEGfxVertexList* pMesh;
};

struct GameObjInst {
	GameObj* pObject;
	unsigned long flag;
	AEVec2 scale;
	AEVec2 posCurr;
	AEVec2 posPrev;
	AEVec2 velCurr;
	float dirCurr;
	AABB boundingBox;
	AEMtx33 transform;
};

// Static member initialization
static GameObj sGameObjList[GAME_OBJ_NUM_MAX];
static unsigned long sGameObjNum;
static GameObjInst sGameObjInstList[GAME_OBJ_INST_NUM_MAX];
static unsigned long sGameObjInstNum;
static GameObjInst* ship;
static GameObjInst* wall;
static long shipLives;
static unsigned long shipScore;

// Multiplayer objects
std::unordered_map<int, GameObjInst*> shipMap;
std::vector<GameObjInst*> bulletArray;
extern std::unordered_map<std::string, AsteroidData> asteroids;
extern std::unordered_map<std::string, BulletData> bullets;
extern GameClient globalClient;

// Flags
const unsigned long FLAG_ACTIVE = 0x00000001;

/**
 * @brief Creates a new game object instance
 *
 * Allocates and initializes a game object instance of the specified type.
 * Finds the first available slot in the game object instance list and initializes it.
 *
 * @param type The TYPE enum value of the object to create
 * @param scale Pointer to the scale vector for the new instance
 * @param pPos Pointer to the position vector (nullptr for zero position)
 * @param pVel Pointer to the velocity vector (nullptr for zero velocity)
 * @param dir Initial direction in radians
 * @return Pointer to the created instance, or nullptr if no slots available
 */
GameObjInst* gameObjInstCreate(unsigned long type, AEVec2* scale, AEVec2* pPos, AEVec2* pVel, float dir)
{
	AEVec2 zero;
	AEVec2Zero(&zero);
	AE_ASSERT_PARM(type < sGameObjNum);

	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjInst* pInst = sGameObjInstList + i;

		if (pInst->flag == 0)
		{
			pInst->pObject = sGameObjList + type;
			pInst->flag = FLAG_ACTIVE;
			pInst->scale = *scale;
			pInst->posCurr = pPos ? *pPos : zero;
			pInst->velCurr = pVel ? *pVel : zero;
			pInst->dirCurr = dir;
			return pInst;
		}
	}
	return nullptr;
}

/**
 * @brief Destroys a game object instance
 *
 * Marks the instance as inactive by clearing its flag. The instance
 * will be available for reuse by gameObjInstCreate().
 *
 * @param pInst Pointer to the instance to destroy
 */
void gameObjInstDestroy(GameObjInst* pInst)
{
	if (!pInst || pInst->flag == 0) {
		return;
	}

	pInst->flag = 0;
}



/******************************************************************************/
/*!
	@brief Check for collision between the Ship and Wall, and apply physics response on the Ship.

	This function checks for a potential collision between the Ship and the Wall and applies
	a physics response if a collision is detected. The function applies the collision response
	only to the "Ship" object, as the "Wall" is considered stationary. Collision is checked
	only when the Ship is moving towards the Wall.

	Collision detection is performed using the dot product to determine if the Ship's velocity
	is directed toward the Wall. If a collision is detected, the Ship's position is updated based
	on the intersection time calculated by the `CollisionIntersection_RectRect` function, and
	the Ship's velocity is reset.

	@note The code inside this function is not to be modified as per the instructions.
*/
/******************************************************************************/
void Helper_Wall_Collision()
{
	//calculate the vectors between the previous position of the ship and the boundary of wall
	AEVec2 vec1{};
	vec1.x = ship->posPrev.x - wall->boundingBox.min.x;
	vec1.y = ship->posPrev.y - wall->boundingBox.min.y;
	AEVec2 vec2{};
	vec2.x = 0.0f;
	vec2.y = -1.0f;
	AEVec2 vec3{};
	vec3.x = ship->posPrev.x - wall->boundingBox.max.x;
	vec3.y = ship->posPrev.y - wall->boundingBox.max.y;
	AEVec2 vec4{};
	vec4.x = 1.0f;
	vec4.y = 0.0f;
	AEVec2 vec5{};
	vec5.x = ship->posPrev.x - wall->boundingBox.max.x;
	vec5.y = ship->posPrev.y - wall->boundingBox.max.y;
	AEVec2 vec6{};
	vec6.x = 0.0f;
	vec6.y = 1.0f;
	AEVec2 vec7{};
	vec7.x = ship->posPrev.x - wall->boundingBox.min.x;
	vec7.y = ship->posPrev.y - wall->boundingBox.min.y;
	AEVec2 vec8{};
	vec8.x = -1.0f;
	vec8.y = 0.0f;
	if (
		(AEVec2DotProduct(&vec1, &vec2) >= 0.0f) && (AEVec2DotProduct(&ship->velCurr, &vec2) <= 0.0f) ||
		(AEVec2DotProduct(&vec3, &vec4) >= 0.0f) && (AEVec2DotProduct(&ship->velCurr, &vec4) <= 0.0f) ||
		(AEVec2DotProduct(&vec5, &vec6) >= 0.0f) && (AEVec2DotProduct(&ship->velCurr, &vec6) <= 0.0f) ||
		(AEVec2DotProduct(&vec7, &vec8) >= 0.0f) && (AEVec2DotProduct(&ship->velCurr, &vec8) <= 0.0f)
		)
	{
		float firstTimeOfCollision = 0.0f;
		if (CollisionIntersection_RectRect(ship->boundingBox, ship->velCurr, wall->boundingBox, wall->velCurr, firstTimeOfCollision)) // Documentation for CollisionIntersection_RectRect found in Collision.cpp.
		{
			//re-calculating the new position based on the collision's intersection time
			ship->posCurr.x = ship->velCurr.x * (float)firstTimeOfCollision + ship->posPrev.x;
			ship->posCurr.y = ship->velCurr.y * (float)firstTimeOfCollision + ship->posPrev.y;

			//reset ship velocity
			ship->velCurr.x = 0.0f;
			ship->velCurr.y = 0.0f;
		}
	}
}

/******************************************************************************/
/*!
	@brief Load function for the Asteroids game state.

	This function is responsible for loading the game state, initializing game
	objects such as the ship, bullets, asteroids, and walls. It also creates
	the corresponding meshes for each of these objects.
*/
/******************************************************************************/
void GameStateAsteroidsLoad(void)
{
	GameObj* pObj; /**< Pointer to the current game object being created. */

	// Zero out the game object array and instance array.
	memset(sGameObjList, 0, sizeof(GameObj) * GAME_OBJ_NUM_MAX);
	sGameObjNum = 0;
	memset(sGameObjInstList, 0, sizeof(GameObjInst) * GAME_OBJ_INST_NUM_MAX);
	sGameObjInstNum = 0;

	// The ship object instance hasn't been created yet.
	ship = nullptr;
	wall = nullptr;

	// Create the ship shape mesh.
	pObj = sGameObjList + sGameObjNum++;
	pObj->type = TYPE_SHIP;
	AEGfxMeshStart();
	AEGfxTriAdd(-1.0f, 1.0f, 0xFFFF0000, 0.0f, 0.0f,
		-1.0f, -1.0f, 0xFFFF0000, 0.0f, 0.0f,
		1.5f, 0.0f, 0xFFFF0000, 0.0f, 0.0f);
	pObj->pMesh = AEGfxMeshEnd();
	AE_ASSERT_MESG(pObj->pMesh, "Failed to create ship object mesh!");

	// Create the bullet shape mesh.
	pObj = sGameObjList + sGameObjNum++;
	pObj->type = TYPE_BULLET;
	AEGfxMeshStart();
	AEGfxTriAdd(-0.5f, -0.5f, 0xFFFFFF00, 0.0f, 0.0f,
		0.5f, 0.5f, 0xFFFFFF00, 0.0f, 0.0f,
		-0.5f, 0.5f, 0xFFFFFF00, 0.0f, 0.0f);
	AEGfxTriAdd(-0.5f, -0.5f, 0xFFFFFF00, 0.0f, 0.0f,
		0.5f, -0.5f, 0xFFFFFF00, 0.0f, 0.0f,
		0.5f, 0.5f, 0xFFFFFF00, 0.0f, 0.0f);
	pObj->pMesh = AEGfxMeshEnd();
	AE_ASSERT_MESG(pObj->pMesh, "Failed to create bullet object mesh!");

	// Create the asteroid shape mesh.
	pObj = sGameObjList + sGameObjNum++;
	pObj->type = TYPE_ASTEROID;
	AEGfxMeshStart();
	AEGfxTriAdd(-0.5f, -0.5f, 0xFF808080, 0.0f, 0.0f,
		0.5f, 0.5f, 0xFF808080, 0.0f, 0.0f,
		-0.5f, 0.5f, 0xFF808080, 0.0f, 0.0f);
	AEGfxTriAdd(-0.5f, -0.5f, 0xFF808080, 0.0f, 0.0f,
		0.5f, -0.5f, 0xFF808080, 0.0f, 0.0f,
		0.5f, 0.5f, 0xFF808080, 0.0f, 0.0f);
	pObj->pMesh = AEGfxMeshEnd();
	AE_ASSERT_MESG(pObj->pMesh, "Failed to create asteroid object mesh!");

	// Create the wall shape mesh.
	pObj = sGameObjList + sGameObjNum++;
	pObj->type = TYPE_WALL;
	AEGfxMeshStart();
	AEGfxTriAdd(-0.5f, -0.5f, 0x6600FF00, 0.0f, 0.0f,
		0.5f, 0.5f, 0x6600FF00, 0.0f, 0.0f,
		-0.5f, 0.5f, 0x6600FF00, 0.0f, 0.0f);
	AEGfxTriAdd(-0.5f, -0.5f, 0x6600FF00, 0.0f, 0.0f,
		0.5f, -0.5f, 0x6600FF00, 0.0f, 0.0f,
		0.5f, 0.5f, 0x6600FF00, 0.0f, 0.0f);
	pObj->pMesh = AEGfxMeshEnd();
	AE_ASSERT_MESG(pObj->pMesh, "Failed to create wall object mesh!");
}

/******************************************************************************/
/*!
	@brief Initialize function for the Asteroids game state.

	This function initializes the game state by clearing previous game objects
	(such as ships and bullets), creating the main ship object, and setting
	initial game values such as score and lives.
*/
/******************************************************************************/
void GameStateAsteroidsInit(void)
{
	// Clear existing ships and bullets.
	for (auto it = shipMap.begin(); it != shipMap.end(); ++it) {
		if (it->second) {
			gameObjInstDestroy(it->second);
		}
	}
	shipMap.clear();

	for (auto it = bulletArray.begin(); it != bulletArray.end(); ++it) {
		if (*it) {
			gameObjInstDestroy(*it);
		}
	}
	bulletArray.clear();

	// Create the main ship instance.
	AEVec2 scale;
	AEVec2Set(&scale, SHIP_SCALE_X, SHIP_SCALE_Y);
	ship = gameObjInstCreate(TYPE_SHIP, &scale, nullptr, nullptr, 0.0f);
	AE_ASSERT(ship);

	// Reset score and lives.
	shipScore = 0;
	shipLives = SHIP_INITIAL_NUM;
}

/******************************************************************************/ 
/*!
	\brief "Update" function of this state

	Handles game state updates including score changes, player movement, 
	collisions, bullet creation, and game over conditions. It updates the 
	player's score, checks for collisions, moves the player's ship, and 
	handles shooting mechanics. Additionally, it checks for game over conditions
	and announces the winner when applicable.
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
		GameClient::displayPlayerScores();
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

	// Only process input if the player hasn't won or lost yet
	if (shipScore < 5000 && shipLives >= 0) {

		// --- Movement: Forward ---
		if (AEInputCheckCurr(AEVK_UP)) {
			AEVec2 accelerationDir, newVelocity{};
			AEVec2Set(&accelerationDir, cosf(ship->dirCurr), sinf(ship->dirCurr));
			AEVec2Normalize(&accelerationDir, &accelerationDir);
			AEVec2Scale(&accelerationDir, &accelerationDir, SHIP_ACCEL_FORWARD * (f32)AEFrameRateControllerGetFrameTime());
			AEVec2Add(&newVelocity, &accelerationDir, &ship->velCurr);
			AEVec2Scale(&newVelocity, &newVelocity, 0.99f); // Apply damping
			ship->velCurr = newVelocity;
		}

		// --- Movement: Backward ---
		if (AEInputCheckCurr(AEVK_DOWN)) {
			AEVec2 reverseDir, newVelocity{};
			AEVec2Set(&reverseDir, cosf(ship->dirCurr), sinf(ship->dirCurr));
			AEVec2Normalize(&reverseDir, &reverseDir);
			AEVec2Scale(&reverseDir, &reverseDir, -SHIP_ACCEL_BACKWARD * (f32)AEFrameRateControllerGetFrameTime());
			AEVec2Add(&newVelocity, &reverseDir, &ship->velCurr);
			AEVec2Scale(&newVelocity, &newVelocity, 0.99f);
			ship->velCurr = newVelocity;
		}

		// --- Rotation: Left ---
		if (AEInputCheckCurr(AEVK_LEFT)) {
			ship->dirCurr += SHIP_ROT_SPEED * (f32)AEFrameRateControllerGetFrameTime();
			ship->dirCurr = AEWrap(ship->dirCurr, -PI, PI);
		}

		// --- Rotation: Right ---
		if (AEInputCheckCurr(AEVK_RIGHT)) {
			ship->dirCurr -= SHIP_ROT_SPEED * (f32)AEFrameRateControllerGetFrameTime();
			ship->dirCurr = AEWrap(ship->dirCurr, -PI, PI);
		}

		// --- Shooting ---
		if (AEInputCheckTriggered(AEVK_SPACE)) {
			AEVec2 bulletDir;
			AEVec2Set(&bulletDir, cosf(ship->dirCurr), sinf(ship->dirCurr));

			AEVec2 bulletVelocity;
			AEVec2Scale(&bulletVelocity, &bulletDir, BULLET_SPEED);

			AEVec2 bulletSpawnPos, shipNose;
			AEVec2Scale(&shipNose, &bulletDir, SHIP_SCALE_X * 0.5f);
			AEVec2Add(&bulletSpawnPos, &ship->posCurr, &shipNose);

			AEVec2 bulletScale;
			AEVec2Set(&bulletScale, BULLET_SCALE_X, BULLET_SCALE_Y);

			GameObjInst* newBullet = gameObjInstCreate(TYPE_BULLET, &bulletScale, &bulletSpawnPos, &bulletVelocity, ship->dirCurr);

			if (newBullet) {
				std::string uniqueBulletID = std::to_string(GameClient::getPlayerID()) + "_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());

				GameClient::ScopedBulletLock lock;
				BulletData bulletInfo;
				bulletInfo.bulletID = uniqueBulletID;
				bulletInfo.X = bulletSpawnPos.x;
				bulletInfo.Y = bulletSpawnPos.y;
				bulletInfo.velocityX = bulletVelocity.x;
				bulletInfo.velocityY = bulletVelocity.y;
				bulletInfo.direction = ship->dirCurr;
				bulletInfo.fromLocalPlayer = true;
				bullets[uniqueBulletID] = bulletInfo;
				bulletArray.push_back(newBullet);

				globalClient.sendBulletCreationEvent(bulletSpawnPos, bulletVelocity, ship->dirCurr, uniqueBulletID);
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
		if ((pInst->flag & FLAG_ACTIVE) == 0) {
			continue;
		}

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

		pInst->boundingBox.min.x = -(BOUNDING_RECT_SIZE / 2.0f) * pInst->scale.x + pInst->posPrev.x;
		pInst->boundingBox.min.y = -(BOUNDING_RECT_SIZE / 2.0f) * pInst->scale.y + pInst->posPrev.y;
		pInst->boundingBox.max.x = (BOUNDING_RECT_SIZE / 2.0f) * pInst->scale.x + pInst->posPrev.x;
		pInst->boundingBox.max.y = (BOUNDING_RECT_SIZE / 2.0f) * pInst->scale.y + pInst->posPrev.y;

		pInst->posCurr.x = pInst->velCurr.x * globalDeltaTime + pInst->posCurr.x;
		pInst->posCurr.y = pInst->velCurr.y * globalDeltaTime + pInst->posCurr.y;
	}

	// ======================================================================
	// check for dynamic-static collisions (one case only: Ship vs Wall)
	// [DO NOT UPDATE THIS PARAGRAPH'S CODE]
	// ======================================================================

	//======================================================================
	//check for dynamic-dynamic collisions [AA-BB] 
	//======================================================================
	if (!(shipScore >= 5000)) { // Just like movement, collision only takes place when the game is active.
		if (!(shipLives < 0)) {
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
							if (CollisionIntersection_RectRect(pInst->boundingBox, pInst->velCurr, ship->boundingBox, ship->velCurr,
								tFirst)) { // This if condition checks between the min & max for the SHIP x/y coordinates against the ASTEROID																																																										// It is important to check for both ASTEROID - SHIP and SHIP - ASTEROID collisions, this confirms there is a collision b/w both objects																																																						// If either one of the checks is to be omitted, the will be a case where no collision is detected when either object is WITHIN the other.
								gameObjInstDestroy(pInst); // Destroy the ASTEROID if there is collision																																									
								gameObjInstDestroy(ship); // Likewise destroy the ship
								AEVec2 scale{ SHIP_SCALE_X, SHIP_SCALE_Y }; // The following vectors are the initial vectors for the ship.
								ship = gameObjInstCreate(TYPE_SHIP, &scale, nullptr, nullptr, 0.0f);
								AEVec2 scale2{ (f32)(rand() % (60 - 10 + 1) + 10) , (f32)(rand() % (60 - 10 + 1) + 10) }, pos{ (f32)(rand() % (900 - (-500) + 1) + (-500)), 400.f }, vel{ (f32)(rand() % (100 - (-100) + 1) + (-100)),  (f32)(rand() % (100 - (-100) + 1) + (-100)) };
								gameObjInstCreate(TYPE_ASTEROID, &scale2, &pos, &vel, 0.0f); // Creating a new ship at the the center of the screen.
								--shipLives; // Decrement ship lives
								onValueChange = true; // Setting this to true to print to the user their current score and amount of ship lives left.
							}
						}
						else if (NewpInst->pObject->type == TYPE_BULLET) { // If the current instance is a bullet, eheck for collision.
							if (CollisionIntersection_RectRect(pInst->boundingBox, pInst->velCurr,
								NewpInst->boundingBox, NewpInst->velCurr,
								tFirst)) { // ASTEROID - BULLET and BULLET - ASTEROID collision checks are account for to prevent false collisions.

								// Removing pointer of destroyed bullet from bulletArray container.
								bulletArray.erase(std::remove_if(bulletArray.begin(), bulletArray.end(),
									[pInst](GameObjInst* bullet) {
										return bullet == pInst;
									}), bulletArray.end());

								gameObjInstDestroy(pInst); // Destroy the ASTEROID if there is collision.
								gameObjInstDestroy(NewpInst); // Likewise destroy the bullet.
								AEVec2 scale{ (f32)(rand() % (60 - 10 + 1) + 10), (f32)(rand() % (60 - 10 + 1) + 10) }, pos{ (f32)(rand() % (900 - (-500) + 1) + (-500)), 400.f }, vel{ (f32)(rand() % (100 - (-100) + 1) + (-100)), (f32)(rand() % (100 - (-100) + 1) + (-100)) }; // Setting random values for the new ASTEROID to be created.
								gameObjInstCreate(TYPE_ASTEROID, &scale, &pos, &vel, 0.0f); // Creating a new ASTEROID instance with random values.
								AEVec2Scale(&pos, &pos, -1.f); // Modifying position for the 2nd ASTEROID
								AEVec2Scale(&vel, &vel, -1.3f); // Modifying velocity for the 2nd ASTEROID
								gameObjInstCreate(TYPE_ASTEROID, &scale, &pos, &vel, 0.0f); // Creating a new ASTEROID with modified values. 
								shipScore += 100; // Incrementing the score.
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

		// Skip non-active game object
		if ((pInst->flag & FLAG_ACTIVE) == 0) {
			continue;
		}

		// Handle ship objects: Wrap the ship around the screen when it goes off the edges
		if (pInst->pObject->type == TYPE_SHIP) {
			// Wrap ship coordinates to opposite end
			pInst->posCurr.x = AEWrap(pInst->posCurr.x, AEGfxGetWinMinX() - SHIP_SCALE_X, AEGfxGetWinMaxX() + SHIP_SCALE_X);
			pInst->posCurr.y = AEWrap(pInst->posCurr.y, AEGfxGetWinMinY() - SHIP_SCALE_Y, AEGfxGetWinMaxY() + SHIP_SCALE_Y);
		}

		// Handle asteroid objects: Wrap the asteroids around the screen when they go off the edges
		if (pInst->pObject->type == TYPE_ASTEROID) {
			// Wrap asteroid coordinates to opposite end
			pInst->posCurr.x = AEWrap(pInst->posCurr.x, AEGfxGetWinMinX() - ASTEROID_MAX_SCALE_X, AEGfxGetWinMaxX() + ASTEROID_MAX_SCALE_X);
			pInst->posCurr.y = AEWrap(pInst->posCurr.y, AEGfxGetWinMinY() - ASTEROID_MAX_SCALE_Y, AEGfxGetWinMaxY() + ASTEROID_MAX_SCALE_Y);
		}

		// Handle bullet objects: Remove bullets that go out of bounds to free up resources
		if (pInst->pObject->type == TYPE_BULLET) {
			// Check if the bullet has moved out of the screen's horizontal or vertical bounds
			if (pInst->posCurr.x > (AEGfxGetWindowWidth() / 2.f) || pInst->posCurr.x < -(AEGfxGetWindowWidth() / 2.f) || pInst->posCurr.y >(AEGfxGetWindowHeight() / 2.f) || pInst->posCurr.y < -(AEGfxGetWindowHeight() / 2.f)) {
				// Destroy out of bound bullets
				gameObjInstDestroy(pInst);
			}
		}
	}

	// =====================================================================
	// calculate the matrix for all objects
	// =====================================================================

	AEMtx33 translate{}, rotation{}, scale{}; // Vectors for matrices.
	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjInst* pInst = sGameObjInstList + i; // Iterate through each instance.

		// Skip inactive object
		if ((pInst->flag & FLAG_ACTIVE) == 0) {
			continue;
		}

		// Reset the matrices for each object
		AEMtx33Zero(&translate);
		AEMtx33Zero(&rotation);
		AEMtx33Zero(&scale);

		// Scale, Rotate, Translate matrix
		AEMtx33Scale(&scale, pInst->scale.x, pInst->scale.y);
		AEMtx33Rot(&rotation, pInst->dirCurr);
		AEMtx33Trans(&translate, pInst->posCurr.x, pInst->posCurr.y);

		// Concatenate in Scale * Rotate * Translate = Transform order.
		AEMtx33Concat(&pInst->transform, &rotation, &scale);
		AEMtx33Concat(&pInst->transform, &translate, &pInst->transform);
	}

	for (const auto& pair : players) {

		if (pair.second.playerID != GameClient::getPlayerID()) {
			continue;
		}
		finalPlayerPosition = { ship->posCurr.x, ship->posCurr.y };
		playerRotate = ship->dirCurr;
		break;
	}

	// Bullet updates
	bulletArray.erase(std::remove_if(bulletArray.begin(), bulletArray.end(), [](GameObjInst* bullet)
	{
			if (!bullet || !(bullet->flag & FLAG_ACTIVE)) {
				return true;
			}

			bullet->posCurr.x += bullet->velCurr.x * globalDeltaTime;
			bullet->posCurr.y += bullet->velCurr.y * globalDeltaTime;

			bool outOfBounds = bullet->posCurr.x < AEGfxGetWinMinX() - BULLET_SCALE_X || bullet->posCurr.x > AEGfxGetWinMaxX() + BULLET_SCALE_X || bullet->posCurr.y < AEGfxGetWinMinY() - BULLET_SCALE_Y || bullet->posCurr.y > AEGfxGetWinMaxY() + BULLET_SCALE_Y;

			if (outOfBounds)
			{
				gameObjInstDestroy(bullet);
				return true;
			}

			AEMtx33 scaleMat, rotMat, transMat;
			AEMtx33Scale(&scaleMat, bullet->scale.x, bullet->scale.y);
			AEMtx33Rot(&rotMat, bullet->dirCurr);
			AEMtx33Trans(&transMat, bullet->posCurr.x, bullet->posCurr.y);

			AEMtx33Concat(&bullet->transform, &rotMat, &scaleMat);
			AEMtx33Concat(&bullet->transform, &transMat, &bullet->transform);

			return false;
	}), bulletArray.end());

	{
		GameClient::ScopedBulletLock lock;
		for (auto& pair : bullets) {
			pair.second.X += pair.second.velocityX * globalDeltaTime;
			pair.second.Y += pair.second.velocityY * globalDeltaTime;
		}
	}

	updateAsteroidInterpolation();
	checkBulletAsteroidCollisions();
	synchronizeShips(players);

	// Tracking local player ship
	auto it = players.find(GameClient::getPlayerID());
	if (it != players.end())
	{
		finalPlayerPosition = { ship->posCurr.x, ship->posCurr.y };
		playerRotate = ship->dirCurr;
		Playerscore = it->second.score;
	}
}

/******************************************************************************/
/*!
	@brief Draw function for all the instances in the game.

	This function is responsible for rendering the game objects such as the ship, bullets,
	asteroids, and the static wall. It also handles the display of the ship's lives and score
	and displays game-over messages when appropriate.
*/
/******************************************************************************/
void GameStateAsteroidsDraw(void)
{
	// Set up rendering settings
	AEGfxSetRenderMode(AE_GFX_RM_COLOR); /**< Set render mode to color. */
	AEGfxTextureSet(NULL, 0, 0); /**< No texture is set for rendering. */
	AEGfxSetBlendMode(AE_GFX_BM_BLEND); /**< Enable blending for transparency. */
	AEGfxSetTransparency(1.0f); /**< Full opacity (no transparency). */

	// Iterate through all game object instances and render active ones
	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjInst* pInst = sGameObjInstList + i; /**< Access the current game object instance. */

		// Skip inactive objects
		if ((pInst->flag & FLAG_ACTIVE) == 0) {
			continue;
		}

		// Set the transform matrix for the current object
		AEGfxSetTransform(pInst->transform.m);

		// Draw the object using its mesh
		AEGfxMeshDraw(pInst->pObject->pMesh, AE_GFX_MDM_TRIANGLES);
	}

	// Render the asteroids and network bullets
	renderAsteroids();
	renderNetworkBullets();

	// Display ship lives and score values to the user if there are updates
	if (onValueChange)
	{
		onValueChange = false;

		// Display Game Over message when lives are exhausted
		if (shipLives < 0)
		{
			printf("       GAME OVER       \n");
		}
	}

	// Display winner if the game is over
	if (gameOver)
	{
		// Render "GAME OVER" message
		AEGfxSetRenderMode(AE_GFX_RM_COLOR);
		AEGfxSetBlendMode(AE_GFX_BM_BLEND);
		AEGfxTextureSet(NULL, 0, 0);
		AEGfxSetTransparency(1.0f);

		AEGfxPrint(fontId, "GAME OVER", -0.5f, 0.0f, 3.0f, 1.0f, 0.0f, 0.0f, 1.0f);
	}

	// Render player names on the screen
	renderNames(players);

	// Display scores for each player
	for (auto p : players)
	{
		displayScores();
	}
}

/******************************************************************************/
/*!
	@brief Frees all active game object instances.

	This function iterates through the array of game object instances and destroys
	each active instance by calling `gameObjInstDestroy`. An instance is considered
	active if its `flag` has the `FLAG_ACTIVE` bit set. This function ensures that
	all active instances are properly cleaned up and memory is freed.
*/
/******************************************************************************/
void GameStateAsteroidsFree(void)
{
	// Iterate through all the game object instances
	for (int i = 0; i < GAME_OBJ_INST_NUM_MAX; ++i) {
		// Get the current instance
		GameObjInst* pInst = sGameObjInstList + i;

		// Check if the instance is active (based on its flag)
		if (pInst->flag & FLAG_ACTIVE) {
			// Destroy the active instance and free associated memory
			gameObjInstDestroy(pInst);
		}
	}
}

/******************************************************************************/
/*!
	@brief Unloads all meshes created at the start.

	This function iterates through all the game objects and frees the meshes
	that were created during the initialization phase. After freeing each mesh,
	it sets the corresponding mesh pointer to `NULL` to avoid dangling pointers.
*/
/******************************************************************************/
void GameStateAsteroidsUnload(void)
{
	// Iterate through all the game objects and free their meshes
	for (unsigned long i = 0; i < sGameObjNum; ++i) {
		// Get the current game object
		GameObj* pObject = sGameObjList + i;

		// Free the mesh associated with this object
		AEGfxMeshFree(pObject->pMesh);

		// Set the mesh pointer to NULL to prevent further access
		pObject->pMesh = NULL;
	}
}

/**
 * @brief Retrieves the current position of the player.
 *
 * This function returns the final position of the player in the game world. The position
 * is represented as a 2D vector (AEVec2) with x and y coordinates.
 *
 * @return AEVec2 The player's position in the game world.
 */
AEVec2 fetchPlayerPosition()
{
	return finalPlayerPosition;
}

/**
 * @brief Retrieves the current rotation of the player.
 *
 * This function returns the player's rotation, which is a float representing the angle
 * (in radians or degrees depending on the system's conventions) at which the player is facing.
 *
 * @return float The player's current rotation.
 */
float fetchPlayerRotation() {
	return playerRotate;
}

/**
 * @brief Retrieves the current score of the player.
 *
 * This function returns the player's current score. The score is an integer value that may
 * represent points gained through actions such as destroying asteroids or completing objectives.
 *
 * @return int The player's current score.
 */
int fetchPlayerScore() {
	return Playerscore;
}

/**
 * @brief Retrieves the current position of the bullet.
 *
 * This function returns the final position of the bullet in the game world. The position is
 * represented as a 2D vector (AEVec2) with x and y coordinates.
 *
 * @return AEVec2 The bullet's position in the game world.
 */
AEVec2 fetchBulletPosition() {
	return finalBulletPosition;
}

/**
 * @brief Retrieves the current rotation of the bullet.
 *
 * This function returns the bullet's rotation, which is a float representing the angle
 * (in radians or degrees depending on the system's conventions) at which the bullet is moving.
 *
 * @return float The bullet's current rotation.
 */
float fetchBulletRotation() {
	return bulletRotate;
}

/******************************************************************************/
/*!
	@brief Checks for collisions between bullets and asteroids.

	This function iterates through all active asteroids and checks for collisions
	with both remote (networked) and local bullets. If a collision is detected,
	the asteroid is marked as inactive, and appropriate events are sent to the server
	to notify about the destruction of the asteroid and the update of the player's score.
*/
/******************************************************************************/
void checkBulletAsteroidCollisions() {
	std::vector<std::string> destroyedAsteroids;
	std::vector<std::string> destroyedBulletIDs;
	std::vector<GameObjInst*> destroyedLocalBullets;

	int playerID = GameClient::getPlayerID();

	// === PART 1: Lock asteroids and bullets together for collision check ===
	{
		GameClient::ScopedAsteroidLock asteroidsLock;
		GameClient::ScopedBulletLock playerLock;

		// Collision logic
		for (auto& asteroidPair : asteroids)
		{
			if (!asteroidPair.second.isActive)
				continue;

			// Build asteroid AABB
			AABB asteroidAABB;
			float halfSizeX = asteroidPair.second.scaleX * 0.5f;
			float halfSizeY = asteroidPair.second.scaleY * 0.5f;
			asteroidAABB.min.x = asteroidPair.second.currentX - halfSizeX;
			asteroidAABB.min.y = asteroidPair.second.currentY - halfSizeY;
			asteroidAABB.max.x = asteroidPair.second.currentX + halfSizeX;
			asteroidAABB.max.y = asteroidPair.second.currentY + halfSizeY;

			AEVec2 asteroidVel = { asteroidPair.second.velocityX * 120.0f, asteroidPair.second.velocityY * 120.0f };

			bool asteroidDestroyed = false;

			// Check network bullets
			for (auto& bulletPair : bullets)
			{
				if (!bulletPair.second.fromLocalPlayer)
					continue;

				AABB bulletAABB;
				float halfBulletX = BULLET_SCALE_X * 0.5f;
				float halfBulletY = BULLET_SCALE_Y * 0.5f;
				bulletAABB.min.x = bulletPair.second.X - halfBulletX;
				bulletAABB.min.y = bulletPair.second.Y - halfBulletY;
				bulletAABB.max.x = bulletPair.second.X + halfBulletX;
				bulletAABB.max.y = bulletPair.second.Y + halfBulletY;

				AEVec2 bulletVel = { bulletPair.second.velocityX, bulletPair.second.velocityY };

				float tFirst = 0.0f;
				if (CollisionIntersection_RectRect(bulletAABB, bulletVel, asteroidAABB, asteroidVel, tFirst))
				{
					destroyedAsteroids.push_back(asteroidPair.first);
					destroyedBulletIDs.push_back(bulletPair.first);
					asteroidDestroyed = true;
					break; // Done with this asteroid
				}
			}

			// Check local bullets only if asteroid is not destroyed yet
			if (!asteroidDestroyed)
			{
				for (auto* bullet : bulletArray)
				{
					if (!bullet || !(bullet->flag & FLAG_ACTIVE))
						continue;

					AABB bulletAABB;
					bulletAABB.min.x = bullet->posCurr.x - bullet->scale.x * 0.5f;
					bulletAABB.min.y = bullet->posCurr.y - bullet->scale.y * 0.5f;
					bulletAABB.max.x = bullet->posCurr.x + bullet->scale.x * 0.5f;
					bulletAABB.max.y = bullet->posCurr.y + bullet->scale.y * 0.5f;

					float tFirst = 0.0f;
					if (CollisionIntersection_RectRect(bulletAABB, bullet->velCurr, asteroidAABB, asteroidVel, tFirst))
					{
						destroyedAsteroids.push_back(asteroidPair.first);
						destroyedLocalBullets.push_back(bullet);
						asteroidDestroyed = true;
						break;
					}
				}
			}
		}
	}

	// === PART 2: Lock bullets alone to erase them ===
	{
		GameClient::ScopedBulletLock lock;

		// Remove bullets
		for (const auto& bulletID : destroyedBulletIDs) {
			bullets.erase(bulletID);
		}
	}

	// === PART 3: Local bullets (no lock needed because local) ===
	for (auto* bullet : destroyedLocalBullets)
	{
		gameObjInstDestroy(bullet);
		bulletArray.erase(std::remove(bulletArray.begin(), bulletArray.end(), bullet), bulletArray.end());
	}

	// === PART 4: Lock asteroids again needed for destruction and players for score ===
	{
		GameClient::ScopedAsteroidLock asteroidsLock;
		GameClient::ScopedPlayerLock playerLock;

		// Apply destructions after loop
		for (const auto& asteroidID : destroyedAsteroids)
		{
			globalClient.sendAsteroidDestructionEvent(asteroidID);
			asteroids[asteroidID].isActive = false;

			// Give score
			if (players.find(playerID) != players.end())
			{
				players[playerID].score += 100;
				globalClient.sendScoreUpdateEvent(players[playerID].clientIP, players[playerID].score);
			}
		}
	}
}

/**
 * @brief Updates asteroid positions using interpolation for smooth movement
 */
void updateAsteroidInterpolation() {
	constexpr float INTERPOLATION_DURATION = 0.1f; // 100ms window

	GameClient::ScopedAsteroidLock lock;
	auto currentTime = std::chrono::steady_clock::now();

	for (auto& pair : asteroids) {
		auto& asteroid = pair.second;
		if (!asteroid.isActive) continue;

		float deltaTime = std::chrono::duration<float>(currentTime - asteroid.lastUpdateTime).count();

		if (deltaTime < INTERPOLATION_DURATION) {
			float interpolationFactor = deltaTime / INTERPOLATION_DURATION;
			asteroid.currentX += (asteroid.targetX - asteroid.currentX) * interpolationFactor;
			asteroid.currentY += (asteroid.targetY - asteroid.currentY) * interpolationFactor;
		}
		else {
			asteroid.currentX = asteroid.targetX;
			asteroid.currentY = asteroid.targetY;
		}
	}
}

/**
 * @brief Synchronizes the state of player ships based on the provided player data.
 *
 * This function iterates through all the players in the given data, ensures that ships for active players are created or updated, and destroys ships for inactive players.
 *
 * @param pData A map containing player data, keyed by player ID.
 */
void synchronizeShips(std::unordered_map<int, PlayerData>& pData) {
	int myID = GameClient::getPlayerID();

	// Clean up ships for players who left
	for (auto it = shipMap.begin(); it != shipMap.end(); )
	{
		if (pData.find(it->first) == pData.end() || it->first == myID)
		{
			gameObjInstDestroy(it->second);
			it = shipMap.erase(it);
		}
		else
		{
			++it;
		}
	}

	// Create or update ships for other players
	for (const auto& pair : pData)
	{
		int id = pair.first;
		const PlayerData& pdata = pair.second;

		if (id == myID)
			continue; // Skip yourself

		GameObjInst*& shipPointer = shipMap[id];
		if (!shipPointer)
		{
			AEVec2 scale = { SHIP_SCALE_X, SHIP_SCALE_Y };
			AEVec2 pos = { pdata.X, pdata.Y };
			shipPointer = gameObjInstCreate(TYPE_SHIP, &scale, &pos, nullptr, 0.0f);
		}

		if (shipPointer)
		{
			shipPointer->posCurr = { pdata.X, pdata.Y };
			shipPointer->dirCurr = pdata.rotation;
		}
	}
}

/**
 * @brief Renders the names of all players at their respective positions.
 *
 * This function iterates through all players and renders their names at positions based on their current location. The name is adjusted based on window bounds and normalization.
 *
 * @param pData A map containing player data, keyed by player ID.
 */
void renderNames(std::unordered_map<int, PlayerData>& pData) {
	// Iterate through each player in the unordered map (pData)
	for (const auto& pair : pData) {
		const PlayerData& player = pair.second; // Retrieve player data from the map entry

		AEVec2 position;
		f32 normalizedX, normalizedY;

		// Set the player's position based on the player data (X, Y coordinates)
		AEVec2Set(&position, player.X, player.Y);

		// Wrap the player's position within the window bounds to handle screen boundaries
		f32 wrappedX = AEWrap(position.x, AEGfxGetWinMinX() - SHIP_SCALE_X, AEGfxGetWinMaxX() + SHIP_SCALE_X);
		f32 wrappedY = AEWrap(position.y, AEGfxGetWinMinY() - SHIP_SCALE_Y, AEGfxGetWinMaxY() + SHIP_SCALE_Y);

		// Normalize the wrapped position to fit within the normalized screen space [-1, 1]
		normalizedX = (wrappedX - AEGfxGetWinMinX()) / (AEGfxGetWinMaxX() - AEGfxGetWinMinX()) * 2.0f - 1.0f;
		normalizedY = (wrappedY - AEGfxGetWinMinY()) / (AEGfxGetWinMaxY() - AEGfxGetWinMinY()) * 2.0f - 1.0f;

		// Apply slight offsets for visual positioning adjustments
		normalizedX -= 0.2f;
		normalizedY += 0.1f;

		// Convert the player ID to a string format (e.g., "Player 1")
		char nameBuffer[100];
		snprintf(nameBuffer, sizeof(nameBuffer), "Player %d", pair.first);

		// Set rendering properties for drawing the player name
		AEGfxSetRenderMode(AE_GFX_RM_COLOR);  // Set render mode to color
		AEGfxSetBlendMode(AE_GFX_BM_BLEND);   // Enable blending for transparency
		AEGfxTextureSet(NULL, 0, 0);          // Set texture to NULL (no texture)
		AEGfxSetTransparency(1.0f);           // Set full opacity

		// Render the player's name at the calculated normalized position on screen
		AEGfxPrint(fontId, nameBuffer, normalizedX, normalizedY, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
	}
}

/**
 * @brief Renders server-side asteroids by updating their positions and drawing them.
 *
 * This function updates asteroid interpolation and renders them with correct transformations.
 */
void renderAsteroids() {
	// First update all asteroid interpolation
	updateAsteroidInterpolation();

	GameClient::ScopedAsteroidLock lock;

	// Iterate through all asteroids
	for (const auto& pair : asteroids) {
		const AsteroidData& asteroid = pair.second;

		if (!asteroid.isActive) {
			continue;
		}

		AEVec2 scale = { asteroid.scaleX, asteroid.scaleY };
		AEVec2 pos = { asteroid.currentX, asteroid.currentY };
		AEVec2 vel = { asteroid.velocityX, asteroid.velocityY };

		// Create the asteroid object instance
		GameObjInst* pInst = gameObjInstCreate(TYPE_ASTEROID, &scale, &pos, &vel, 0.0f);

		if (pInst) {
			pInst->posCurr = pos;
			pInst->velCurr = vel;

			AEMtx33 scaleMtx;
			AEMtx33 rotMtx;
			AEMtx33 transMtx;

			AEMtx33Scale(&scaleMtx, scale.x, scale.y);
			AEMtx33Rot(&rotMtx, pInst->dirCurr);
			AEMtx33Trans(&transMtx, pos.x, pos.y);

			AEMtx33Concat(&pInst->transform, &rotMtx, &scaleMtx);
			AEMtx33Concat(&pInst->transform, &transMtx, &pInst->transform);

			AEGfxSetTransform(pInst->transform.m);
			AEGfxMeshDraw(pInst->pObject->pMesh, AE_GFX_MDM_TRIANGLES);

			gameObjInstDestroy(pInst);
		}
	}
}

/**
 * @brief Renders bullets coming from the network, excluding those from the local player.
 *
 * This function iterates over network bullets, skipping those from the local player, and renders them based on their position and velocity.
 */
void renderNetworkBullets() {

	// Temporary list to manage instances
	std::vector<GameObjInst*> tempBullets;

	{
		GameClient::ScopedBulletLock lock;
		// Pre-create all temporary bullets
		for (const auto& pair : bullets)
		{
			const BulletData& bullet = pair.second;

			if (bullet.fromLocalPlayer)
				continue; // Skip local bullets

			AEVec2 scale = { BULLET_SCALE_X, BULLET_SCALE_Y };
			AEVec2 pos = { bullet.X, bullet.Y };
			AEVec2 vel = { bullet.velocityX, bullet.velocityY };

			GameObjInst* pInst = gameObjInstCreate(TYPE_BULLET, &scale, &pos, &vel, bullet.direction);
			if (pInst)
			{
				pInst->posCurr = pos;
				pInst->dirCurr = bullet.direction;
				tempBullets.push_back(pInst);
			}
		}
	}

	// Now render all temp bullets
	for (GameObjInst* pInst : tempBullets)
	{
		// Calculate transform
		AEMtx33 scaleMtx, rotMtx, transMtx;
		AEMtx33Scale(&scaleMtx, pInst->scale.x, pInst->scale.y);
		AEMtx33Rot(&rotMtx, pInst->dirCurr);
		AEMtx33Trans(&transMtx, pInst->posCurr.x, pInst->posCurr.y);

		AEMtx33Concat(&pInst->transform, &rotMtx, &scaleMtx);
		AEMtx33Concat(&pInst->transform, &transMtx, &pInst->transform);

		// Draw
		AEGfxSetTransform(pInst->transform.m);
		AEGfxMeshDraw(pInst->pObject->pMesh, AE_GFX_MDM_TRIANGLES);

		// Destroy temp instance
		gameObjInstDestroy(pInst);
	}
}

/**
 * @brief Displays the scores of all players, highlighting the local player’s score.
 *
 * This function iterates through all players, renders their score at the correct position, and highlights the local player's score in green.
 *
 * @param gamePlayers A map containing player data, keyed by player ID.
 * @param playerID The local player's ID.
 */
void displayScores() {
	for (const auto& pair : players) {
		const int id = pair.first;
		const PlayerData& player = pair.second;

		AEVec2 wrappedPos = {
			AEWrap(player.X, AEGfxGetWinMinX() - SHIP_SCALE_X, AEGfxGetWinMaxX() + SHIP_SCALE_X),
			AEWrap(player.Y, AEGfxGetWinMinY() - SHIP_SCALE_Y, AEGfxGetWinMaxY() + SHIP_SCALE_Y)
		};

		float normalizedX = ((wrappedPos.x - AEGfxGetWinMinX()) / (AEGfxGetWinMaxX() - AEGfxGetWinMinX())) * 2.0f - 1.0f - 0.2f;
		float normalizedY = ((wrappedPos.y - AEGfxGetWinMinY()) / (AEGfxGetWinMaxY() - AEGfxGetWinMinY())) * 2.0f - 1.0f + 0.1f;

		char scoreText[64];
		snprintf(scoreText, sizeof(scoreText), "Player %d: %d points", id, player.score);

		AEGfxSetRenderMode(AE_GFX_RM_COLOR);
		AEGfxSetBlendMode(AE_GFX_BM_BLEND);
		AEGfxTextureSet(NULL, 0, 0);
		AEGfxSetTransparency(1.0f);

		AEGfxPrint(fontId, scoreText, normalizedX, normalizedY, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f);
	}
}

// Initialize to Zero matrix
void AEMtx33Zero(AEMtx33* mtx) {
	mtx->m[0][0] = 0.0f; mtx->m[0][1] = 0.0f; mtx->m[0][2] = 0.0f;
	mtx->m[1][0] = 0.0f; mtx->m[1][1] = 0.0f; mtx->m[1][2] = 0.0f;
	mtx->m[2][0] = 0.0f; mtx->m[2][1] = 0.0f; mtx->m[2][2] = 0.0f;
}