
varying vec3 varco;
varying vec3 varcamco;
varying vec3 varnormal;

void main()
{
	varco = gl_Vertex.xyz;
	varcamco = (gl_ModelViewMatrix * gl_Vertex).xyz;
	varnormal = gl_NormalMatrix * gl_Normal;
	gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * gl_Vertex;

	gl_TexCoord[0] = gl_MultiTexCoord0;

