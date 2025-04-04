/* Start Header
*******************************************************************/
/*!
\file Collision.cpp
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

/**************************************************************************/
/*!
	AABB collision to detect overlaps of min and max x and y values of each object
	In
		- aabb1: min and max x and y values for 1st object
		- vel1: velocity of 1st object
		- aabb2: min and max x and y values for 2nd object
		- vel2: velocity of 2nd object
		- firstTimeOfCollision: no collision is less than this
*/
/**************************************************************************/
bool CollisionIntersection_RectRect(const AABB& aabb1,          //Input
	const AEVec2& vel1,         //Input 
	const AABB& aabb2,          //Input 
	const AEVec2& vel2,         //Input
	float& firstTimeOfCollision) //Output: the calculated value of tFirst, below, must be returned here
{
	UNREFERENCED_PARAMETER(aabb1);
	UNREFERENCED_PARAMETER(vel1);
	UNREFERENCED_PARAMETER(aabb2);
	UNREFERENCED_PARAMETER(vel2);
	UNREFERENCED_PARAMETER(firstTimeOfCollision);

	AEVec2 vel_rel;

	vel_rel.x = vel2.x - vel1.x;
	vel_rel.y = vel2.y - vel1.y;
	//collision for one side
	if (aabb1.max.x < aabb2.min.x) {
		return 0;
	}
	if (aabb1.max.y < aabb2.min.y) {
		return 0;
	}
	//collision for the other side
	if (aabb1.min.x > aabb2.max.x) {
		return 0;
	}
	if (aabb1.min.y > aabb2.max.y) {
		return 0;
	}
	//collided



	float t_first = 0;
	float t_last = g_dt;

	// for x axis
	//case for vel < 0;
	if (vel_rel.x < 0) {
		//case1
		if (aabb1.min.x > aabb2.max.x) {
			return 0;
		}
		//case4
		if (aabb1.max.x < aabb2.min.x) {
			t_first = max(((aabb1.max.x - aabb2.min.x) / vel_rel.x), t_first);
		}
		if (aabb1.min.x < aabb2.max.x) {
			t_last = min(((aabb1.min.x - aabb2.max.x) / vel_rel.x), t_last);

		}

		//case for velocity more than 0
	}
	if (vel_rel.x > 0) {
		//case2
		if (aabb1.min.x > aabb2.max.x) {
			t_first = max(((aabb1.min.x - aabb2.max.x) / vel_rel.x), t_first);
		}
		if (aabb1.max.x > aabb2.min.x) {
			t_last = min(((aabb1.max.x - aabb2.min.x) / vel_rel.x), t_last);
		}
		//case3
		if (aabb1.max.x < aabb2.min.x) {
			return 0;
		}

	}
	//case 5 parallel towards the opposite coordinates
	if (vel_rel.x == 0) {
		if (aabb1.max.x < aabb2.min.x) {
			return 0;
		}
		else if (aabb1.min.x > aabb2.max.x) {
			return 0;
		}
	}
	//case 6
	if (t_first > t_last) {
		return 0;
	}




	// for y axis
	if (vel_rel.y < 0) {
		//case1
		if (aabb1.min.y > aabb2.max.y) {
			return 0;
		}
		//case4
		if (aabb1.max.y < aabb2.min.y) {
			t_first = max(((aabb1.max.y - aabb2.min.y) / vel_rel.y), t_first);
		}
		if (aabb1.min.y < aabb2.max.y) {
			t_last = min(((aabb1.min.y - aabb2.max.y) / vel_rel.y), t_last);
		}
	}
	//for velocity more than 0
	if (vel_rel.y > 0) {
		//case2
		if (aabb1.min.y > aabb2.max.y) {
			t_first = max(((aabb1.min.y - aabb2.max.y) / vel_rel.y), t_first);
		}
		if (aabb1.max.y > aabb2.min.y) {
			t_last = min(((aabb1.max.y - aabb2.min.y) / vel_rel.y), t_last);
		}
		//case3
		if (aabb1.max.y < aabb2.min.y) {
			return 0;
		}
	}
	//case 5
	if (vel_rel.y == 0) {
		if (aabb1.max.y < aabb2.min.y) {
			return 0;
		}
		else if (aabb1.min.y > aabb2.max.y) {
			return 0;
		}
	}
	//case 6
	if (t_first > t_last) {
		return 0;
	}



	return true;
	/*
	Implement the collision intersection over here.

	The steps are:
	Step 1: Check for static collision detection between rectangles (static: before moving).
				If the check returns no overlap, you continue with the dynamic collision test
					with the following next steps 2 to 5 (dynamic: with velocities).
				Otherwise you return collision is true, and you stop.

	Step 2: Initialize and calculate the new velocity of Vb
			tFirst = 0  //tFirst variable is commonly used for both the x-axis and y-axis
			tLast = dt  //tLast variable is commonly used for both the x-axis and y-axis

	Step 3: Working with one dimension (x-axis).
			if(Vb < 0)
				case 1
				case 4
			else if(Vb > 0)
				case 2
				case 3
			else //(Vb == 0)
				case 5

			case 6

	Step 4: Repeat step 3 on the y-axis

	Step 5: Return true: the rectangles intersect

	*/

}