/* begin common uniforms */

uniform mat4 b_ModelViewMatrix;
uniform mat4 b_ProjectionMatrix;
uniform mat4 b_ModelViewProjectionMatrix;


#ifdef USE_TEXTURE_2D

uniform mat4 b_TextureMatrix[b_MaxTextureCoords];

uniform sampler2D b_Sampler2D[b_MaxCombinedTextureImageUnits];

#endif


#ifdef USE_LIGHTING

uniform mat3 b_NormalMatrix;  // transpose of upper 3x3 of b_ModelViewMatrix

uniform mat4 b_ModelViewMatrixInverse;

struct b_MaterialParameters {
    vec4  specular;  // Scm * Scli
    float shininess; // Srm
};

uniform b_MaterialParameters b_FrontMaterial;

struct b_LightSourceParameters {
    vec4  diffuse;              // Dcli
    vec4  specular;             // Scli
    vec4  position;             // Ppli

    vec3  spotDirection;        // Sdli
    float spotExponent;         // Srli
    float spotCutoff;           // Crli
                                // (range: [0.0,90.0], 180.0)

    float spotCosCutoff;        // Derived: cos(Crli)
                                // (range: [1.0,0.0],-1.0)

    float constantAttenuation;  // K0
    float linearAttenuation;    // K1
    float quadraticAttenuation; // K2
};

uniform b_LightSourceParameters b_LightSource[b_MaxLights];

uniform int b_LightCount;

#endif


#ifdef USE_CLIP_PLANES

uniform double b_ClipPlane[b_MaxClipPlanes][4];

uniform int b_ClipPlaneCount;

#endif

/* end common uniforms */