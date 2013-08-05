/* begin common constants */

#ifdef USE_TEXTURE_2D
const int b_MaxTextureCoords             = GPU_MAX_COMMON_TEXCOORDS;
const int b_MaxCombinedTextureImageUnits = GPU_MAX_COMMON_SAMPLERS;
#endif

#ifdef USE_LIGHTING
const int b_MaxLights = GPU_MAX_COMMON_LIGHTS;
#endif

/* end known constants */

