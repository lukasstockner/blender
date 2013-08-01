/* begin known uniforms */

uniform mat4 b_ModelViewMatrix;
uniform mat4 b_ProjectionMatrix;
uniform mat4 b_ModelViewProjectionMatrix;

#if defined(USE_TEXTURE)
uniform mat4 b_TextureMatrix[b_MaxTextureCoords];
#endif

#if defined(USE_LIGHTING)
uniform mat4 b_NormalMatrix;  // transpose of upper 3x3 of b_ModelViewMatrix

uniform mat4 b_ModelViewMatrixInverse;

struct gl_MaterialParameters {
    vec4  diffuse;   // Dcm * Dcli
    vec4  specular;  // Scm * Scli
    float shininess; // Srm
};

uniform gl_MaterialParameters gl_FrontMaterial;

struct b_LightSourceParameters {
    vec4  diffuse;              // Dcli
    vec4  specular;             // Scli
    vec4  position;             // Ppli

    vec3  spotDirection;        // Sdli
    float spotExponent;         // Srli
    float spotCutoff;           // Crli

    float constantAttenuation;  // K0
    float linearAttenuation;    // K1
    float quadraticAttenuation; // K2
};

uniform b_LightSourceParameters b_LightSource[b_MaxLights];

uniform int b_LightCount;
#endif

/* end known uniforms */

