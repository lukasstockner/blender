

#ifdef __cplusplus 
extern "C" {
#endif

typedef struct GPUGLSL_ES_info
{
		int viewmatloc;
		int normalmatloc;
		int projectionmatloc;
		int texturematloc;
	
		int texidloc;

		int vertexloc;
		int normalloc;	
		int colorloc;
		int texturecoordloc;
	

} GPUGLSL_ES_info;

extern struct GPUGLSL_ES_info *curglslesi;

void gpu_assign_gles_loc(struct GPUGLSL_ES_info * glslesinfo, unsigned int program);

void gpu_set_shader_es(struct GPUGLSL_ES_info * s, int update);

void gpuVertexPointer_gles(int size, int type, int stride, const void *pointer);
void gpuNormalPointer_gles(          int type, int stride, const void *pointer);
void gpuColorPointer_gles (int size, int type, int stride, const void *pointer);
void gpuTexCoordPointer_gles(int size, int type, int stride, const void *pointer);
void gpuClientActiveTexture_gles(int texture);
void gpuColorSet_gles(const float *value);

void gpuCleanupAfterDraw_gles(void);

extern GPUGLSL_ES_info shader_main_info;
extern int shader_main;


extern GPUGLSL_ES_info shader_alphatexture_info;
extern int shader_alphatexture;


void gpu_object_init_gles(void);

#ifdef __cplusplus 
}
#endif
