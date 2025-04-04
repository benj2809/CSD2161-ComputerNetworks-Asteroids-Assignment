/* Start Header ************************************************************************/
/*!
\file Collision.h
\co author Ho Jing Rui
\co author Saminathan Aaron Nicholas
\co author Jay Lim Jun Xiang
\par emails: jingrui.ho@digipen.edu
\	         s.aaronnicholas@digipen.edu
\	         jayjunxiang.lim@digipen.edu
\date 28 March, 2025
\brief This file declares functions
bool CollisionIntersection_RectRect(const AABB & aabb1,          //Input
									const AEVec2 & vel1,         //Input
									const AABB & aabb2,          //Input
									const AEVec2 & vel2,         //Input
									float& firstTimeOfCollision);

Copyright (C) 2024 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/* End Header **************************************************************************/


#ifndef CSD1130_COLLISION_H_
#define CSD1130_COLLISION_H_

#include "AEEngine.h"

/**************************************************************************/
/*!

	*/
	/**************************************************************************/
struct AABB
{
	AEVec2	min;
	AEVec2	max;
};

bool CollisionIntersection_RectRect(const AABB& aabb1,            //Input
	const AEVec2& vel1,           //Input 
	const AABB& aabb2,            //Input 
	const AEVec2& vel2,           //Input
	float& firstTimeOfCollision); //Output: the calculated value of tFirst, must be returned here


#endif // CSD1130_COLLISION_H_