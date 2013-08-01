void main()
{
	varying_texcoord = mat2(b_TextureMatrix) * b_TexCoord;
	varying_color    = b_Color;

	gl_Position = b_ProjectionMatrix * b_ModelViewMatrix * b_Vertex;
}
