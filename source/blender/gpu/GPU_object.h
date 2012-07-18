typedef struct GPU_object_func
{
	void (*gpuVertexPointer)(int size, int type, int stride, const void *pointer);
	void (*gpuNormalPointer)(          int type, int stride, const void *pointer);
	void (*gpuColorPointer )(int size, int type, int stride, const void *pointer);	
} GPU_object_func;



#ifdef __cplusplus
extern "C" {
#endif

extern GPU_object_func gpugameobj;

void GPU_init_object_func(void);

#ifdef __cplusplus
}
#endif