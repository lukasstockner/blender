#ifndef _COM_SocketReader_h
#define _COM_SocketReader_h
#include "BLI_rect.h"
#include "COM_defines.h"

class MemoryBuffer;
/**
  * @brief Helper class for reading socket data.
  * Only use this class for dispatching (un-ary and n-ary) executions.
  * @ingroup Execution
  */
class SocketReader {
private:
protected:
	/**
	  * @brief Holds the width of the output of this operation.
	  */
	unsigned int width;

	/**
	  * @brief Holds the height of the output of this operation.
	  */
	unsigned int height;


	/**
	  * @brief calculate a single pixel
	  * @note this method is called for non-complex
	  * @param result is a float[4] array to store the result
	   * @param x the x-coordinate of the pixel to calculate in image space
	   * @param y the y-coordinate of the pixel to calculate in image space
	   * @param inputBuffers chunks that can be read by their ReadBufferOperation.
	  */
	virtual void executePixel(float* result, float x, float y, MemoryBuffer *inputBuffers[]) {}

	/**
	  * @brief calculate a single pixel
	  * @note this method is called for complex
	  * @param result is a float[4] array to store the result
	   * @param x the x-coordinate of the pixel to calculate in image space
	   * @param y the y-coordinate of the pixel to calculate in image space
	* @param inputBuffers chunks that can be read by their ReadBufferOperation.
	* @param chunkData chunk specific data a during execution time.
	  */
	virtual void executePixel(float* result, int x, int y, MemoryBuffer *inputBuffers[], void* chunkData) {
		executePixel(result, x, y, inputBuffers);
	}

public:
	inline void read(float* result, float x, float y, MemoryBuffer *inputBuffers[]) {
		executePixel(result, x, y, inputBuffers);
	}
	inline void read(float* result, float x, float y, MemoryBuffer *inputBuffers[], void* chunkData) {
		executePixel(result, x, y, inputBuffers, chunkData);
	}

    virtual void* initializeTileData(rcti *rect, MemoryBuffer** memoryBuffers) {
		return 0;
	}
    virtual void deinitializeTileData(rcti *rect, MemoryBuffer** memoryBuffers, void* data) {
	}
	
	virtual MemoryBuffer* getInputMemoryBuffer(MemoryBuffer** memoryBuffers) {return 0;}


	inline const unsigned int getWidth() const {return this->width;}
	inline const unsigned int getHeight() const {return this->height;}
};

#endif
