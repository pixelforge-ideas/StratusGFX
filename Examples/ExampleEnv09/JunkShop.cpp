#include "StratusCommon.h"
#include "glm/glm.hpp"
#include <iostream>
#include <StratusPipeline.h>
#include <StratusCamera.h>
#include "StratusAsync.h"
#include <chrono>
#include "StratusEngine.h"
#include "StratusResourceManager.h"
#include "StratusLog.h"
#include "StratusRendererFrontend.h"
#include "StratusWindow.h"
#include <StratusLight.h>
#include <StratusUtils.h>
#include <memory>
#include <filesystem>
#include "CameraController.h"
#include "WorldLightController.h"
#include "LightComponents.h"
#include "LightControllers.h"
#include "StratusTransformComponent.h"
#include "StratusGpuCommon.h"
#include "WorldLightController.h"
#include "FrameRateController.h"
#include "StratusWindow.h"

class JunkShop : public stratus::Application {
public:
    virtual ~JunkShop() = default;

    const char * GetAppName() const override {
        return "JunkShop";
    }

    // Perform first-time initialization - true if success, false otherwise
    virtual bool Initialize() override {
        STRATUS_LOG << "Initializing " << GetAppName() << std::endl;

        //INSTANCE(Window)->SetWindowDims(1920, 1080);

        LightCreator::Initialize();

        stratus::InputHandlerPtr controller(new CameraController());
        INSTANCE(InputManager)->AddInputHandler(controller);

        const glm::vec3 warmMorningColor = glm::vec3(254.0f / 255.0f, 232.0f / 255.0f, 176.0f / 255.0f);
        const glm::vec3 defaultSunColor = glm::vec3(1.0f);
        const glm::vec3 moonlightColor = glm::vec3(79.0f / 255.0f, 105.0f / 255.0f, 136.0f / 255.0f);
        //auto wc = new WorldLightController(defaultSunColor, warmMorningColor, 5);
        auto wc = new WorldLightController(moonlightColor, moonlightColor, 3.0f);
        wc->SetRotation(stratus::Rotation(stratus::Degrees(56.8385f), stratus::Degrees(10.0f), stratus::Degrees(0)));
        controller = stratus::InputHandlerPtr(wc);
        INSTANCE(InputManager)->AddInputHandler(controller);

        controller = stratus::InputHandlerPtr(new FrameRateController());
        INSTANCE(InputManager)->AddInputHandler(controller);

        // Moonlight
        //worldLight->setColor(glm::vec3(80.0f / 255.0f, 104.0f / 255.0f, 134.0f / 255.0f));
        //worldLight->setIntensity(0.5f);

        //INSTANCE(RendererFrontend)->SetAtmosphericShadowing(0.2f, 0.3f);

        // Disable culling for this model since there are some weird parts that seem to be reversed
        //stratus::Async<stratus::Entity> e = stratus::ResourceManager::Instance()->LoadModel("../Resources/glTF-Sample-Models/2.0/JunkShop/glTF/JunkShop.gltf", stratus::ColorSpace::SRGB, true, stratus::RenderFaceCulling::CULLING_CCW);
        stratus::Async<stratus::Entity> e = stratus::ResourceManager::Instance()->LoadModel("../Resources/JunkShop.glb", stratus::ColorSpace::SRGB, true, stratus::RenderFaceCulling::CULLING_CCW);
        requested.push_back(e);
        
        auto callback = [this](stratus::Async<stratus::Entity> e) { 
            if (e.Failed()) return;
            //STRATUS_LOG << "Adding\n";
            received.push_back(e.GetPtr());
            auto transform = stratus::GetComponent<stratus::LocalTransformComponent>(e.GetPtr());
            //transform->SetLocalPosition(glm::vec3(0.0f));
            //transform->SetLocalScale(glm::vec3(15.0f));
            transform->SetLocalScale(glm::vec3(10.0f));
            transform->SetLocalRotation(stratus::Rotation(stratus::Degrees(0.0f), stratus::Degrees(90.0f), stratus::Degrees(0.0f)));  
            INSTANCE(EntityManager)->AddEntity(e.GetPtr());
            //INSTANCE(RendererFrontend)->AddDynamicEntity(sponza);
        };

        e.AddCallback(callback);

        auto settings = INSTANCE(RendererFrontend)->GetSettings();
        settings.skybox = stratus::ResourceManager::Instance()->LoadCubeMap("../Resources/Skyboxes/learnopengl/sbox_", stratus::ColorSpace::NONE, "jpg");
        settings.SetSkyboxIntensity(0.25f);
        settings.SetSkyboxColorMask(moonlightColor);
        settings.SetEmissionStrength(5.0f);
        INSTANCE(RendererFrontend)->SetSettings(settings);

        INSTANCE(RendererFrontend)->GetWorldLight()->SetAlphaTest(true);

        LightCreator::CreateStationaryLight(
            LightParams(glm::vec3(31.2952, 38.3336, -89.1543), glm::vec3(1, 1, 1), 600, true),
            false
        );
        LightCreator::CreateStationaryLight(
            LightParams(glm::vec3(71.6399, 30.0002, -62.7616), glm::vec3(1, 1, 1), 600, true),
            false
        );
        LightCreator::CreateStationaryLight(
            LightParams(glm::vec3(62.0486, 11.6668, -71.4374), glm::vec3(1, 1, 1), 600, true),
            false
        );
        LightCreator::CreateStationaryLight(
            LightParams(glm::vec3(97.0067, 41.6671, -3.15014), glm::vec3(1, 1, 1), 600, true),
            false
        );
        LightCreator::CreateStationaryLight(
            LightParams(glm::vec3(53.4627, 42.5004, 33.4819), glm::vec3(1, 1, 0.5), 800, true),
            false
        );
        LightCreator::CreateStationaryLight(
            LightParams(glm::vec3(52.8963, 43.3338, 0.290731), glm::vec3(1, 1, 0.5), 800, true),
            false
        );
        LightCreator::CreateStationaryLight(
            LightParams(glm::vec3(14.8891, 39.1671, 12.0289), glm::vec3(1, 1, 0.5), 800, true),
            false
        );
        LightCreator::CreateStationaryLight(
            LightParams(glm::vec3(17.7811, 42.5004, -20.3952), glm::vec3(1, 1, 0.5), 800, true),
            false
        );
        LightCreator::CreateStationaryLight(
            LightParams(glm::vec3(14.3924, 12.0836, 4.13855), glm::vec3(1, 1, 1), 10, true),
            false
        );
        LightCreator::CreateStationaryLight(
            LightParams(glm::vec3(14.2906, 12.0836, -1.69393), glm::vec3(1, 1, 1), 10, true),
            false
        );

        bool running = true;

        // for (int i = 0; i < 64; ++i) {
        //     float x = rand() % 600;
        //     float y = rand() % 600;
        //     float z = rand() % 200;
        //     stratus::VirtualPointLight * vpl = new stratus::VirtualPointLight();
        //     vpl->setIntensity(worldLight->getIntensity() * 50.0f);
        //     vpl->position = glm::vec3(x, y, z);
        //     vpl->setColor(worldLight->getColor());
        //     INSTANCE(RendererFrontend)->AddLight(stratus::LightPtr((stratus::Light *)vpl));
        // }

        return true;
    }

    // Run a single update for the application (no infinite loops)
    // deltaSeconds = time since last frame
    virtual stratus::SystemStatus Update(const double deltaSeconds) override {
        if (INSTANCE(Engine)->FrameCount() % 100 == 0) {
            STRATUS_LOG << "FPS:" << (1.0 / deltaSeconds) << " (" << (deltaSeconds * 1000.0) << " ms)" << std::endl;
        }

        auto worldLight = INSTANCE(RendererFrontend)->GetWorldLight();
        const glm::vec3 worldLightColor = worldLight->GetColor();
        const glm::vec3 warmMorningColor = glm::vec3(254.0f / 255.0f, 232.0f / 255.0f, 176.0f / 255.0f);
        const glm::vec3 defaultSunColor = glm::vec3(1.0f);
        const glm::vec3 moonlightColor = glm::vec3(79.0f / 255.0f, 105.0f / 255.0f, 136.0f / 255.0f);

        //STRATUS_LOG << "Camera " << camera.getYaw() << " " << camera.getPitch() << std::endl;

        // Check for key/mouse events
        auto events = INSTANCE(InputManager)->GetInputEventsLastFrame();
        for (auto e : events) {
            switch (e.type) {
                case SDL_QUIT:
                    return stratus::SystemStatus::SYSTEM_SHUTDOWN;
                case SDL_KEYDOWN:
                case SDL_KEYUP: {
                    bool released = e.type == SDL_KEYUP;
                    SDL_Scancode key = e.key.keysym.scancode;
                    switch (key) {
                        case SDL_SCANCODE_ESCAPE:
                            if (released) {
                                return stratus::SystemStatus::SYSTEM_SHUTDOWN;
                            }
                            break;
                        case SDL_SCANCODE_R:
                            if (released) {
                                INSTANCE(RendererFrontend)->RecompileShaders();
                            }
                            break;
                        case SDL_SCANCODE_1: {
                            if (released) {
                                LightCreator::CreateStationaryLight(
                                    //LightParams(INSTANCE(RendererFrontend)->GetCamera()->getPosition(), glm::vec3(1.0f, 1.0f, 0.5f), 1200.0f)
                                    LightParams(INSTANCE(RendererFrontend)->GetCamera()->GetPosition(), defaultSunColor, 600.0f),
                                    false
                                );
                            }
                            break;
                        }
                        case SDL_SCANCODE_2: {
                            if (released) {
                                LightCreator::CreateStationaryLight(
                                    LightParams(INSTANCE(RendererFrontend)->GetCamera()->GetPosition(), glm::vec3(1.0f, 1.0f, 0.5f), 800.0f),
                                    false
                                );
                            }
                            break;
                        }
                        case SDL_SCANCODE_3: {
                            if (released) {
                                LightCreator::CreateStationaryLight(
                                    LightParams(INSTANCE(RendererFrontend)->GetCamera()->GetPosition(), defaultSunColor, 10.0f),
                                    false
                                );
                            }
                            break;
                        }
                        default: break;
                    }
                    break;
                }
                default: break;
            }
        }

        if (requested.size() == received.size()) {
            received.clear();
            int spawned = 0;
            int stepSize = 15;
            for (int x = -25; x < 50; x += stepSize) {
                for (int y = 5; y < 70; y += stepSize) {
                    for (int z = -45; z < 300; z += stepSize) {
                            ++spawned;
                            LightCreator::CreateVirtualPointLight(
                                LightParams(glm::vec3(float(x), float(y), float(z)), glm::vec3(1.0f), 1.0f),
                                false
                            );
                    }
                }
            }

            STRATUS_LOG << "SPAWNED " << spawned << " VPLS\n";
        }

        // worldLight->setRotation(glm::vec3(75.0f, 0.0f, 0.0f));
        //worldLight->setRotation(stratus::Rotation(stratus::Degrees(30.0f), stratus::Degrees(0.0f), stratus::Degrees(0.0f)));

        #define LERP(x, v1, v2) (x * v1 + (1.0f - x) * v2)

        //renderer->toggleWorldLighting(worldLightEnabled);
        // worldLight->setColor(glm::vec3(1.0f, 0.75f, 0.5));
        // worldLight->setColor(glm::vec3(1.0f, 0.75f, 0.75f));
        //const float x = std::sinf(stratus::Radians(worldLight->getRotation().x).value());
        
        //worldLight->setRotation(glm::vec3(90.0f, 0.0f, 0.0f));
        //renderer->setWorldLight(worldLight);

        INSTANCE(RendererFrontend)->SetClearColor(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

        //renderer->addDrawable(rocks);

        // Add the camera's light
        //if (camLightEnabled) renderer->addPointLight(&cameraLight);
        //for (auto & entity : entities) {
        //    renderer->addDrawable(entity);
        //}

        //renderer->end(camera);

        //// 0 lets it run as fast as it can
        //SDL_GL_SetSwapInterval(0);
        //// Swap front and back buffer
        //SDL_GL_SwapWindow(window);

        return stratus::SystemStatus::SYSTEM_CONTINUE;
    }

    // Perform any resource cleanup
    virtual void Shutdown() override {
        LightCreator::Shutdown();
    }

private:
    std::vector<stratus::Async<stratus::Entity>> requested;
    std::vector<stratus::EntityPtr> received;
};

STRATUS_ENTRY_POINT(JunkShop)