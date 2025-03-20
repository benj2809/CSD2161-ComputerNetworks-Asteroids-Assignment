/******************************************************************************/
/*!
\file		Collision.h
\author		Ong Jun Han Benjamin, o.junhanbenjamin, 2301532
\par		o.junhanbenjamin\@digipen.edu
\date		Feb 08, 2024
\brief		This file contains the declarations for function CollisionIntersection_RectRect and struct AABB, to be used for collision detection.

			Defintion and documentation found in Collision.cpp.

Copyright (C) 20xx DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
/******************************************************************************/

#ifndef CSD1130_COLLISION_H_
#define CSD1130_COLLISION_H_

#include "AEEngine.h"

/**************************************************************************/
/*!
	Struct AABB and data vectors min and max.
*/
/**************************************************************************/
struct AABB
{
	AEVec2	min;
	AEVec2	max;
};

/**************************************************************************/
/*!
	Declaration for the collision function.
 */
/**************************************************************************/
bool CollisionIntersection_RectRect(const AABB& aabb1,            //Input
									const AEVec2& vel1,           //Input 
									const AABB& aabb2,            //Input 
									const AEVec2& vel2,           //Input
									float& firstTimeOfCollision); //Output: the calculated value of tFirst, must be returned here


#endif // CSD1130_COLLISION_H_