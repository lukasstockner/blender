varying vec2 varying_texcoord;

void main()
{
	varying_texcoord = b_MultiTexCoord0.st;

	//gl_Position = b_ModelViewProjectionMatrix * b_Vertex;
	gl_Position = b_ProjectionMatrix * b_ModelViewMatrix * b_Vertex;
}