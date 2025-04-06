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
const float SHIP_SCALE_X = 15.0f;
const float SHIP_SCALE_Y = 15.0f;
const float BULLET_SCALE_X = 20.0f;
const float BULLET_SCALE_Y = 3.0f;
const float ASTEROID_MIN_SCALE_X = 15.0f;
const float ASTEROID_MAX_SCALE_X = 50.0f;
const float ASTEROID_MIN_SCALE_Y = 15.0f;
const float ASTEROID_MAX_SCALE_Y = 50.0f;

// Movement parameters
const float SHIP_ACCEL_FORWARD = 100.0f;
const float SHIP_ACCEL_BACKWARD = 100.0f;
const float SHIP_ROT_SPEED = (2.0f * PI);
const float BULLET_SPEED = 500.0f;
const float BOUNDING_RECT_SIZE = 1.0f;

// Game state
static bool updateValue{true};
bool endGame = false;
int winningPlayerID = -1; // Initialize winningPlayerID to -1 to indicate no winner yet
int playerCount = 1;

enum TYPE {
	TYPE_SHIP = 0,
	TYPE_BULLET,
	TYPE_ASTEROID,
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
	static std::unordered_map<int, int> old_score;
	static bool newscore_flag = false;
	static bool gameOverMessageShown = false;

	newscore_flag = false;
	for (auto it = players.begin(); it != players.end(); ++it)
	{
		int id = it->first;
		const PlayerData& player = it->second;

		std::unordered_map<int, int>::iterator scoreIt = old_score.find(id);

		if (scoreIt == old_score.end())
		{
			old_score[id] = player.score;
			newscore_flag = true;
		}
		else if (scoreIt->second != player.score)
		{
			scoreIt->second = player.score;
			newscore_flag = true;
		}


		if (player.score >= 2500 && !endGame)
		{
			endGame = true;
			winningPlayerID = id;
		}
	}

	if (old_score.size() != players.size()) {
		newscore_flag = true;
	}

	if (newscore_flag) {
		GameClient::displayPlayerScores();
	}

	for (auto it = old_score.begin(); it != old_score.end();) {
		if (players.find(it->first) == players.end()) {
			it = old_score.erase(it);
		}
		else {
			++it;
		}
	}

	// Game over condition
	if (endGame) {
		int high_score = -1;
		std::vector<int> highscoring_player;

		for (auto it = players.begin(); it != players.end(); ++it)
		{
			int id = it->first;
			const PlayerData& player = it->second;

			if (player.score > high_score)
			{
				high_score = player.score;
				highscoring_player.clear();
				highscoring_player.push_back(id);
			}
			else if (player.score == high_score)
			{
				highscoring_player.push_back(id);
			}
		}

		return;
	}

	// Only process input if the player hasn't won or lost yet
	if (shipScore < 2500) {

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

				{
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
				}

				globalClient.sendBulletCreationEvent(bulletSpawnPos, bulletVelocity, ship->dirCurr, uniqueBulletID);
			}
		}
	}

	// ======================================================================
	// Save previous positions
	//  -- For all instances
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
	// check for collisions
	// ======================================================================

	if (shipScore < 2500) {
		//for collision between rect-rect function 
		float tfirst = 0;
		// ======================================================================
		// check for dynamic-dynamic collisions
		// ======================================================================
		for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
		{
			GameObjInst* pInst = sGameObjInstList + i;

			// skip non-active object
			if ((pInst->flag & FLAG_ACTIVE) == 0) {
				continue;
			}

			if (pInst->pObject->type == TYPE_ASTEROID) {
				for (unsigned long j = 0; j < GAME_OBJ_INST_NUM_MAX - 1; j++) {
					int asteroid_max_vel = 200;
					int corner = rand() % 4;
					int width = 0;
					int height = 0;
					int x = 0;
					int y = 0;
					float vel_y = 0.0f;
					float vel_x = 0.0f;
					float scale_y = 0.0f;
					float scale_x = 0.0f;

					if (i == j) {
						continue;
					}
					GameObjInst* next_obj = sGameObjInstList + j;

					if ((next_obj->flag & FLAG_ACTIVE) == 0) {
						continue;
					}
					if (next_obj->pObject->type == TYPE_ASTEROID || (next_obj->flag & FLAG_ACTIVE) == 0) {
						continue;
					}
					if (next_obj->pObject->type == TYPE_SHIP) {
						//if there is collision between ship and asteroid, ship lives -1. If ship lives reaches below 0, reset position and velocity to 0.
						if (CollisionIntersection_RectRect(pInst->boundingBox, pInst->velCurr, next_obj->boundingBox, next_obj->velCurr, tfirst)) {
							updateValue = true;

							if (shipLives < 0) {
								next_obj->velCurr.x = 0;
								next_obj->velCurr.y = 0;
								next_obj->posCurr.x = 0;
								next_obj->posCurr.y = 0;

							}

							gameObjInstDestroy(pInst);
							next_obj->posCurr.x = 0;
							next_obj->posCurr.y = 0;
							next_obj->dirCurr = 0;
							//random asteroid pos velocity

							for (int ran = 0; ran < 1; ran++) {


								//spawn top corner
								//Randomnise new asteroid size and velocity 
								if (corner == 0) {
									width = (rand() % 2 == 0) ? 1 : -1;
									x = rand() % (AEGfxGetWindowWidth() / 2) * width;
									y = (AEGfxGetWindowHeight() / 2);
									scale_x = static_cast<float>(rand() % static_cast<int>(ASTEROID_MAX_SCALE_X - ASTEROID_MIN_SCALE_X));
									scale_y = static_cast<float>(rand() % static_cast<int>(ASTEROID_MAX_SCALE_Y - ASTEROID_MIN_SCALE_Y));


									vel_y = (rand() % 2 == 0) ? static_cast<float>(1 * (rand() % asteroid_max_vel)) : static_cast<float>(-1 * (rand() % asteroid_max_vel));
									vel_x = (rand() % 2 == 0) ? static_cast<float>(1 * (rand() % asteroid_max_vel)) : static_cast<float>(-1 * (rand() % asteroid_max_vel));



								}
								//spawn bottom corner
								if (corner == 1) {
									width = (rand() % 2 == 0) ? 1 : -1;
									x = rand() % (AEGfxGetWindowWidth() / 2) * width;
									y = -(AEGfxGetWindowHeight() / 2);
									scale_x = static_cast<float>(rand() % static_cast<int>(ASTEROID_MAX_SCALE_X - ASTEROID_MIN_SCALE_X));
									scale_y = static_cast<float>(rand() % static_cast<int>(ASTEROID_MAX_SCALE_Y - ASTEROID_MIN_SCALE_Y));
									vel_y = (rand() % 2 == 0) ? static_cast<float>(1 * (rand() % asteroid_max_vel)) : static_cast<float>(-1 * (rand() % asteroid_max_vel));
									vel_x = (rand() % 2 == 0) ? static_cast<float>(1 * (rand() % asteroid_max_vel)) : static_cast<float>(-1 * (rand() % asteroid_max_vel));

								}
								//left corner
								if (corner == 2) {
									height = (rand() % 2 == 0) ? 1 : -1;
									y = rand() % (AEGfxGetWindowWidth() / 2) * height;
									x = -(AEGfxGetWindowHeight() / 2);
									scale_x = static_cast<float>(rand() % static_cast<int>(ASTEROID_MAX_SCALE_X - ASTEROID_MIN_SCALE_X));
									scale_y = static_cast<float>(rand() % static_cast<int>(ASTEROID_MAX_SCALE_Y - ASTEROID_MIN_SCALE_Y));
									vel_y = (rand() % 2 == 0) ? static_cast<float>(1 * (rand() % asteroid_max_vel)) : static_cast<float>(-1 * (rand() % asteroid_max_vel));
									vel_x = (rand() % 2 == 0) ? static_cast<float>(1 * (rand() % asteroid_max_vel)) : static_cast<float>(-1 * (rand() % asteroid_max_vel));

								}
								//right corner
								if (corner == 3) {
									height = (rand() % 2 == 0) ? 1 : -1;
									y = rand() % (AEGfxGetWindowWidth() / 2) * height;
									x = (AEGfxGetWindowHeight() / 2);
									scale_x = static_cast<float>(rand() % static_cast<int>(ASTEROID_MAX_SCALE_X - ASTEROID_MIN_SCALE_X));
									scale_y = static_cast<float>(rand() % static_cast<int>(ASTEROID_MAX_SCALE_Y - ASTEROID_MIN_SCALE_Y));
									vel_y = (rand() % 2 == 0) ? static_cast<float>(1 * (rand() % asteroid_max_vel)) : static_cast<float>(-1 * (rand() % asteroid_max_vel));
									vel_x = (rand() % 2 == 0) ? static_cast<float>(1 * (rand() % asteroid_max_vel)) : static_cast<float>(-1 * (rand() % asteroid_max_vel));

								}
								//set and create new asteroid
								AEVec2 scale, pos, vel;
								pos.x = static_cast<float>(x);
								pos.y = static_cast<float>(y);
								vel.x = vel_x;
								vel.y = vel_y;
								scale.x = scale_x;
								scale.y = scale_y;
								AEVec2Set(&scale, scale.x, scale.y);
								gameObjInstCreate(TYPE_ASTEROID, &scale, &pos, &vel, 0.0f);

							}

						}
					}
					//if asteroid and bullet collides
					if (next_obj->pObject->type == TYPE_BULLET) {
						if (CollisionIntersection_RectRect(pInst->boundingBox, pInst->velCurr, next_obj->boundingBox, next_obj->velCurr, tfirst)) {

							gameObjInstDestroy(pInst);
							gameObjInstDestroy(next_obj);
							shipScore += 100;
							updateValue = true;
							// will randonmly spawn either 1 or 2 asteroid with different scaling and velocity
							for (int bull = 0; bull < 1 + rand() % 2; bull++) {


								//spawn top border
								if (corner == 0) {
									width = (rand() % 2 == 0) ? 1 : -1;
									x = rand() % (AEGfxGetWindowWidth() / 2) * width;
									y = (AEGfxGetWindowHeight() / 2);
									vel_y = (rand() % 2 == 0) ? static_cast<float>(1 * (rand() % asteroid_max_vel)) : static_cast<float>(-1 * (rand() % asteroid_max_vel));
									vel_x = (rand() % 2 == 0) ? static_cast<float>(1 * (rand() % asteroid_max_vel)) : static_cast<float>(-1 * (rand() % asteroid_max_vel));

									scale_x = static_cast<float>(rand() % static_cast<int>(ASTEROID_MAX_SCALE_X - ASTEROID_MIN_SCALE_X));
									scale_y = static_cast<float>(rand() % static_cast<int>(ASTEROID_MAX_SCALE_Y - ASTEROID_MIN_SCALE_Y));

								}
								//spawn bottom corner
								if (corner == 1) {
									width = (rand() % 2 == 0) ? 1 : -1;
									x = rand() % (AEGfxGetWindowWidth() / 2) * width;
									y = -(AEGfxGetWindowHeight() / 2);
									vel_y = (rand() % 2 == 0) ? static_cast<float>(1 * (rand() % asteroid_max_vel)) : static_cast<float>(-1 * (rand() % asteroid_max_vel));
									vel_x = (rand() % 2 == 0) ? static_cast<float>(1 * (rand() % asteroid_max_vel)) : static_cast<float>(-1 * (rand() % asteroid_max_vel));
									scale_x = static_cast<float>(rand() % static_cast<int>(ASTEROID_MAX_SCALE_X - ASTEROID_MIN_SCALE_X));
									scale_y = static_cast<float>(rand() % static_cast<int>(ASTEROID_MAX_SCALE_Y - ASTEROID_MIN_SCALE_Y));
								}
								//left corner
								if (corner == 2) {
									height = (rand() % 2 == 0) ? 1 : -1;
									y = rand() % (AEGfxGetWindowWidth() / 2) * height;
									x = -(AEGfxGetWindowHeight() / 2);
									vel_y = (rand() % 2 == 0) ? static_cast<float>(1 * (rand() % asteroid_max_vel)) : static_cast<float>(-1 * (rand() % asteroid_max_vel));
									vel_x = (rand() % 2 == 0) ? static_cast<float>(1 * (rand() % asteroid_max_vel)) : static_cast<float>(-1 * (rand() % asteroid_max_vel));
									scale_x = static_cast<float>(rand() % static_cast<int>(ASTEROID_MAX_SCALE_X - ASTEROID_MIN_SCALE_X));
									scale_y = static_cast<float>(rand() % static_cast<int>(ASTEROID_MAX_SCALE_Y - ASTEROID_MIN_SCALE_Y));
								}
								//right corner
								if (corner == 3) {
									height = (rand() % 2 == 0) ? 1 : -1;
									y = rand() % (AEGfxGetWindowWidth() / 2) * height;
									x = (AEGfxGetWindowHeight() / 2);
									vel_y = (rand() % 2 == 0) ? static_cast<float>(1 * (rand() % asteroid_max_vel)) : static_cast<float>(-1 * (rand() % asteroid_max_vel));
									vel_x = (rand() % 2 == 0) ? static_cast<float>(1 * (rand() % asteroid_max_vel)) : static_cast<float>(-1 * (rand() % asteroid_max_vel));
									scale_x = static_cast<float>(rand() % static_cast<int>(ASTEROID_MAX_SCALE_X - ASTEROID_MIN_SCALE_X));
									scale_y = static_cast<float>(rand() % static_cast<int>(ASTEROID_MAX_SCALE_Y - ASTEROID_MIN_SCALE_Y));
								}
								//set new asteroid scaling and velocity.
								AEVec2 scale, pos, vel;
								pos.x = static_cast<float>(x);
								pos.y = static_cast<float>(y);
								vel.x = vel_x;
								vel.y = vel_y;
								scale.x = scale_x;
								scale.y = scale_y;
								AEVec2Set(&scale, scale.x, scale.x);
								gameObjInstCreate(TYPE_ASTEROID, &scale, &pos, &vel, 0.0f);

							}

						}
					}


				}

			}
		}

	}

	// ===================================================================
	// update active game object instances
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
	localRemoteActiveInstCheck();
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
	SetUpRenderState();
	DrawGameObjects();
	DrawAsteroids();
	DrawBullets();

	if (updateValue)
	{
		updateValue = false;
	}

	// Display winner if the game is over
	if (endGame)
	{
		DrawGameOverScreen();
	}

	DrawNames(players);
	DrawPlayerScores();
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
void localRemoteActiveInstCheck() {
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
void DrawNames(std::unordered_map<int, PlayerData>& pData) {
	// Cache window boundaries and dimensions ONCE
	f32 winMinX = AEGfxGetWinMinX();
	f32 winMaxX = AEGfxGetWinMaxX();
	f32 winMinY = AEGfxGetWinMinY();
	f32 winMaxY = AEGfxGetWinMaxY();

	f32 windowWidth = winMaxX - winMinX;
	f32 windowHeight = winMaxY - winMinY;

	// Setup render state ONCE
	AEGfxSetRenderMode(AE_GFX_RM_COLOR);
	AEGfxSetBlendMode(AE_GFX_BM_BLEND);
	AEGfxTextureSet(NULL, 0, 0);
	AEGfxSetTransparency(1.0f);

	// Draw player names
	for (const auto& pair : pData)
	{
		const PlayerData& player = pair.second;

		AEVec2 position;
		AEVec2Set(&position, player.X, player.Y);

		// Wrap the position
		f32 wrappedX = AEWrap(position.x, winMinX - SHIP_SCALE_X, winMaxX + SHIP_SCALE_X);
		f32 wrappedY = AEWrap(position.y, winMinY - SHIP_SCALE_Y, winMaxY + SHIP_SCALE_Y);

		// Normalize to screen space [-1, 1]
		f32 normalizedX = ((wrappedX - winMinX) / windowWidth) * 2.0f - 1.0f;
		f32 normalizedY = ((wrappedY - winMinY) / windowHeight) * 2.0f - 1.0f;

		// Offset for better name placement
		normalizedX -= 0.2f;
		normalizedY += 0.1f;

		// Format player name
		char nameBuffer[100];
		snprintf(nameBuffer, sizeof(nameBuffer), "Player %d", pair.first);

		// Render the name
		AEGfxPrint(fontId, nameBuffer, normalizedX, normalizedY, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
	}
}

/**
 * @brief Renders server-side asteroids by updating their positions and drawing them.
 *
 * This function updates asteroid interpolation and renders them with correct transformations.
 */
void DrawAsteroids() {
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
void DrawBullets() {

	// Temporary list to manage instances
	std::vector<GameObjInst*> tempBullets;

	{
		GameClient::ScopedBulletLock lock;
		// Pre-create all temporary bullets
		for (const auto& pair : bullets)
		{
			const BulletData& bullet = pair.second;

			if (bullet.fromLocalPlayer) {
				continue; // Skip local bullets
			}

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

/******************************************************************************/
/*!
	@brief Sets up the rendering state for 2D drawing.

	This function configures the graphics engine to use color rendering, enables
	alpha blending for transparency, and binds no textures. Should be called before
	drawing game objects or UI elements.
*/
/******************************************************************************/
void SetUpRenderState()
{
	AEGfxSetRenderMode(AE_GFX_RM_COLOR);
	AEGfxSetBlendMode(AE_GFX_BM_BLEND);
	AEGfxSetTransparency(1.0f);
	AEGfxTextureSet(nullptr, 0, 0);
}

/******************************************************************************/
/*!
	@brief Sets up the rendering state for displaying text.

	This function configures the graphics engine to use color rendering,
	enables alpha blending for transparency, unbinds any texture,
	and sets full transparency (alpha = 1.0f).
	Should be called before printing any text (e.g., scores, messages).
*/
/******************************************************************************/
void SetUpTextRenderState()
{
	AEGfxSetRenderMode(AE_GFX_RM_COLOR);
	AEGfxSetBlendMode(AE_GFX_BM_BLEND);
	AEGfxTextureSet(nullptr, 0, 0);
	AEGfxSetTransparency(1.0f);
}

/******************************************************************************/
/*!
	@brief Draws all active game object instances.

	Iterates through all object instances in the global list and renders any
	instance flagged as active by applying its transform and mesh.
*/
/******************************************************************************/
void DrawGameObjects()
{
	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjInst* pInst = sGameObjInstList + i;
		if ((pInst->flag & FLAG_ACTIVE) == 0) {
			continue;
		}

		AEGfxSetTransform(pInst->transform.m);
		AEGfxMeshDraw(pInst->pObject->pMesh, AE_GFX_MDM_TRIANGLES);
	}
}

/******************************************************************************/
/*!
	@brief Displays the "Game Over" screen.

	If a winning player ID exists, their victory message is shown.
	Otherwise, a general "YOU WIN" message is displayed at the center of the screen.
*/
/******************************************************************************/
void DrawGameOverScreen()
{
	SetUpRenderState(); // Ensure transparency is correct for text
	AEGfxPrint(fontId, "GAME OVER", -0.45f, 0.0f, 3.0f, 1.0f, 0.0f, 0.0f, 1.0f);
	char winText[64];
	if (winningPlayerID != -1) {
		snprintf(winText, sizeof(winText), "Player %d Wins!", winningPlayerID);
	}
	AEGfxPrint(fontId, winText, -0.35f, -0.60f, 2.0f, 0.0f, 1.0f, 0.0f, 1.0f);
}

/******************************************************************************/
/*!
	@brief Draws scores for all players.

	Loops through all player entries and renders their current scores on the screen.
	Highlights the local player's score if applicable.
*/
/******************************************************************************/
void DrawPlayerScores()
{
	for (auto& p : players)
	{
		DisplayScores(players, p.first);
	}
}

/******************************************************************************/
/*!
	@brief Calculates normalized screen coordinates for a given world position.

	This function converts a given (x, y) world position into normalized
	screen space coordinates in the range [-1, 1], adjusting with a small
	horizontal and vertical offset to better align text displays.

	@param x The world-space x position.
	@param y The world-space y position.
	@return AEVec2 The normalized screen-space position.
*/
/******************************************************************************/
AEVec2 GetNormalizedScreenPos(float x, float y)
{
	float winMinX = AEGfxGetWinMinX();
	float winMaxX = AEGfxGetWinMaxX();
	float winMinY = AEGfxGetWinMinY();
	float winMaxY = AEGfxGetWinMaxY();

	float normalizedX = ((x - winMinX) / (winMaxX - winMinX)) * 2.0f - 1.0f - 0.2f;
	float normalizedY = ((y - winMinY) / (winMaxY - winMinY)) * 2.0f - 1.0f + 0.1f;

	return { normalizedX, normalizedY };
}

/**
 * @brief Displays the scores of all players, highlighting the local player?s score.
 *
 * This function iterates through all players, renders their score at the correct position, and highlights the local player's score in green.
 *
 * @param gamePlayers A map containing player data, keyed by player ID.
 * @param playerID The local player's ID.
 */
void DisplayScores(const std::unordered_map<int, PlayerData>& allPlayers, int localPlayerID)
{
	SetUpTextRenderState();

	for (const auto& pair : allPlayers)
	{
		int id = pair.first;
		const PlayerData& player = pair.second;

		AEVec2 wrappedPos = {
			AEWrap(player.X, AEGfxGetWinMinX() - SHIP_SCALE_X, AEGfxGetWinMaxX() + SHIP_SCALE_X),
			AEWrap(player.Y, AEGfxGetWinMinY() - SHIP_SCALE_Y, AEGfxGetWinMaxY() + SHIP_SCALE_Y)
		};

		AEVec2 screenPos = GetNormalizedScreenPos(wrappedPos.x, wrappedPos.y);

		char scoreText[64];
		snprintf(scoreText, sizeof(scoreText), "Player %d: %d points", id, player.score);

		if (id == localPlayerID)
		{
			AEGfxPrint(fontId, scoreText, screenPos.x, screenPos.y, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
		}
		else
		{
			AEGfxPrint(fontId, scoreText, screenPos.x, screenPos.y, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
		}
	}
}

/**
 * @brief Zeroes out the matrix
 *
 * This function zeroes out the matrix that is passed into it.
 *
 * @param mtx A matrix.
 */
void AEMtx33Zero(AEMtx33* mtx) {
	mtx->m[0][0] = 0.0f; mtx->m[0][1] = 0.0f; mtx->m[0][2] = 0.0f;
	mtx->m[1][0] = 0.0f; mtx->m[1][1] = 0.0f; mtx->m[1][2] = 0.0f;
	mtx->m[2][0] = 0.0f; mtx->m[2][1] = 0.0f; mtx->m[2][2] = 0.0f;
}