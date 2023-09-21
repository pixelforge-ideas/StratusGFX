
#ifndef STRATUSGFX_RENDERER_H
#define STRATUSGFX_RENDERER_H

#include <string>
#include <vector>
#include <list>
#include <deque>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include "StratusEntity.h"
#include "StratusCommon.h"
#include "StratusCamera.h"
#include "StratusTexture.h"
#include "StratusFrameBuffer.h"
#include "StratusLight.h"
#include "StratusMath.h"
#include "StratusGpuBuffer.h"
#include "StratusGpuCommon.h"
#include "StratusThread.h"
#include "StratusAsync.h"
#include "StratusEntityCommon.h"
#include "StratusEntity.h"
#include "StratusTransformComponent.h"
#include "StratusRenderComponents.h"
#include "StratusGpuMaterialBuffer.h"
#include "StratusGpuCommandBuffer.h"
#include <functional>
#include "StratusStackAllocator.h"
#include <set>
#include "StratusGpuCommandBuffer.h"
#include "StratusTypes.h"
#include "StratusRendererData.h"

namespace stratus {
    class Pipeline;
    class Light;
    class InfiniteLight;
    class Quad;
    struct PostProcessFX;

    typedef std::function<GpuBuffer(GpuCommandBufferPtr&)> CommandBufferSelectionFunction;

    extern bool IsRenderable(const EntityPtr&);
    extern bool IsLightInteracting(const EntityPtr&);
    extern usize GetMeshCount(const EntityPtr&);

    class RendererBackend {
        // Geometry buffer
        struct GBuffer {
            FrameBuffer fbo;
            //Texture position;                 // RGB16F (rgba instead of rgb due to possible alignment issues)
            Texture normals;                  // RGB16F
            Texture albedo;                   // RGB8F
            // Texture baseReflectivity;         // RGB8F
            Texture roughnessMetallicReflectivity; // RGB8F
            Texture structure;                // RGBA16F
            Texture velocity;
            Texture id;
            Texture depth;                    // Default bit depth
            Texture depthPyramid;
        };

        struct PostFXBuffer {
            FrameBuffer fbo;
        };

        struct VirtualPointLightData {
            // For splitting viewport into tiles
            const i32 tileXDivisor = 5;
            const i32 tileYDivisor = 5;
            // This needs to match what is in the vpl tiled deferred shader compute header!
            i32 vplShadowCubeMapX = 32, vplShadowCubeMapY = 32;
            //GpuBuffer vplDiffuseMaps;
            //GpuBuffer vplShadowMaps;
            GpuBuffer shadowDiffuseIndices;
            GpuBuffer vplStage1Results;
            GpuBuffer vplVisiblePerTile;
            GpuBuffer vplData;
            GpuBuffer vplUpdatedData;
            GpuBuffer vplVisibleIndices;
            //GpuBuffer vplNumVisible;
            FrameBuffer vplGIFbo;
            FrameBuffer vplGIDenoisedPrevFrameFbo;
            FrameBuffer vplGIDenoisedFbo1;
            FrameBuffer vplGIDenoisedFbo2;
        };

        struct RenderState {
            i32 numRegularShadowMaps = 200;
            i32 shadowCubeMapX = 256, shadowCubeMapY = 256;
            i32 maxShadowCastingLightsPerFrame = 200; // per frame
            i32 maxTotalRegularLightsPerFrame = 200; // per frame
            GpuBuffer nonShadowCastingPointLights;
            //GpuBuffer shadowCubeMaps;
            GpuBuffer shadowIndices;
            GpuBuffer shadowCastingPointLights;
            VirtualPointLightData vpls;
            // How many shadow maps can be rebuilt each frame
            // Lights are inserted into a queue to prevent any light from being
            // over updated or neglected
            i32 maxShadowUpdatesPerFrame = 3;
            //std::shared_ptr<Camera> camera;
            Pipeline * currentShader = nullptr;
            // Buffer where all color data is written
            GBuffer currentFrame;
            GBuffer previousFrame;
            // Used for culling - based on depth in previous frame's GBuffer
            Texture depthPyramid;
            //std::unique_ptr<Pipeline> depthPyramidCopy;
            std::unique_ptr<Pipeline> depthPyramidConstruct;
            // Buffer for lighting pass
            FrameBuffer lightingFbo;
            Texture lightingColorBuffer;
            // Used for effects like bloom
            //Texture lightingHighBrightnessBuffer;
            Texture lightingDepthBuffer;
            FrameBuffer flatPassFboCurrentFrame;
            FrameBuffer flatPassFboPreviousFrame;
            // Used for Screen Space Ambient Occlusion (SSAO)
            Texture ssaoOffsetLookup;               // 4x4 table where each pixel is (16-bit, 16-bit)
            Texture ssaoOcclusionTexture;
            FrameBuffer ssaoOcclusionBuffer;        // Contains light factors computed per pixel
            Texture ssaoOcclusionBlurredTexture;
            FrameBuffer ssaoOcclusionBlurredBuffer; // Counteracts low sample count of occlusion buffer by depth-aware blurring
            // Used for atmospheric shadowing
            FrameBuffer atmosphericFbo;
            Texture atmosphericTexture;
            Texture atmosphericNoiseTexture;
            // Used for gamma-tonemapping
            PostFXBuffer gammaTonemapFbo;
            // Used for fast approximate anti-aliasing (FXAA)
            PostFXBuffer fxaaFbo1;
            PostFXBuffer fxaaFbo2;
            // Used for temporal anti-aliasing (TAA)
            PostFXBuffer taaFbo;
            // Need to keep track of these to clear them at the end of each frame
            std::vector<GpuArrayBuffer> gpuBuffers;
            // For everything else including bloom post-processing
            i32 numBlurIterations = 10;
            // Might change from frame to frame
            i32 numDownsampleIterations = 0;
            i32 numUpsampleIterations = 0;
            std::vector<PostFXBuffer> gaussianBuffers;
            std::vector<PostFXBuffer> postFxBuffers;
            // Handles atmospheric post processing
            PostFXBuffer atmosphericPostFxBuffer;
            // End of the pipeline should write to this
            FrameBuffer finalScreenBuffer;
            // Used for TAA
            FrameBuffer previousFrameBuffer;
            // Used for a call to glBlendFunc
            GLenum blendSFactor = GL_ONE;
            GLenum blendDFactor = GL_ZERO;
            // Depth prepass
            std::unique_ptr<Pipeline> depthPrepass;
            // Skybox
            std::unique_ptr<Pipeline> skybox;
            std::unique_ptr<Pipeline> skyboxLayered;
            // Postprocessing shader which allows for application
            // of hdr and gamma correction
            std::unique_ptr<Pipeline> gammaTonemap;
            // Preprocessing shader which sets up the scene to allow for dynamic shadows
            std::unique_ptr<Pipeline> shadows;
            std::unique_ptr<Pipeline> vplShadows;
            // Geometry pass - handles all combinations of material properties
            std::unique_ptr<Pipeline> geometry;
            // Forward rendering pass
            std::unique_ptr<Pipeline> forward;
            // Handles first SSAO pass (no blurring)
            std::unique_ptr<Pipeline> ssaoOcclude;
            // Handles second SSAO pass (blurring)
            std::unique_ptr<Pipeline> ssaoBlur;
            // Handles the atmospheric shadowing stage
            std::unique_ptr<Pipeline> atmospheric;
            // Handles atmospheric post fx stage
            std::unique_ptr<Pipeline> atmosphericPostFx;
            // Handles the lighting stage
            std::unique_ptr<Pipeline> lighting;
            std::unique_ptr<Pipeline> lightingWithInfiniteLight;
            // Handles global illuminatino stage
            std::unique_ptr<Pipeline> vplGlobalIllumination;
            std::unique_ptr<Pipeline> vplGlobalIlluminationDenoising;
            // Bloom stage
            std::unique_ptr<Pipeline> bloom;
            // Handles virtual point light culling
            std::unique_ptr<Pipeline> vplCulling;
            std::unique_ptr<Pipeline> vplColoring;
            std::unique_ptr<Pipeline> vplTileDeferredCullingStage1;
            std::unique_ptr<Pipeline> vplTileDeferredCullingStage2;
            // Draws axis-aligned bounding boxes
            std::unique_ptr<Pipeline> aabbDraw;
            // Handles cascading shadow map depth buffer rendering
            // (we compile one depth shader per cascade - max 6)
            std::vector<std::unique_ptr<Pipeline>> csmDepth;
            std::vector<std::unique_ptr<Pipeline>> csmDepthRunAlphaTest;
            // Handles fxaa luminance followed by fxaa smoothing
            std::unique_ptr<Pipeline> fxaaLuminance;
            std::unique_ptr<Pipeline> fxaaSmoothing;
            // Handles temporal anti-aliasing
            std::unique_ptr<Pipeline> taa;
            // Performs full screen pass through
            std::unique_ptr<Pipeline> fullscreen;
            std::unique_ptr<Pipeline> fullscreenPages;
            std::unique_ptr<Pipeline> fullscreenPageGroups;
            std::vector<Pipeline *> shaders;
            // Generic unit cube to render as skybox
            EntityPtr skyboxCube;
            // Generic screen quad so we can render the screen
            // from a separate frame buffer
            EntityPtr screenQuad;
            // Gets around what might be a driver bug...
            TextureHandle dummyCubeMap;
            std::vector<GpuCommandReceiveManagerPtr> dynamicPerPointLightDrawCalls;
            std::vector<GpuCommandReceiveManagerPtr> staticPerPointLightDrawCalls;
            std::unique_ptr<Pipeline> viscullPointLights;
            // Used for virtual shadow maps
            std::unique_ptr<Pipeline> vsmAnalyzeDepth;
            std::unique_ptr<Pipeline> vsmMarkPages;
            std::unique_ptr<Pipeline> vsmMarkScreen;
            std::unique_ptr<Pipeline> vsmFreePages;
            std::unique_ptr<Pipeline> vsmCull;
            std::unique_ptr<Pipeline> vsmClear;
        };

        struct TextureCache {
            std::string file;
            TextureHandle handle = TextureHandle::Null();
            Texture texture;
            /**
             * If true then the file is currently loaded into memory.
             * If false then it has been unloaded, so if anyone tries
             * to use it then it needs to first be re-loaded.
             */
            bool loaded = true;
        };

        struct ShadowMapCache {
            // Framebuffer which wraps around all available cube maps
            std::vector<FrameBuffer> buffers;

            // Lights -> Handles map
            std::unordered_map<LightPtr, GpuAtlasEntry> lightsToShadowMap;

            // Lists all shadow maps which are currently available
            std::list<GpuAtlasEntry> freeShadowMaps;

            // Marks which lights are currently in the cache
            std::list<LightPtr> cachedLights;
        };

        // Contains the cache for regular lights
        ShadowMapCache smapCache_;

        // Contains the cache for virtual point lights
        ShadowMapCache vplSmapCache_;

        /**
         * Contains information about various different settings
         * which will affect final rendering.
         */
        RenderState state_;

        /**
         * Contains all of the shaders that are used by the renderer.
         */
        std::vector<Pipeline *> shaders_;

        /**
         * This encodes the same information as the _textures map, except
         * that it can be indexed by a TextureHandle for fast lookup of
         * texture handles attached to Material objects.
         */
        //mutable std::unordered_map<TextureHandle, TextureCache> _textureHandles;

        // Current frame data used for drawing
        std::shared_ptr<RendererFrame> frame_;

        // Contains some number of Halton sequence values
        GpuBuffer haltonSequence_;

        /**
         * If the renderer was setup properly then this will be marked
         * true.
         */
        bool isValid_ = false;

    public:
        explicit RendererBackend(const u32 width, const u32 height, const std::string&);
        ~RendererBackend();

        /**
         * @return true if the renderer initialized itself properly
         *      and false if any errors occurred
         */
        bool Valid() const;

        const Pipeline * GetCurrentShader() const;

        //void invalidateAllTextures();

        void RecompileShaders();

        /**
         * Attempts to load a model if not already loaded. Be sure to check
         * the returned model's isValid() function.
         */
        // Model loadModel(const std::string & file);

        /**
         * Sets the render mode to be either ORTHOGRAPHIC (2d)
         * or PERSPECTIVE (3d).
         */
        // void setRenderMode(RenderMode mode);

        /**
         * IMPORTANT! This sets up the renderer for a new frame.
         *
         * @param clearScreen if false then renderer will begin
         * drawing without clearing the screen
         */
        void Begin(const std::shared_ptr<RendererFrame>&, bool clearScreen);

        // Takes all state set during Begin and uses it to render the scene
        void RenderScene(const f64);

        /**
         * Finalizes the current scene and displays it.
         */
        void End();

        /**
         * Gets the hierarchical z buffer which can be used for occlusion culling.
         * 
         * Make sure to check the result for null (Texture()) in the event that hiz data is
         * not available.
         */
        Texture GetHiZOcclusionBuffer() const;

        // Returns window events since the last time this was called
        // std::vector<SDL_Event> PollInputEvents();

        // // Returns the mouse status as of the most recent frame
        // RendererMouseState GetMouseState() const;

    private:
        struct VplDistKey_ {
            LightPtr key;
            f64 distance = 0.0;

            VplDistKey_(const LightPtr& key = nullptr, const f64 distance = 0.0)
                : key(key), distance(distance) {}

            bool operator<(const VplDistKey_& other) const {
                return distance < other.distance;
            }
        };

        struct VplDistKeyLess_ {
            bool operator()(const VplDistKey_& left, const VplDistKey_& right) const {
                return left < right;
            }
        };

        // Used for point light sorting and culling
        using VplDistKeyAllocator_ = StackBasedPoolAllocator<VplDistKey_>;
        typedef std::multiset<VplDistKey_, VplDistKeyLess_, VplDistKeyAllocator_> VplDistMultiSet_;
        typedef std::vector<VplDistKey_, VplDistKeyAllocator_> VplDistVector_;

    private:
        void InitializeVplData_();
        void ClearGBuffer_();
        void InitGBuffer_();
        void UpdateWindowDimensions_();
        void ClearFramebufferData_(const bool);
        void InitPointShadowMaps_();
        // void _InitAllEntityMeshData();
        void InitCoreCSMData_(Pipeline *);
        void InitLights_(Pipeline * s, const VplDistVector_& lights, const usize maxShadowLights);
        void InitSSAO_();
        void InitAtmosphericShadowing_();
        void UpdateHiZBuffer_();
        // void _InitEntityMeshData(RendererEntityData &);
        // void _ClearEntityMeshData();
        void ClearRemovedLightData_();
        void BindShader_(Pipeline *);
        void UnbindShader_();
        void PerformPostFxProcessing_();
        void PerformBloomPostFx_();
        void PerformAtmosphericPostFx_();
        void PerformFxaaPostFx_();
        void PerformTaaPostFx_();
        void PerformGammaTonemapPostFx_();
        void FinalizeFrame_();
        void InitializePostFxBuffers_();
        void ProcessCSMVirtualTexture_();
        void RenderBoundingBoxes_(GpuCommandBufferPtr&);
        void RenderBoundingBoxes_(std::unordered_map<RenderFaceCulling, GpuCommandBufferPtr>&);
        void RenderImmediate_(const RenderFaceCulling, GpuCommandBufferPtr&, const CommandBufferSelectionFunction&, usize offset);
        void Render_(Pipeline&, const RenderFaceCulling, GpuCommandBufferPtr&, const CommandBufferSelectionFunction&, usize offset, bool isLightInteracting, bool removeViewTranslation = false);
        void Render_(Pipeline&, std::unordered_map<RenderFaceCulling, GpuCommandBufferPtr>&, const CommandBufferSelectionFunction&, usize offset, bool isLightInteracting, bool removeViewTranslation = false);
        void InitVplFrameData_(const VplDistVector_& perVPLDistToViewer);
        void RenderImmediate_(std::unordered_map<RenderFaceCulling, GpuCommandBufferPtr>&, const CommandBufferSelectionFunction&, usize offset, const bool reverseCullFace);
        void UpdatePointLights_(
            VplDistMultiSet_&, 
            VplDistVector_&,
            VplDistMultiSet_&,
            VplDistVector_&,
            VplDistMultiSet_&,
            VplDistVector_&,
            std::vector<i32, StackBasedPoolAllocator<i32>>& visibleVplIndices
        );
        void PerformVirtualPointLightCullingStage1_(VplDistVector_&, std::vector<i32, StackBasedPoolAllocator<i32>>& visibleVplIndices);
        //void PerformVirtualPointLightCullingStage2_(const std::vector<std::pair<LightPtr, f64>>&, const std::vector<i32>& visibleVplIndices);
        void PerformVirtualPointLightCullingStage2_(const VplDistVector_&);
        void ComputeVirtualPointLightGlobalIllumination_(const VplDistVector_&, const f64);
        void RenderCSMDepth_();
        void RenderQuad_();
        void RenderSkybox_(Pipeline *, const glm::mat4&);
        void RenderSkybox_();
        void RenderForwardPassPbr_();
        void RenderForwardPassFlat_();
        void RenderSsaoOcclude_();
        void RenderSsaoBlur_();
        glm::vec3 CalculateAtmosphericLightPosition_() const;
        void RenderAtmosphericShadowing_();
        ShadowMapCache CreateShadowMap3DCache_(u32 resolutionX, u32 resolutionY, u32 count, bool vpl, const TextureComponentSize&);
        GpuAtlasEntry GetOrAllocateShadowMapForLight_(LightPtr);
        void SetLightShadowMap3D_(LightPtr, GpuAtlasEntry);
        GpuAtlasEntry EvictLightFromShadowMapCache_(LightPtr);
        GpuAtlasEntry EvictOldestLightFromShadowMapCache_(ShadowMapCache&);
        void AddLightToShadowMapCache_(LightPtr);
        void RemoveLightFromShadowMapCache_(LightPtr);
        bool ShadowMapExistsForLight_(LightPtr);
        ShadowMapCache& GetSmapCacheForLight_(LightPtr);
        void RecalculateCascadeData_();
        void ValidateAllShaders_();
        void PerformVSMCulling(
            Pipeline& pipeline,
            const std::function<GpuCommandBufferPtr(const RenderFaceCulling&)>& selectInput,
            const std::function<GpuCommandReceiveBufferPtr(const RenderFaceCulling&)>& selectOutput,
            std::unordered_map<RenderFaceCulling, GpuCommandBufferPtr>& commands
        );
    };
}

#endif //STRATUSGFX_RENDERER_H
