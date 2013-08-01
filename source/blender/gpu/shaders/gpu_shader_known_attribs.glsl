/* begin known attributes */

attribute vec4 b_Vertex;

#if !defined(USE_MATERIAL_COLOR)
attribute vec4 b_Color;
#endif

#if defined(USE_LIGHTING)
attribute vec4 b_Normal;
#endif

#if defined(USE_TEXTURE)
attribute vec4 b_MultiTexCoord[b_MaxTexCoords];
#endif

/* end known attributes */

