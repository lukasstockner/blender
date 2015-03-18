varying vec4 varying_color;
varying vec2 varying_texcoord;

void main()
{
	varying_texcoord = (b_TextureMatrix[0] * b_MultiTexCoord0).st;
	varying_color    = b_Color;

	gl_Position = b_ModelViewProjectionMatrix * b_Vertex;
}