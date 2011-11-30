#ifndef _COM_ChunkOrderHotSpot_h_
#define _COM_ChunkOrderHotSpot_h_

class ChunkOrderHotspot {
private:
    int x;
    int y;
    float addition;

public:
	ChunkOrderHotspot(int x, int y, float addition);
    double determineDistance(int x, int y);
};

#endif
