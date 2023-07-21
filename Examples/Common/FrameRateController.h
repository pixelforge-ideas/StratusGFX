#pragma once

#include "StratusCommon.h"
#include "glm/glm.hpp"
#include "StratusWindow.h"
#include "StratusRendererFrontend.h"
#include "StratusLog.h"
#include "StratusCamera.h"
#include "StratusLight.h"

struct FrameRateController : public stratus::InputHandler {
    FrameRateController() {
        //INSTANCE(RendererFrontend)->SetVsyncEnabled(true);
        // 1000 fps is just to get the engine out of the way so SDL can control it with vsync
        frameRates_ = {1000, 60, 55, 50, 45, 40, 35, 30};
        INSTANCE(Engine)->SetMaxFrameRate(frameRates_.size() - 1]);
    }

    // This class is deleted when the Window is deleted so the Renderer has likely already
    // been taken offline. The check is for if the camera controller is removed while the engine
    // is still running.
    virtual ~FrameRateController() {}

    void HandleInput(const stratus::MouseState& mouse, const std::vector<SDL_Event>& input, const double deltaSeconds) {
        const float camSpeed = 100.0f;

        // Handle WASD movement
        for (auto e : input) {
            switch (e.type) {
                case SDL_KEYDOWN:
                case SDL_KEYUP: {
                    bool released = e.type == SDL_KEYUP;
                    SDL_Scancode key = e.key.keysym.scancode;
                    switch (key) {
                        // Why M? Because F is used for flash light and R is recompile :(
                        case SDL_SCANCODE_M:
                            if (released) {
                                frameRateIndex_ = (frameRateIndex_ + 1) % frameRates_.size();
                                INSTANCE(Engine)->SetMaxFrameRate(frameRates_[frameRateIndex_]);
                                STRATUS_LOG << "Max Frame Rate Toggled: " << frameRates_[frameRateIndex_] << std::endl;
                                break;
                            }
                    }
                }
            }
        }
    }

private:
    size_t frameRateIndex_ = 0;
    std::vector<uint32_t> frameRates_;
};