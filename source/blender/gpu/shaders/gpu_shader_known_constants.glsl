/* begin known constants */

#if defined(USE_TEXTURE)
const int b_MaxTextureCoords = 1;
#endif

#if defined(USE_TEXTURE)
const int b_MaxTextureCoords             = GPU_MAX_KNOWN_TEXCOORDS;
const int b_MaxCombinedTextureImageUnits = GPU_MAX_KNOWN_SAMPLERS;
#endif

#if defined(USE_LIGHTING)
const int b_MaxLights        = 8;
#endif

/* end known constants */

