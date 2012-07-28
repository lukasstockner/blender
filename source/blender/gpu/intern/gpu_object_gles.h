#ifdef GLES

#ifdef __cplusplus 
extern "C" {
#endif

typedef struct GPUGLSL_ES_info
{
		int viewmatloc;
		int normalmatloc;
		int projectionmatloc;
	

		int vertexloc;
		int normalloc;	
	

} GPUGLSL_ES_info;

extern struct GPUGLSL_ES_info *curglslesi;

void gpu_assign_gles_loc(struct GPUGLSL_ES_info * glslesinfo, unsigned int program);

void gpu_set_shader_es(struct GPUGLSL_ES_info * s, int update);

void gpuVertexPointer_gles(int size, int type, int stride, const void *pointer);
void gpuNormalPointer_gles(          int type, int stride, const void *pointer);
void gpuColorPointer_gles (int size, int type, int stride, const void *pointer);
void gpuTexCoordPointer_gles(int size, int type, int stride, const void *pointer);
void gpuClientActiveTexture_gles(int texture);

void gpuCleanupAfterDraw_gles(void);


#ifdef __cplusplus 
}
#endif
#endif
