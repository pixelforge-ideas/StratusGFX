STRATUS_GLSL_VERSION

#extension GL_ARB_bindless_texture : require

#include "mesh_data.glsl"

layout (std430, binding = CURR_FRAME_MODEL_MATRICES_BINDING_POINT) readonly buffer SSBO3 {
    mat4 modelMatrices[];
};

uniform mat4 projectionView;

void main() {
    gl_Position = projectionView * modelMatrices[gl_DrawID] * vec4(getPosition(gl_VertexID), 1.0);
}