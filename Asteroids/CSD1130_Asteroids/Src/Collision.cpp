/******************************************************************************/
/*!
\file		Collision.cpp
\author		Ong Jun Han Benjamin, o.junhanbenjamin, 2301532
\par		o.junhanbenjamin\@digipen.edu
\date		Feb 08, 2024
\brief		This file contains the definition of the function CollisionIntersection_RectRect. The function used to check for collision between two objects.

Copyright (C) 20xx DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
/******************************************************************************/

#include "main.h"

/**************************************************************************/
/*!
	Collision function called to check collision between two instances.
	Compares the min & max x/y coordinates of both instances with each other.
	Should either instances min & max x/y coordinates fall between the other instances x/y coordinates, a collision is detected and will return true,
	else return false.
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

    // AABB
    if ((aabb1.max.x > aabb2.min.x && aabb1.max.y > aabb2.min.y) &&
        (aabb2.max.x > aabb1.min.x && aabb2.max.y > aabb1.min.y))
    {
        return true;
    }

    // Dynamic 
    firstTimeOfCollision = 0; float tLast = g_dt;
    AEVec2 Va = vel1;
    AEVec2 Vb = vel2;
    AEVec2Sub(&Vb, &Vb, &Va); // Storing result back into Vb

    // Checking for x-axis
    if (Vb.x < 0) {
        if (aabb1.min.x > aabb2.max.x) { // Case 1
            return false;
        }
        if (aabb1.max.x < aabb2.min.x) { // Case 4.1
            firstTimeOfCollision = max(((aabb1.max.x - aabb2.min.x) / Vb.x), firstTimeOfCollision);
        }
        if (aabb1.min.x < aabb2.max.x) { // Case 4.2
            tLast = min(((aabb1.min.x - aabb2.max.x) / Vb.x), tLast);
        }
    }
    else if (Vb.x > 0) {
        if (aabb1.min.x > aabb2.max.x) { // Case 2.1
            firstTimeOfCollision = max(((aabb1.min.x - aabb2.max.x) / Vb.x), firstTimeOfCollision);
        }
        if (aabb1.max.x > aabb2.min.x) { // Case 2.2
            tLast = min(((aabb1.max.x - aabb2.min.x) / Vb.x), tLast);
        }
        if (aabb1.max.x < aabb2.min.x) { // Case 3
            return false;
        }
    }
    else { // Vb.x == 0
        if (aabb1.max.x < aabb2.min.x) { // Case 5.1
            return false;
        }
        else if (aabb1.min.x > aabb2.max.x) { // Case 5.2
            return false;
        }
    }

    // Repeating steps above with y-movement
    if (Vb.y < 0) {
        if (aabb1.min.y > aabb2.max.y) { // Case 1
            return false;
        }
        if (aabb1.max.y < aabb2.min.y) { // Case 4.1
            firstTimeOfCollision = max(((aabb1.max.y - aabb2.min.y) / Vb.y), firstTimeOfCollision);
        }
        if (aabb1.min.y < aabb2.max.y) { // Case 4.2
            tLast = min(((aabb1.min.y - aabb2.max.y) / Vb.y), tLast);
        }
    }
    else if (Vb.y > 0) {
        if (aabb1.min.y > aabb2.max.y) { // Case 2.1
            firstTimeOfCollision = max(((aabb1.min.y - aabb2.max.y) / Vb.y), firstTimeOfCollision);
        }
        if (aabb1.max.y > aabb2.min.y) { // Case 2.2
            tLast = min(((aabb1.max.y - aabb2.min.y) / Vb.y), tLast);
        }
        if (aabb1.max.y < aabb2.min.y) { // Case 3
            return false;
        }
    }
    else { // Vb.x == 0
        if (aabb1.max.y < aabb2.min.y) { // Case 5.1
            return false;
        }
        else if (aabb1.min.y > aabb2.max.y) { // Case 5.2
            return false;
        }
    }

    // Return true if the rectangles intersect
    if (firstTimeOfCollision > tLast) { // Case 6
        return false;
    }

    return true;
}