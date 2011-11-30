#include "COM_ChunkOrderHotspot.h"
#include <math.h>

ChunkOrderHotspot::ChunkOrderHotspot(int x, int y, float addition) {
    this->x = x;
    this->y = y;
    this->addition = addition;
}

double ChunkOrderHotspot::determineDistance(int x, int y) {
    int dx = x-this->x;
    int dy = y-this->y;
    double result = sqrt(dx*dx+dy*dy);
    result += this->addition;
    return result;
}
