
#include "gpu_glew.h"
#include "gpu_object_gles.h"

#include "GPU_functions.h"
#include "MEM_guardedalloc.h"

#include REAL_GL_MODE
struct GPUGLSL_ES_info *curglslesi = 0;



void gpuVertexPointer_gles(int size, int type, int stride, const void *pointer)
{
	if(curglslesi && (curglslesi->vertexloc!=-1))
	{
		glEnableVertexAttribArray(curglslesi->vertexloc);
		glVertexAttribPointer(curglslesi->vertexloc, size, type, 0, stride, pointer);
	}
}

void gpuNormalPointer_gles(int type, int stride, const void *pointer)
{

	if(curglslesi && (curglslesi->normalloc!=-1))
	{
		glEnableVertexAttribArray(curglslesi->normalloc);
		glVertexAttribPointer(curglslesi->normalloc, 3, type, 0, stride, pointer);
	}
}

void gpuColorPointer_gles (int size, int type, int stride, const void *pointer)
{


	if(curglslesi && (curglslesi->colorloc!=-1))
	{
		glEnableVertexAttribArray(curglslesi->colorloc);
		glVertexAttribPointer(curglslesi->colorloc, size, type, GL_TRUE, stride, pointer);
	}



}

void gpuColorSet_gles(const float *value)
{


	if(curglslesi && (curglslesi->colorloc!=-1))
	{
		glDisableVertexAttribArray(curglslesi->colorloc);
		glVertexAttrib4fv(curglslesi->colorloc, value);
		

	}



}

void gpuTexCoordPointer_gles(int size, int type, int stride, const void *pointer)
{
	if(curglslesi && (curglslesi->texturecoordloc!=-1))
	{
		glEnableVertexAttribArray(curglslesi->texturecoordloc);
		//glDisableVertexAttribArray(curglslesi->texturecoordloc);
		glVertexAttribPointer(curglslesi->texturecoordloc, size, type, 0, stride, pointer);
	}
		if(curglslesi && curglslesi->texidloc!=-1)
			glUniform1i(curglslesi->texidloc, 0);
}


void gpuClientActiveTexture_gles(int texture)
{

}

void gpuCleanupAfterDraw_gles(void)
{


}

void gpu_set_shader_es(struct GPUGLSL_ES_info * s, int update)
{
	curglslesi = s;
//	if(update)
//		GPU_matrix_forced_update();
}


void gpu_assign_gles_loc(GPUGLSL_ES_info * glslesinfo, unsigned int program)
{
		glslesinfo->normalmatloc = gpu_glGetUniformLocation(program, "b_NormalMatrix");	
		glslesinfo->viewmatloc = gpu_glGetUniformLocation(program, "b_ModelViewMatrix");	
		glslesinfo->projectionmatloc = gpu_glGetUniformLocation(program, "b_ProjectionMatrix");
		glslesinfo->texturematloc = gpu_glGetUniformLocation(program, "b_TextureMatrix");
		
		glslesinfo->texidloc = gpu_glGetUniformLocation(program, "v_texid");
		
		glslesinfo->vertexloc = gpu_glGetAttribLocation(program, "b_Vertex");
		glslesinfo->normalloc = gpu_glGetAttribLocation(program, "b_Normal");
		glslesinfo->colorloc = gpu_glGetAttribLocation(program, "b_Color");
		glslesinfo->texturecoordloc = gpu_glGetAttribLocation(program, "b_Coord");
}




GLuint static compile_shader(GLenum type, const char** src, int count)
{
	GLint status;
	GLuint shader = gpu_glCreateShader(type);
	
	
	if(shader == 0)
		return 0;
		
		
	gpu_glShaderSource(shader, count, src, NULL);
	gpu_glCompileShader(shader);
	
	
	gpu_glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	
	if(status == 0)
	{
		GLint len = 0;
		gpu_glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
		
		if(len > 0)
		{
			char * log = MEM_mallocN(len, "GLSLErrLog");
			
			gpu_glGetShaderInfoLog(shader, len, NULL, log);
			printf("Error in generating Objet GLSL: \n %s\n", log);
			
			MEM_freeN(log);
			
			gpu_glGetShaderiv(shader, GL_SHADER_SOURCE_LENGTH, &len);
			
			if(len > 0)
			{
				char* shadersrc = MEM_mallocN(len, "GLSLErrLog");

				//glGetShaderSource(shader, len, rlen, log);
				//printf("Objet GLSL source: \n %s", shadersrc);
				
				MEM_freeN(shadersrc);
				
				
				
					
			}
				
		}
		
		gpu_glDeleteShader(shader);
	
		return 0;
	}
	
	return shader;

}


GLuint static create_program(GLuint vertex, GLuint fragment)
{
	GLint status = 0;
	GLuint program = gpu_glCreateProgram();
	
	if(program == 0)
	{
		printf("Cannot create GLSL program object\n");
		return 0;	
	}
	
	gpu_glAttachShader(program, vertex);
	gpu_glAttachShader(program, fragment);
	
	/* b_Color cannot be 0 because b_Color can be a singular color and glVertexAttrib* won't work */
	glBindAttribLocation(program, 1, "b_Color");
	
	gpu_glLinkProgram(program);
	
	gpu_glGetProgramiv(program, GL_LINK_STATUS, &status);
	
	if(status == 0) 
	{
		GLint len = 0;
		
		gpu_glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
		
		if(len > 0)
		{
			char * log = MEM_mallocN(len, "GLSLErrProgLog");
			
			gpu_glGetProgramInfoLog(program, len, NULL, log);
			printf("Error in generating Program GLSL: \n %s\n", log);
			
			MEM_freeN(log);
				
		}
		
		gpu_glDeleteProgram(program);
		return 0;	
	}

	return program;
}

char * object_shader_vector_basic = 
	"uniform mat4 b_ProjectionMatrix ;	\n"
	"uniform mat4 b_ModelViewMatrix ;	\n"
"attribute vec4 b_Vertex;	\n"
"attribute vec4 b_Color;	\n"
"varying vec4 v_Color;	\n"
"void main()	\n"
"{	\n"
"	gl_Position = b_ProjectionMatrix * b_ModelViewMatrix * b_Vertex;	\n"
"	v_Color = b_Color;	\n"
"}	\n"
;

char * object_shader_fragment_basic = 
"precision mediump float;	\n"
"varying vec4 v_Color;	\n"
"void main()	\n"
"{	\n"
"	gl_FragColor = v_Color;	\n"
"}	\n"
;


char * object_shader_vector_alphatexture = 
	"uniform mat4 b_ProjectionMatrix ;	\n"
	"uniform mat4 b_ModelViewMatrix ;	\n"
	"uniform mat4 b_TextureMatrix ;	\n"
"attribute vec4 b_Vertex;	\n"
"attribute vec4 b_Color;	\n"
"varying vec4 v_Color;	\n"
"attribute vec2 b_Coord;	\n"
"varying vec2 v_Coord;	\n"
"void main()	\n"
"{	\n"
"	gl_Position = b_ProjectionMatrix * b_ModelViewMatrix * b_Vertex;	\n"
"	v_Coord = mat2(b_TextureMatrix) * b_Coord;	\n"
"	v_Color = b_Color;	\n"
"}	\n"
;

char * object_shader_fragment_alphatexture = 
"precision mediump float;	\n"
"varying vec4 v_Color;	\n"
"varying vec2 v_Coord;	\n"
"uniform sampler2D v_texid;	\n"
"void main()	\n"
"{	\n"
"	gl_FragColor = vec4(v_Color.rgb, texture2D(v_texid, v_Coord).a*v_Color.a);	\n"
"}	\n"
;


GPUGLSL_ES_info shader_main_info;
int shader_main;

GPUGLSL_ES_info shader_alphatexture_info;
int shader_alphatexture;



void gpu_object_init_gles(void)
{
	GLuint vo = compile_shader(GL_VERTEX_SHADER, &object_shader_vector_basic, 1);
	GLuint fo = compile_shader(GL_FRAGMENT_SHADER, &object_shader_fragment_basic, 1);	
	shader_main = create_program(vo, fo);
	gpu_assign_gles_loc(&shader_main_info, shader_main);

	vo = compile_shader(GL_VERTEX_SHADER, &object_shader_vector_alphatexture, 1);
	fo = compile_shader(GL_FRAGMENT_SHADER, &object_shader_fragment_alphatexture, 1);
	shader_alphatexture = create_program(vo, fo);	
	
	gpu_assign_gles_loc(&shader_alphatexture_info, shader_alphatexture);
}


