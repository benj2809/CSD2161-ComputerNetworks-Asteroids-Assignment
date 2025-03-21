/******************************************************************************/
/*!
\file		GameState_Asteroids.cpp
\author		Ong Jun Han Benjamin, o.junhanbenjamin, 2301532
\par		o.junhanbenjamin\@digipen.edu
\date		Feb 08, 2024
\brief		This file contains the functions:	- GameStateAsteroidsLoad;
												- GameStateAsteroidsInit;
												- GameStateAsteroidsUpdate;
												- GameStateAsteroidsDraw;
												- GameStateAsteroidsFree;
												- GameStateAsteroidsUnload;
												- gameObjInstCreate;
												- gameObjInstDestroy;
												- Helper_Wall_Collision;

			Each of the functions mentioned above contains blocks of code for movement and collision etc.
			More documentation are provided below.

Copyright (C) 20xx DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
/******************************************************************************/

#include "main.h"
#include <iostream>

/******************************************************************************/
/*!
	Defines
*/
/******************************************************************************/
const unsigned int	GAME_OBJ_NUM_MAX		= 32;			// The total number of different objects (Shapes)
const unsigned int	GAME_OBJ_INST_NUM_MAX	= 2048;			// The total number of different game object instances


const unsigned int	SHIP_INITIAL_NUM		= 3;			// initial number of ship lives
const float			SHIP_SCALE_X			= 16.0f;		// ship scale x
const float			SHIP_SCALE_Y			= 16.0f;		// ship scale y
const float			BULLET_SCALE_X			= 20.0f;		// bullet scale x
const float			BULLET_SCALE_Y			= 3.0f;			// bullet scale y
const float			ASTEROID_MIN_SCALE_X	= 10.0f;		// asteroid minimum scale x
const float			ASTEROID_MAX_SCALE_X	= 60.0f;		// asteroid maximum scale x
const float			ASTEROID_MIN_SCALE_Y	= 10.0f;		// asteroid minimum scale y
const float			ASTEROID_MAX_SCALE_Y	= 60.0f;		// asteroid maximum scale y

const float			WALL_SCALE_X			= 64.0f;		// wall scale x
const float			WALL_SCALE_Y			= 164.0f;		// wall scale y

const float			SHIP_ACCEL_FORWARD		= 100.0f;		// ship forward acceleration (in m/s^2)
const float			SHIP_ACCEL_BACKWARD		= 100.0f;		// ship backward acceleration (in m/s^2)
const float			SHIP_ROT_SPEED			= (2.0f * PI);	// ship rotation speed (degree/second)

const float			BULLET_SPEED			= 400.0f;		// bullet speed (m/s)

const float         BOUNDING_RECT_SIZE      = 1.0f;         // this is the normalized bounding rectangle (width and height) sizes - AABB collision data

static bool onValueChange{ true };

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

const unsigned long FLAG_ACTIVE				= 0x00000001;

/******************************************************************************/
/*!
	Struct/Class Definitions
*/
/******************************************************************************/

//Game object structure
struct GameObj
{
	unsigned long		type;		// object type
	AEGfxVertexList *	pMesh;		// This will hold the triangles which will form the shape of the object
};

// ---------------------------------------------------------------------------

//Game object instance structure
struct GameObjInst
{
	GameObj *			pObject;	// pointer to the 'original' shape
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
static GameObjInst *		spShip;										// Pointer to the "Ship" game object instance

// pointer to the wall object
static GameObjInst *		spWall;										// Pointer to the "Wall" game object instance

// number of ship available (lives 0 = game over)
static long					sShipLives;									// The number of lives left

// the score = number of asteroid destroyed
static unsigned long		sScore;										// Current score

// ---------------------------------------------------------------------------

// functions to create/destroy a game object instance
GameObjInst *		gameObjInstCreate (unsigned long type, AEVec2* scale,
											   AEVec2 * pPos, AEVec2 * pVel, float dir);
void				gameObjInstDestroy(GameObjInst * pInst);

void				Helper_Wall_Collision();


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
		-0.5f, 0.5f, 0xFFFF0000, 0.0f, 0.0f,
		-0.5f, -0.5f, 0xFFFF0000, 0.0f, 0.0f,
		0.5f, 0.0f, 0xFFFFFFFF, 0.0f, 0.0f);

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
	// create the main ship
	AEVec2 scale;
	AEVec2Set(&scale, SHIP_SCALE_X, SHIP_SCALE_Y);
	spShip = gameObjInstCreate(TYPE_SHIP, &scale, nullptr, nullptr, 0.0f);
	AE_ASSERT(spShip);
	AEVec2 pos{}, vel{};

	//Asteroid 1
	pos.x = 90.0f;		pos.y = -220.0f;
	vel.x = -60.0f;		vel.y = -30.0f;
	AEVec2Set(&scale, ASTEROID_MIN_SCALE_X, ASTEROID_MAX_SCALE_Y);
	gameObjInstCreate(TYPE_ASTEROID, &scale, &pos, &vel, 0.0f);

	//Asteroid 2
	pos.x = -260.0f;	pos.y = -250.0f;
	vel.x = 39.0f;		vel.y = -130.0f;
	AEVec2Set(&scale, ASTEROID_MAX_SCALE_X, ASTEROID_MIN_SCALE_Y);
	gameObjInstCreate(TYPE_ASTEROID, &scale, &pos, &vel, 0.0f);

	//Asteroid 3
	pos.x = -200.0f;	pos.y = -150.0f;
	vel.x = 40.0f;		vel.y = -200.0f;
	AEVec2Set(&scale, ASTEROID_MAX_SCALE_X / 2.f, ASTEROID_MIN_SCALE_Y / 2.f);
	gameObjInstCreate(TYPE_ASTEROID, &scale, &pos, &vel, 0.0f);

	//Asteroid 4
	pos.x = 300.0f;		pos.y = -115.0f;
	vel.x = -25.0f;		vel.y = -100.0f;
	AEVec2Set(&scale, ASTEROID_MIN_SCALE_X / 2.f, ASTEROID_MAX_SCALE_Y / 2.f);
	gameObjInstCreate(TYPE_ASTEROID, &scale, &pos, &vel, 0.0f);

	// create the static wall
	AEVec2Set(&scale, WALL_SCALE_X, WALL_SCALE_Y);
	AEVec2 position{};
	AEVec2Set(&position, 300.0f, 150.0f);
	spWall = gameObjInstCreate(TYPE_WALL, &scale, &position, nullptr, 0.0f);
	AE_ASSERT(spWall);

	// reset the score and the number of ships
	sScore      = 0;
	sShipLives  = SHIP_INITIAL_NUM;
}

/******************************************************************************/
/*!
	"Update" function of this state
*/
/******************************************************************************/
void GameStateAsteroidsUpdate(void)
{
	// =========================================================
	// Update according to input.
	// Movement input is updated only when the game is active.
	// Game is active only when the max score (5000) has not been achieved yet,
	// or when there are still remaining ship lives.
	// =========================================================

	// This input handling moves the ship without any velocity nor acceleration
	// It should be changed when implementing the Asteroids project
	//
	// Updating the velocity and position according to acceleration is 
	// done by using the following:
	// Pos1 = 1/2 * a*t*t + v0*t + Pos0
	//
	// In our case we need to divide the previous equation into two parts in order 
	// to have control over the velocity and that is done by:
	//
	// v1 = a*t + v0		//This is done when the UP or DOWN key is pressed 
	// Pos1 = v1*t + Pos0
	
	if (!(sScore >= 5000)) {
		if (!(sShipLives < 0)) {
			if (AEInputCheckCurr(AEVK_UP)) // Moving forward
			{
				AEVec2 added, newVel{}, newPos{};
				AEVec2Set(&added, cosf(spShip->dirCurr), sinf(spShip->dirCurr)); // Creating vector of the direction of the acceleration.
				AEVec2Normalize(&added, &added); // Normalizing the vector
				added.x *= SHIP_ACCEL_FORWARD; // * Acceleration
				added.y *= SHIP_ACCEL_FORWARD; // * Acceleration
				AEVec2Scale(&added, &added, (float)AEFrameRateControllerGetFrameTime()); // * dt - Makes time-based movement, which will cover the same amount of distance regardless of frame rate.
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
			if (AEInputCheckTriggered(AEVK_SPACE)) // Creating a bullet when space is triggered.
			{ 
				AEVec2 scale{}, pos{}, vel{};
				AEVec2Set(&scale, BULLET_SCALE_X, BULLET_SCALE_Y); // Vector for scaling the bullet
				AEVec2Set(&pos, spShip->posCurr.x, spShip->posCurr.y); // Vector for the position of where the bullet is created - In this case it is created at the ship's location.
				AEVec2Set(&vel, BULLET_SPEED * cosf(spShip->dirCurr), BULLET_SPEED * sinf(spShip->dirCurr)); // Vector for velocity of the bullet, it shoots in the direction the ship is facing.
				gameObjInstCreate(TYPE_BULLET, &scale, &pos, &vel, spShip->dirCurr); // Creating an instance for the bullet.
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
	Helper_Wall_Collision(); // More information can be found below, at the definition of the function. TLDR; calls function CollisionIntersection_RectRect, definition found in Collision.cpp.

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
		GameObjInst * pInst = sGameObjInstList + i;

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

	// Displaying ship lives and score values to user should there be an update to either values.
	if(onValueChange)
	{
		sprintf_s(strBuffer, "Score: %d", sScore);
		//AEGfxPrint(10, 10, (u32)-1, strBuffer);
		printf("%s \n", strBuffer);

		sprintf_s(strBuffer, "Ship Left: %d", sShipLives >= 0 ? sShipLives : 0);
		//AEGfxPrint(600, 10, (u32)-1, strBuffer);
		printf("%s \n", strBuffer);
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
		GameObj * pObject = sGameObjList + i; 
		AEGfxMeshFree(pObject->pMesh); // Freeing the mesh.
		pObject->pMesh = NULL; // Setting it to NULL after freeing.
	}
}

/******************************************************************************/
/*!
	Function to aid in creating an instance for a object. Essentially initializing the new instance with the appropriate values.
*/
/******************************************************************************/
GameObjInst * gameObjInstCreate(unsigned long type, 
							   AEVec2 * scale,
							   AEVec2 * pPos, 
							   AEVec2 * pVel, 
							   float dir)
{
	AEVec2 zero;
	AEVec2Zero(&zero); // Zero'd out vector to assign data members assigned to nullptr.

	AE_ASSERT_PARM(type < sGameObjNum); // Error message if the type of instance to be created is not of the correct type.
	
	// loop through the object instance list to find a non-used object instance
	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjInst * pInst = sGameObjInstList + i;

		// check if current instance is not used
		if (pInst->flag == 0)
		{
			// it is not used => use it to create the new instance
			pInst->pObject	= sGameObjList + type;
			pInst->flag		= FLAG_ACTIVE;
			pInst->scale	= *scale;
			pInst->posCurr	= pPos ? *pPos : zero;
			pInst->velCurr	= pVel ? *pVel : zero;
			pInst->dirCurr	= dir;
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
void gameObjInstDestroy(GameObjInst * pInst)
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
