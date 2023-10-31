// Performs basic culling using combination of virtual screen bounds and hierarchical
// page buffer to quickly determine if an object overlaps any valid, uncached pages
//
// Results are written to a draw indirect buffer

STRATUS_GLSL_VERSION

#extension GL_ARB_bindless_texture : require
#extension GL_ARB_sparse_texture2 : require

precision highp float;
precision highp int;
precision highp uimage2D;
precision highp sampler2D;
precision highp sampler2DArrayShadow;

layout (local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

#include "common.glsl"
#include "aabb.glsl"
#include "vsm_common.glsl"

// Hierarchical page buffer
uniform sampler2DArray hpb;

uniform uint frameCount;
uniform uint numDrawCalls;
uniform uint maxDrawCommands;

uniform uint numPageGroupsX;
uniform uint numPageGroupsY;
uniform uint numPagesXY;
uniform uint numPixelsXY;

layout (std430, binding = CURR_FRAME_MODEL_MATRICES_BINDING_POINT) readonly buffer inputBlock2 {
    mat4 modelTransforms[];
};

layout (std430, binding = AABB_BINDING_POINT) readonly buffer inputBlock4 {
    AABB aabbs[];
};

layout (std430, binding = VISCULL_VSM_IN_DRAW_CALLS_BINDING_POINT) readonly buffer inputBlock1 {
    DrawElementsIndirectCommand inDrawCalls[];
};

layout (std430, binding = VISCULL_VSM_OUT_DRAW_CALLS_BINDING_POINT) coherent buffer outputBlock1 {
    DrawElementsIndirectCommand outDrawCalls[];
};

layout (std430, binding = VSM_PAGE_BOUNDING_BOX_BINDING_POINT) readonly buffer inputBlock5 {
    ClipMapBoundingBox clipMapBoundingBoxes[];
};

shared ivec2 pageGroupCorners[4];
shared vec2 pageGroupCornersTexCoords[4];
// shared uint pageGroupIsValid;
// shared uint atLeastOneResidentPage;

// #define IS_PAGE_WITHIN_BOUNDS(type)                                                                 \
//     bool isPageGroupWithinBounds(in type pageCorner, in type texCornerMin, in type texCornerMax) {  \
//         return pageCorner.x >= texCornerMin.x && pageCorner.x <= texCornerMax.x &&                  \
//             pageCorner.y >= texCornerMin.y && pageCorner.y <= texCornerMax.y;                       \
//     }

void main() {

    ivec2 basePageGroup = ivec2(gl_WorkGroupID.xy);
    uint basePageGroupIndex = basePageGroup.x + basePageGroup.y * numPageGroupsX;

    ivec2 residencyTableSize = ivec2(numPagesXY, numPagesXY);
    ivec2 texelsPerPage = ivec2(numPixelsXY) / residencyTableSize;
    ivec2 maxResidencyTableIndex = residencyTableSize - ivec2(1);
    ivec2 pagesPerPageGroup = residencyTableSize / ivec2(numPageGroupsX, numPageGroupsY);
    ivec2 maxPageGroupIndex = ivec2(numPageGroupsX, numPageGroupsY) - ivec2(1);

    vec4 corners[8];
    vec2 texCorners[4];
    vec2 texCoords;
    vec2 currentTileCoords;
    vec2 currentTileCoordsLower;
    vec2 currentTileCoordsUpper;

    for (int cascade = 0; cascade < vsmNumCascades; ++cascade) {
        if (clipMapBoundingBoxes[cascade].minPageX > clipMapBoundingBoxes[cascade].maxPageX || 
            clipMapBoundingBoxes[cascade].minPageY > clipMapBoundingBoxes[cascade].maxPageY) {
            
            continue;
        }

        if (gl_LocalInvocationID == 0) {
            pageGroupCorners[0] = ivec2(clipMapBoundingBoxes[cascade].minPageX, clipMapBoundingBoxes[cascade].minPageY);
            pageGroupCorners[1] = ivec2(clipMapBoundingBoxes[cascade].minPageX, clipMapBoundingBoxes[cascade].maxPageY);
            pageGroupCorners[2] = ivec2(clipMapBoundingBoxes[cascade].maxPageX, clipMapBoundingBoxes[cascade].minPageY);
            pageGroupCorners[3] = ivec2(clipMapBoundingBoxes[cascade].maxPageX, clipMapBoundingBoxes[cascade].maxPageY);

            pageGroupCornersTexCoords[0] = (vec2(2 * pageGroupCorners[0]) / vec2(maxResidencyTableIndex) - 1.0) * 0.5 + 0.5;
            pageGroupCornersTexCoords[1] = (vec2(2 * pageGroupCorners[1]) / vec2(maxResidencyTableIndex) - 1.0) * 0.5 + 0.5;
            pageGroupCornersTexCoords[2] = (vec2(2 * pageGroupCorners[2]) / vec2(maxResidencyTableIndex) - 1.0) * 0.5 + 0.5;
            pageGroupCornersTexCoords[3] = (vec2(2 * pageGroupCorners[3]) / vec2(maxResidencyTableIndex) - 1.0) * 0.5 + 0.5;
        }

        barrier();

        // Invalid page group (all pages uncommitted)
        // if (atLeastOneResidentPage == 0 || atLeastOneResidentPage == 0) {
        //     return;
        // }

        // Defines local work group from layout local size tag above
        uint localWorkGroupSize = gl_WorkGroupSize.x * gl_WorkGroupSize.y;

        for (uint drawIndex = gl_LocalInvocationIndex; drawIndex < numDrawCalls; drawIndex += localWorkGroupSize) {
            DrawElementsIndirectCommand draw = inDrawCalls[drawIndex];
            // if (draw.instanceCount == 0) {
            //     continue;
            // }

            //AABB aabb = transformAabbAsNDCCoords(aabbs[drawIndex], cascadeProjectionView * modelTransforms[drawIndex]);
            AABB aabb = vsmTransformAabbAsNDCCoords(aabbs[drawIndex], vsmClipMap0ProjectionView * modelTransforms[drawIndex], corners, cascade);

            vec2 pageMin = vec2(pageGroupCorners[0]);
            vec2 pageMax = vec2(pageGroupCorners[3]);

            //vec2 aabbMin = corners[0].xy * vec2(maxResidencyTableIndex);
            //vec2 aabbMax = corners[7].xy * vec2(maxResidencyTableIndex);
            vec2 aabbMin = (aabb.vmin.xy * 0.5 + 0.5) * vec2(maxResidencyTableIndex);
            vec2 aabbMax = (aabb.vmax.xy * 0.5 + 0.5) * vec2(maxResidencyTableIndex);

            // Even if our page group is inactive we still need to record commands just in case
            // our inactivity is due to being fully cached (the CPU may clear some/all of our region
            // due to its conservative algorithm)
            if (isOverlapping(pageMin, pageMax, aabbMin, aabbMax)) {
                // draw.instanceCount = 1;

                // We overlap active region - now narrow it down more by using HPB culling
                //
                // First add 2 page border
                aabbMin = aabbMin - vec2(2);
                aabbMax = aabbMax + vec2(2);
                aabbMin /= vec2(maxResidencyTableIndex);
                aabbMax /= vec2(maxResidencyTableIndex);
                aabbMin  = clamp(aabbMin, vec2(0.0), vec2(1.0));
                aabbMax  = clamp(aabbMax, vec2(0.0), vec2(1.0));

                float width  = (aabbMax.x - aabbMin.x) * float(maxResidencyTableIndex);
                float height = (aabbMax.y - aabbMin.y) * float(maxResidencyTableIndex);
                int level    = int(ceil(log2(max(1, max(width, height)))));

                float pow2Val    = float(BITMASK_POW2(level));
                float resolution = float(maxResidencyTableIndex) / pow2Val;

                ivec3 coord1 = ivec3(ivec2(vec2(aabbMin.x, aabbMin.y) * resolution), cascade);
                ivec3 coord2 = ivec3(ivec2(vec2(aabbMin.x, aabbMax.y) * resolution), cascade);
                ivec3 coord3 = ivec3(ivec2(vec2(aabbMax.x, aabbMin.y) * resolution), cascade);
                ivec3 coord4 = ivec3(ivec2(vec2(aabbMax.x, aabbMax.y) * resolution), cascade);

                float pageStatus1 = texelFetch(hpb, coord1, level).r;
                float pageStatus2 = texelFetch(hpb, coord2, level).r;
                float pageStatus3 = texelFetch(hpb, coord3, level).r;
                float pageStatus4 = texelFetch(hpb, coord4, level).r;

                float pageStatusMerged = max(max(pageStatus1, pageStatus2), max(pageStatus3, pageStatus4));

                if (pageStatusMerged > 0) {
                    draw.instanceCount = 1;
                }
                else {
                    draw.instanceCount = 0;
                }
            }
            else {
                draw.instanceCount = 0;
            }

            outDrawCalls[drawIndex + cascade * maxDrawCommands] = draw;
        }
        
        barrier();
    }
}