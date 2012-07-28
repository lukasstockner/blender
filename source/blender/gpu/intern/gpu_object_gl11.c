 
#ifndef GLES

#include "GL/glew.h"



struct GPU_object_gl11_data
{
		char norma;
		char cola;	char texta;

} static od = {0};



void gpuVertexPointer_gl11(int size, int type, int stride, const void *pointer)
{
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(size, type, stride, pointer);
}

void gpuNormalPointer_gl11(int type, int stride, const void *pointer)
{
	glEnableClientState(GL_NORMAL_ARRAY); od.norma = 1;
	glNormalPointer(type, stride, pointer);

}

void gpuColorPointer_gl11 (int size, int type, int stride, const void *pointer)
{

	glEnableClientState(GL_COLOR_ARRAY); od.cola = 1;
	glColorPointer(size, type, stride, pointer);



}

void gpuTexCoordPointer_gl11(int size, int type, int stride, const void *pointer)
{
	if(od.texta==0)
	{
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		od.texta = 1;

	}
	glTexCoordPointer(size, type, stride, pointer);

}

void gpuClientActiveTexture_gl11(int texture)
{
	glClientActiveTexture(texture);
}

void gpuCleanupAfterDraw_gl11(void)
{
	glDisableClientState(GL_VERTEX_ARRAY);

	if(od.norma)
		glDisableClientState(GL_NORMAL_ARRAY);
	if(od.cola)
		glDisableClientState(GL_COLOR_ARRAY);
	if(od.texta)
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	od.norma = 0;
	od.cola = 0;
	od.texta = 0;
}

#endif
