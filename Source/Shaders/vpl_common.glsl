STRATUS_GLSL_VERSION

#include "pbr.glsl"

// These needs to match what is in the renderer backend!
// TODO: Find a better way to sync these values with renderer
#define MAX_TOTAL_VPLS_BEFORE_CULLING (10000)
#define MAX_TOTAL_VPLS_PER_FRAME (MAX_TOTAL_SHADOW_MAPS)
#define MAX_VPLS_PER_TILE (12)

// Needs to use uint for its memory backing since GLSL atomics only work on int and uint
// See for reference:
//  https://codereview.stackexchange.com/questions/258883/glsl-atomic-float-add-for-architectures-with-out-atomic-float-add
//  https://registry.khronos.org/OpenGL-Refpages/gl4/html/atomicCompSwap.xhtml
//  https://registry.khronos.org/OpenGL-Refpages/gl4/html/atomicAdd.xhtml
//  https://registry.khronos.org/OpenGL-Refpages/gl4/html/floatBitsToInt.xhtml
//
// Returns the original contents of mem before the add
//
// See https://community.khronos.org/t/atomiccompswap/69213 for why it needs to be a #define
//
// Type of mem: uint
// Type of data: float
// Type of old: uint
#define ATOMIC_ADD_FLOAT(mem, data, old) {                              \
    uint expected = mem;                                                \
    uint added    = floatBitsToUint(uintBitsToFloat(mem) + data);       \
    uint returned = atomicCompSwap(mem, expected, added);               \
    while (expected != returned) {                                      \
        expected = returned;                                            \
        added    = floatBitsToUint(uintBitsToFloat(expected) + data);   \
        returned = atomicCompSwap(mem, expected, added);                \
    }                                                                   \
    old = returned;                                                     \
}

// Needs to match up with definition in StratusGpuCommon
struct VplStage1PerTileOutputs {
    vec4 averageLocalPosition;
    vec4 averageLocalNormal;
};

struct VplStage2PerTileOutputs {
    int numVisible;
    int indices[MAX_VPLS_PER_TILE];
};

struct VplData {
    vec4 position;
    float radius;
    int visible;
    float placeholder_[2];
};

struct ProbeTextureData {
    samplerCubeArray occlusion;
    samplerCubeArray diffuse;
    samplerCubeArray normals;
    samplerCubeArray properties;
};

layout (std430, binding = 64) readonly buffer probeDataInputBlock {
    ProbeTextureData probeTextures[];
};

layout (r32i) coherent uniform iimage3D probeRayLookupTable;

ivec3 computeProbeIndexFromPositionWithClamping(in ivec3 probeRayLookupDimensions, in vec3 viewPosition, in vec3 worldPos) {
    ivec3 dimensions = probeRayLookupDimensions;

    // We divide dimensions by 2 since we want to store the range [-x/2, +x/2] instead of [0, x]
    vec3 normalization = vec3(dimensions) * 0.5;
    // Divided by 2 since we want each 2 units of space to increase the table index by +1
    vec3 viewSpacePos = (worldPos - viewPosition) * 0.5;
    // Converts from [-1, 1] to [0, 1]
    vec3 tableIndex = (viewSpacePos / normalization + 1.0) * 0.5;

    tableIndex = vec3(
        clamp(tableIndex.x, 0.0, 1.0),
        clamp(tableIndex.y, 0.0, 1.0),
        clamp(tableIndex.z, 0.0, 1.0)
    );

    // For dim of 256 for example, max index is 255 so we subtract 1 from each dimension
    return ivec3(floor(tableIndex * (dimensions - vec3(1.0))));
}

void computeProbeIndexFromPosition(in ivec3 probeRayLookupDimensions, in vec3 viewPosition, in vec3 worldPos, out bool success, out ivec3 index) {
    ivec3 dimensions = probeRayLookupDimensions;

    // We divide dimensions by 2 since we want to store the range [-x/2, +x/2] instead of [0, x]
    vec3 normalization = vec3(dimensions) * 0.5;
    // Divided by 2 since we want each 2 units of space to increase the table index by +1
    vec3 viewSpacePos = (worldPos - viewPosition) * 0.5;
    // Converts from [-1, 1] to [0, 1]
    vec3 tableIndex = (viewSpacePos / normalization + 1.0) * 0.5;

    if (tableIndex.x > 1 || tableIndex.x < 0 || 
        tableIndex.y > 1 || tableIndex.y < 0 || 
        tableIndex.z > 1 || tableIndex.z < 0) {
        
        success = false;
        return;
    }

    // For dim of 256 for example, max index is 255 so we subtract 1 from each dimension
    success = true;
    index = ivec3(floor(tableIndex * (dimensions - vec3(1.0))));
}

//ivec3 computeProbeIndexFromPositionWithClamping(in ivec3 probeRayLookupDimensions, in vec3 viewPosition, in vec3 worldPos) {
//    ivec3 dimensions = probeRayLookupDimensions;
//    ivec3 halfDimensions = dimensions / 2;
//
//    ivec3 coords = ivec3(floor(worldPos)) + halfDimensions;
//
//    dimensions = dimensions - ivec3(1);
//    return ivec3(
//        clamp(coords.x, 0, dimensions.x),
//        clamp(coords.y, 0, dimensions.y),
//        clamp(coords.z, 0, dimensions.z)
//    );
//}
//
//void computeProbeIndexFromPosition(in ivec3 probeRayLookupDimensions, in vec3 viewPosition, in vec3 worldPos, out bool success, out ivec3 index) {
//    ivec3 dimensions = probeRayLookupDimensions;
//    ivec3 halfDimensions = dimensions / 2;
//
//    ivec3 coords = ivec3(floor(worldPos)) + halfDimensions;
//
//    dimensions = dimensions - ivec3(1);
//    if (coords.x > dimensions.x || coords.x < 0 ||
//        coords.y > dimensions.y || coords.y < 0 ||
//        coords.z > dimensions.z || coords.z < 0) {
//
//        success = false;
//        return;
//    }
//
//    success = true;
//    index = coords;
//}

void writeProbeIndexToLookupTable(in ivec3 probeRayLookupDimensions, in ivec3 tableIndex, in int probeIndex) {
    imageAtomicExchange(probeRayLookupTable, tableIndex, probeIndex);
    //imageAtomicOr(probeRayLookupTable, integerTableIndex, 1);
    //imageStore(probeRayLookupTable, tableIndex, ivec4(probeIndex));
}

void writeProbeIndexToLookupTableWithBoundsCheck(in ivec3 probeRayLookupDimensions, in ivec3 tableIndex, in int probeIndex) {
    ivec3 maxIndices = probeRayLookupDimensions - 1;
    if (tableIndex.x < 0 || tableIndex.x > maxIndices.x ||
        tableIndex.y < 0 || tableIndex.y > maxIndices.y ||
        tableIndex.z < 0 || tableIndex.z > maxIndices.z) {
        
        return;
    }

    writeProbeIndexToLookupTable(probeRayLookupDimensions, tableIndex, probeIndex);
}

void writeProbeIndexToLookupTable(in ivec3 probeRayLookupDimensions, in vec3 viewPosition, in vec3 worldPos, in int probeIndex) {
    bool success;
    ivec3 integerTableIndex;
    computeProbeIndexFromPosition(probeRayLookupDimensions, viewPosition, worldPos, success, integerTableIndex);

    if (!success) {
        return;
    }

    writeProbeIndexToLookupTable(probeRayLookupDimensions, integerTableIndex, probeIndex);
}

vec3 probeIndexToWorldPos(in ivec3 probeRayLookupDimensions, in vec3 viewPosition, in ivec3 probeIndex) {
    vec3 dimensions = vec3(probeRayLookupDimensions);
    vec3 maxIndex = dimensions - vec3(1.0);

    // Convert to the range of [-1, 1]
    vec3 normalized = 2.0 * vec3(probeIndex) / maxIndex - 1.0;

    // Convert to world space and account for view position
    return normalized * dimensions + viewPosition;
}