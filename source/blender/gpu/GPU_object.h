typedef struct GPU_object_func
{
	void (*gpuVertexPointer)(int size, int type, int stride, const void *pointer);
	void (*gpuNormalPointer)(          int type, int stride, const void *pointer);
	void (*gpuColorPointer )(int size, int type, int stride, const void *pointer);	





} GPU_object_func;

extern GPU_object_func gpugameobj;


#ifdef __cplusplus
extern "C" {
#endif

void GPU_init_object_func(void);

#ifdef __cplusplus
}
#endif