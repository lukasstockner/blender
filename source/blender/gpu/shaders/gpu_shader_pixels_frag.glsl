varying vec2 varying_texcoord;

void main()
{
	gl_FragColor = texture2D(b_Sampler2D[0], varying_texcoord)*b_Pixels.scale + b_Pixels.bias;
}