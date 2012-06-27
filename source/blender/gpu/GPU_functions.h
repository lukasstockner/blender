

#ifdef GPU_INTERN_FUNC
#define GPUFUNC
#else
#define GPUFUNC extern
#endif 


GPUFUNC unsigned int (*gpuCreateShader)(unsigned int shaderType);
GPUFUNC void (*gpuAttachShader)(	unsigned int program, unsigned int shader);
GPUFUNC void (*gpuShaderSource)(unsigned int shader, int count, const char ** string, const int * length);
GPUFUNC void (*gpuCompileShader)(unsigned int shader);
GPUFUNC void (*gpuGetShaderiv)(unsigned int shader, unsigned int pname, int *params);
GPUFUNC void (*gpuGetShaderInfoLog)(unsigned int shader, int maxLength, int *length, char *infoLog);

GPUFUNC unsigned int (*gpuCreateProgram)(void);
GPUFUNC void (*gpuLinkProgram)(unsigned int program);
GPUFUNC void (*gpuGetProgramiv)(unsigned int shader, unsigned int pname, int *params);
GPUFUNC void (*gpuGetProgramInfoLog)(unsigned int shader, int maxLength, int *length, char *infoLog);


GPUFUNC void (*gpuUniform1i)(int location, int v0);

GPUFUNC void (*gpuUniform1fv)(int location, int count, const float * value);
GPUFUNC void (*gpuUniform2fv)(int location, int count, const float * value);
GPUFUNC void (*gpuUniform3fv)(int location, int count, const float * value);
GPUFUNC void (*gpuUniform4fv)(int location, int count, const float * value);
GPUFUNC void (*gpuUniformMatrix3fv)(int location, int count, unsigned char transpose, const float * value);
GPUFUNC void (*gpuUniformMatrix4fv)(int location, int count, unsigned char transpose, const float * value);

GPUFUNC int (*gpuGetAttribLocation)(unsigned int program, const char *name);
GPUFUNC int (*gpuGetUniformLocation)(unsigned int program, const char * name);


GPUFUNC void (*gpuUseProgram)(unsigned int program);
GPUFUNC void (*gpuDeleteShader)(unsigned int shader);
GPUFUNC void (*gpuDeleteProgram)(unsigned int program);



GPUFUNC void (*gpuGenFramebuffers)(int m, unsigned int * ids);
GPUFUNC void (*gpuBindFramebuffer)(unsigned int target, unsigned int framebuffer);
GPUFUNC void (*gpuDeleteFramebuffers)(int n, const unsigned int * framebuffers);






#ifdef __cplusplus
extern "C" { 
#endif

void GPU_func_comp_init(void);

#ifdef __cplusplus
}
#endif
