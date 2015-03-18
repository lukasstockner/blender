/* begin common constants */

#ifdef USE_TEXTURE_2D
const int b_MaxTextureCoords             = GPU_MAX_COMMON_TEXCOORDS;
const int b_MaxCombinedTextureImageUnits = GPU_MAX_COMMON_SAMPLERS;
#endif

#ifdef USE_LIGHTING
const int b_MaxLights = GPU_MAX_COMMON_LIGHTS;
#endif

#ifdef USE_CLIP_PLANES
const int b_MaxClipPlanes = GPU_MAX_COMMON_CLIP_PLANES;
#endif

/* end known constants */
