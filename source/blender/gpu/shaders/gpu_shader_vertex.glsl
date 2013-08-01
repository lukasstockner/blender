
varying vec3 varposition;
varying vec3 varnormal;

void main()
{
	vec4 co = b_ModelViewMatrix * b_Vertex;

	varposition = co.xyz;
	varnormal = normalize(b_NormalMatrix * b_Normal);
	gl_Position = b_ProjectionMatrix * co;

#ifdef GPU_NVIDIA
	// Setting gl_ClipVertex is necessary to get glClipPlane working on NVIDIA
	// graphic cards, while on ATI it can cause a software fallback.
	gl_ClipVertex = b_ModelViewMatrix * b_Vertex;
#endif 

