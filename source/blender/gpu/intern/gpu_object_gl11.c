 
#ifndef GLES

#include "GL/glew.h"





void gpuVertexPointer_gl11(int size, int type, int stride, const void *pointer)
{
		glVertexPointer(size, type, stride, pointer);
}

void gpuNormalPointer_gl11(int type, int stride, const void *pointer)
{
		glNormalPointer(type, stride, pointer);

}

void gpuColorPointer_gl11 (int size, int type, int stride, const void *pointer)
{






}

#endif
