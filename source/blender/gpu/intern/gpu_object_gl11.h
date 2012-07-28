#ifndef GLES

void gpuVertexPointer_gl11(int size, int type, int stride, const void *pointer);
void gpuNormalPointer_gl11(          int type, int stride, const void *pointer);
void gpuColorPointer_gl11 (int size, int type, int stride, const void *pointer);	 
void gpuTexCoordPointer_gl11(int size, int type, int stride, const void *pointer);
void gpuClientActiveTexture_gl11(int texture);

void gpuCleanupAfterDraw_gl11(void);

#endif
