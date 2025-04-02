#include "playerdata.h"

std::unordered_map<std::string, playerData> players;
int aestroidID = 0; // Add a global variable to keep track of the aestroid ID

void allAestroids::setAestroids(int numAestroids_in)
{
	numAestroids = numAestroids_in;//update the number of aestroids number
    for (int i = 0; i < numAestroids; i++)
    {
        aestroids a;
        a.id = aestroidID++; // Increment the aestroid ID
        a.x = rand() % 800;
        a.y = rand() % 600;
        a.rot = rand() % 360;
        aestroid.push_back(a);
    }
}

void allAestroids::applyMovementVector(float dx, float dy)
{
    for (int i = 0; i < numAestroids; i++)
    {
        aestroid[i].x += dx;
        aestroid[i].y += dy;
        aestroid[i].rot = rand() % 360;
    }
}
