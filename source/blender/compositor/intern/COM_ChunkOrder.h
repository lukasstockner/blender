#ifndef _COM_ChunkOrder_h_
#define _COM_ChunkOrder_h_

#include "COM_ChunkOrderHotspot.h"
class ChunkOrder {
private:
	unsigned int number;
    int x;
    int y;
    double distance;
public:
	ChunkOrder();
	void determineDistance(ChunkOrderHotspot **hotspots, unsigned int numberOfHotspots);
	friend bool operator<(const ChunkOrder& a, const ChunkOrder& b);

	void setChunkNumber(unsigned int chunknumber) {this->number = chunknumber;}
    void setX(int x) {this->x = x;}
    void setY(int y) {this->y = y;}
	unsigned int getChunkNumber() {return this->number;}
    double getDistance() {return this->distance;}


};

#endif
