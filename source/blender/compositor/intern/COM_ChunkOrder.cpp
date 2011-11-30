#include "COM_ChunkOrder.h"
#include "BLI_math.h"

ChunkOrder::ChunkOrder() {
    this->distance = 0.0;
	this->number = 0;
    this->x = 0;
    this->y = 0;
}

void ChunkOrder::determineDistance(ChunkOrderHotspot **hotspots, unsigned int numberOfHotspots) {
    unsigned int index;
    double distance = MAXFLOAT;
    for (index = 0 ; index < numberOfHotspots ; index ++) {
		ChunkOrderHotspot* hotspot = hotspots[index];
        double ndistance = hotspot->determineDistance(this->x, this->y);
        if (ndistance < distance) {
            distance = ndistance;
        }
    }
    this->distance = distance;
}

bool operator<(const ChunkOrder& a, const ChunkOrder& b) {
    return a.distance < b.distance;
}
