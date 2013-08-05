/* begin known constants */

#if defined(USE_TEXTURE)
const int b_MaxTextureCoords             = GPU_MAX_COMMON_TEXCOORDS;
const int b_MaxCombinedTextureImageUnits = GPU_MAX_COMMON_SAMPLERS;
#endif

#if defined(USE_LIGHTING)
const int b_MaxLights = GPU_MAX_COMMON_LIGHTS;
#endif

/* end known constants */

