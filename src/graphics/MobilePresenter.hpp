#pragma once

#include "GLFunc.hpp"
#include "TouchControls.hpp"

#include <SDL2/SDL.h>

struct MobilePresenter
{
    bool Init(SDL_Window *window, bool glesContext, i32 width, i32 height);
    void Present(GLuint gameProgram);
    void Release();

  private:
    void DrawQuad(f32 x0, f32 y0, f32 x1, f32 y1, bool flipV);
    void LoadLabels();

    SDL_Window *window = NULL;
    i32 width = 0;
    i32 height = 0;
    GLint defaultFramebuffer = 0;
    GLuint framebuffer = 0;
    GLuint colorTexture = 0;
    GLuint depthBuffer = 0;
    GLuint program = 0;
    GLint modeUniform = -1;
    GLint colorUniform = -1;
    GLint texUniform = -1;
    i32 drawableW = 1;
    i32 drawableH = 1;
    bool labelsLoaded = false;
    GLuint labelTextures[TouchControls::LABEL_COUNT] = {};
    i32 labelW[TouchControls::LABEL_COUNT] = {};
    i32 labelH[TouchControls::LABEL_COUNT] = {};
};
