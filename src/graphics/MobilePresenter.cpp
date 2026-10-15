#include "MobilePresenter.hpp"
#include "utils.hpp"

#define PRESENT_POSITION_ATTRIBUTE 3
#define PRESENT_TEXCOORD_ATTRIBUTE 4

#define PRESENT_MODE_BLIT 0.0f
#define PRESENT_MODE_CIRCLE 1.0f
#define PRESENT_MODE_RING 2.0f
#define PRESENT_MODE_LABEL 3.0f

static const char *c_PresentVert = "attribute vec2 a_pos;\n"
                                   "attribute vec2 a_uv;\n"
                                   "varying vec2 v_uv;\n"
                                   "void main()\n"
                                   "{\n"
                                   "    v_uv = a_uv;\n"
                                   "    gl_Position = vec4(a_pos, 0.0, 1.0);\n"
                                   "}\n";

static const char *c_PresentFrag = "uniform sampler2D u_tex;\n"
                                   "uniform float u_mode;\n"
                                   "uniform vec4 u_color;\n"
                                   "varying vec2 v_uv;\n"
                                   "void main()\n"
                                   "{\n"
                                   "    if (u_mode < 0.5)\n"
                                   "    {\n"
                                   "        gl_FragColor = vec4(texture2D(u_tex, v_uv).rgb, 1.0);\n"
                                   "    }\n"
                                   "    else if (u_mode < 2.5)\n"
                                   "    {\n"
                                   "        float d = length(v_uv * 2.0 - 1.0);\n"
                                   "        float edge = 1.0 - smoothstep(0.92, 1.0, d);\n"
                                   "        float body = u_mode < 1.5 ? 0.55 + 0.45 * smoothstep(0.72, 0.92, d)\n"
                                   "                                  : smoothstep(0.80, 0.90, d);\n"
                                   "        gl_FragColor = vec4(u_color.rgb, u_color.a * edge * body);\n"
                                   "    }\n"
                                   "    else\n"
                                   "    {\n"
                                   "        float t = texture2D(u_tex, v_uv).a;\n"
                                   "        gl_FragColor = vec4(1.0, 1.0, 1.0, t * u_color.a);\n"
                                   "    }\n"
                                   "}\n";

static GLuint CompilePresentShader(GLenum type, const char *source, bool glesContext)
{
    const char *sources[2];
    sources[0] = glesContext ? "#version 100\nprecision mediump float;\n" : "#version 120\n";
    sources[1] = source;

    GLuint shader = g_glFuncTable.glCreateShader(type);
    if (shader == 0)
    {
        return 0;
    }
    g_glFuncTable.glShaderSource(shader, 2, sources, NULL);
    g_glFuncTable.glCompileShader(shader);

    GLint ok = 0;
    g_glFuncTable.glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char log[512];
        g_glFuncTable.glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        utils::DebugPrint("Present shader compile failed: %s", log);
        g_glFuncTable.glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool MobilePresenter::Init(SDL_Window *targetWindow, bool glesContext, i32 targetWidth, i32 targetHeight)
{
    this->window = targetWindow;
    this->width = targetWidth;
    this->height = targetHeight;

    if (g_glFuncTable.glGenFramebuffers == NULL || g_glFuncTable.glBindFramebuffer == NULL ||
        g_glFuncTable.glGenRenderbuffers == NULL || g_glFuncTable.glIsEnabled == NULL ||
        g_glFuncTable.glDisable == NULL)
    {
        return false;
    }

    g_glFuncTable.glGetIntegerv(GL_FRAMEBUFFER_BINDING, &this->defaultFramebuffer);

    GLuint vert = CompilePresentShader(GL_VERTEX_SHADER, c_PresentVert, glesContext);
    GLuint frag = CompilePresentShader(GL_FRAGMENT_SHADER, c_PresentFrag, glesContext);
    if (vert == 0 || frag == 0)
    {
        return false;
    }

    this->program = g_glFuncTable.glCreateProgram();
    g_glFuncTable.glAttachShader(this->program, vert);
    g_glFuncTable.glAttachShader(this->program, frag);
    g_glFuncTable.glBindAttribLocation(this->program, PRESENT_POSITION_ATTRIBUTE, "a_pos");
    g_glFuncTable.glBindAttribLocation(this->program, PRESENT_TEXCOORD_ATTRIBUTE, "a_uv");
    g_glFuncTable.glLinkProgram(this->program);
    g_glFuncTable.glDeleteShader(vert);
    g_glFuncTable.glDeleteShader(frag);

    GLint linked = 0;
    g_glFuncTable.glGetProgramiv(this->program, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        g_glFuncTable.glDeleteProgram(this->program);
        this->program = 0;
        return false;
    }

    this->modeUniform = g_glFuncTable.glGetUniformLocation(this->program, "u_mode");
    this->colorUniform = g_glFuncTable.glGetUniformLocation(this->program, "u_color");
    this->texUniform = g_glFuncTable.glGetUniformLocation(this->program, "u_tex");
    g_glFuncTable.glUseProgram(this->program);
    g_glFuncTable.glUniform1i(this->texUniform, 0);

    g_glFuncTable.glGenTextures(1, &this->colorTexture);
    g_glFuncTable.glBindTexture(GL_TEXTURE_2D, this->colorTexture);
    g_glFuncTable.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    g_glFuncTable.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    g_glFuncTable.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    g_glFuncTable.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    g_glFuncTable.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, this->width, this->height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                               NULL);
    g_glFuncTable.glBindTexture(GL_TEXTURE_2D, 0);

    g_glFuncTable.glGenRenderbuffers(1, &this->depthBuffer);
    g_glFuncTable.glBindRenderbuffer(GL_RENDERBUFFER, this->depthBuffer);
    g_glFuncTable.glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, this->width, this->height);
    g_glFuncTable.glBindRenderbuffer(GL_RENDERBUFFER, 0);

    g_glFuncTable.glGenFramebuffers(1, &this->framebuffer);
    g_glFuncTable.glBindFramebuffer(GL_FRAMEBUFFER, this->framebuffer);
    g_glFuncTable.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, this->colorTexture,
                                         0);
    g_glFuncTable.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                            this->depthBuffer);

    if (g_glFuncTable.glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        g_glFuncTable.glBindFramebuffer(GL_FRAMEBUFFER, this->defaultFramebuffer);
        this->Release();
        return false;
    }

    g_glFuncTable.glViewport(0, 0, this->width, this->height);
    g_glFuncTable.glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    g_glFuncTable.glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    return true;
}

void MobilePresenter::Release()
{
    for (i32 i = 0; i < TouchControls::LABEL_COUNT; i++)
    {
        if (this->labelTextures[i] != 0)
        {
            g_glFuncTable.glDeleteTextures(1, &this->labelTextures[i]);
            this->labelTextures[i] = 0;
        }
    }
    this->labelsLoaded = false;
    if (this->framebuffer != 0)
    {
        g_glFuncTable.glDeleteFramebuffers(1, &this->framebuffer);
        this->framebuffer = 0;
    }
    if (this->depthBuffer != 0)
    {
        g_glFuncTable.glDeleteRenderbuffers(1, &this->depthBuffer);
        this->depthBuffer = 0;
    }
    if (this->colorTexture != 0)
    {
        g_glFuncTable.glDeleteTextures(1, &this->colorTexture);
        this->colorTexture = 0;
    }
    if (this->program != 0)
    {
        g_glFuncTable.glDeleteProgram(this->program);
        this->program = 0;
    }
}

void MobilePresenter::LoadLabels()
{
    this->labelsLoaded = true;
    for (i32 i = 0; i < TouchControls::LABEL_COUNT; i++)
    {
        SDL_Surface *surface = TouchControls::CreateLabelSurface(i);
        if (surface == NULL)
        {
            continue;
        }
        if (surface->pitch == surface->w * 4)
        {
            g_glFuncTable.glGenTextures(1, &this->labelTextures[i]);
            g_glFuncTable.glBindTexture(GL_TEXTURE_2D, this->labelTextures[i]);
            g_glFuncTable.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            g_glFuncTable.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            g_glFuncTable.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            g_glFuncTable.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            SDL_LockSurface(surface);
            g_glFuncTable.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface->w, surface->h, 0, GL_RGBA,
                                       GL_UNSIGNED_BYTE, surface->pixels);
            SDL_UnlockSurface(surface);
            this->labelW[i] = surface->w;
            this->labelH[i] = surface->h;
        }
        SDL_FreeSurface(surface);
    }
}

void MobilePresenter::DrawQuad(f32 x0, f32 y0, f32 x1, f32 y1, bool flipV)
{
    f32 l = x0 / this->drawableW * 2.0f - 1.0f;
    f32 r = x1 / this->drawableW * 2.0f - 1.0f;
    f32 t = 1.0f - y0 / this->drawableH * 2.0f;
    f32 b = 1.0f - y1 / this->drawableH * 2.0f;
    f32 vTop = flipV ? 0.0f : 1.0f;
    f32 vBottom = flipV ? 1.0f : 0.0f;

    const GLfloat verts[16] = {l, t, 0.0f, vTop, l, b, 0.0f, vBottom, r, t, 1.0f, vTop, r, b, 1.0f, vBottom};

    g_glFuncTable.glVertexAttribPointer(PRESENT_POSITION_ATTRIBUTE, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat),
                                        verts);
    g_glFuncTable.glVertexAttribPointer(PRESENT_TEXCOORD_ATTRIBUTE, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat),
                                        verts + 2);
    g_glFuncTable.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void MobilePresenter::Present(GLuint gameProgram)
{
    GLint prevTexture = 0;
    GLint prevViewport[4] = {0, 0, this->width, this->height};
    GLint prevBlendSrc = GL_SRC_ALPHA;
    GLint prevBlendDst = GL_ONE_MINUS_SRC_ALPHA;
    GLint prevDepthMask = GL_TRUE;
    GLfloat prevClear[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    g_glFuncTable.glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTexture);
    g_glFuncTable.glGetIntegerv(GL_VIEWPORT, prevViewport);
    g_glFuncTable.glGetIntegerv(GL_BLEND_SRC_RGB, &prevBlendSrc);
    g_glFuncTable.glGetIntegerv(GL_BLEND_DST_RGB, &prevBlendDst);
    g_glFuncTable.glGetIntegerv(GL_DEPTH_WRITEMASK, &prevDepthMask);
    g_glFuncTable.glGetFloatv(GL_COLOR_CLEAR_VALUE, prevClear);
    GLboolean prevDepthTest = g_glFuncTable.glIsEnabled(GL_DEPTH_TEST);
    GLboolean prevBlend = g_glFuncTable.glIsEnabled(GL_BLEND);

    SDL_GL_GetDrawableSize(this->window, &this->drawableW, &this->drawableH);
    if (this->drawableW <= 0 || this->drawableH <= 0)
    {
        SDL_GetWindowSize(this->window, &this->drawableW, &this->drawableH);
    }
    TouchControls::SetScreenSize(this->drawableW, this->drawableH);

    if (!this->labelsLoaded)
    {
        this->LoadLabels();
    }

    g_glFuncTable.glBindFramebuffer(GL_FRAMEBUFFER, this->defaultFramebuffer);
    g_glFuncTable.glViewport(0, 0, this->drawableW, this->drawableH);
    g_glFuncTable.glDisable(GL_DEPTH_TEST);
    g_glFuncTable.glDisable(GL_BLEND);
    g_glFuncTable.glDepthMask(GL_FALSE);
    g_glFuncTable.glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    g_glFuncTable.glClear(GL_COLOR_BUFFER_BIT);

    g_glFuncTable.glUseProgram(this->program);
    g_glFuncTable.glEnableVertexAttribArray(PRESENT_POSITION_ATTRIBUTE);
    g_glFuncTable.glEnableVertexAttribArray(PRESENT_TEXCOORD_ATTRIBUTE);

    f32 scaleX = (f32)this->drawableW / this->width;
    f32 scaleY = (f32)this->drawableH / this->height;
    f32 scale = scaleX < scaleY ? scaleX : scaleY;
    f32 outW = this->width * scale;
    f32 outH = this->height * scale;
    f32 outX = (this->drawableW - outW) * 0.5f;
    f32 outY = (this->drawableH - outH) * 0.5f;

    g_glFuncTable.glBindTexture(GL_TEXTURE_2D, this->colorTexture);
    g_glFuncTable.glUniform1f(this->modeUniform, PRESENT_MODE_BLIT);
    this->DrawQuad(outX, outY, outX + outW, outY + outH, false);

    TouchControls::OverlayItem items[TouchControls::MAX_OVERLAY_ITEMS];
    i32 itemCount = TouchControls::BuildOverlay(items, TouchControls::MAX_OVERLAY_ITEMS);

    if (itemCount > 0)
    {
        g_glFuncTable.glEnable(GL_BLEND);
        g_glFuncTable.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        for (i32 i = 0; i < itemCount; i++)
        {
            const TouchControls::OverlayItem &item = items[i];
            g_glFuncTable.glUniform1f(this->modeUniform, item.shape == TouchControls::OVERLAY_RING
                                                             ? PRESENT_MODE_RING
                                                             : PRESENT_MODE_CIRCLE);
            g_glFuncTable.glUniform4f(this->colorUniform, item.r, item.g, item.b, item.a);
            this->DrawQuad(item.x - item.radius, item.y - item.radius, item.x + item.radius, item.y + item.radius,
                           false);

            if (item.label >= 0 && item.label < TouchControls::LABEL_COUNT && this->labelTextures[item.label] != 0)
            {
                f32 labelHeight = item.radius * 0.55f;
                f32 labelWidth = labelHeight * this->labelW[item.label] / (f32)this->labelH[item.label];
                if (labelWidth > item.radius * 1.5f)
                {
                    labelHeight *= item.radius * 1.5f / labelWidth;
                    labelWidth = item.radius * 1.5f;
                }
                g_glFuncTable.glBindTexture(GL_TEXTURE_2D, this->labelTextures[item.label]);
                g_glFuncTable.glUniform1f(this->modeUniform, PRESENT_MODE_LABEL);
                g_glFuncTable.glUniform4f(this->colorUniform, 1.0f, 1.0f, 1.0f, item.a + 0.3f);
                this->DrawQuad(item.x - labelWidth * 0.5f, item.y - labelHeight * 0.5f, item.x + labelWidth * 0.5f,
                               item.y + labelHeight * 0.5f, true);
            }
        }
    }

    g_glFuncTable.glDisableVertexAttribArray(PRESENT_POSITION_ATTRIBUTE);
    g_glFuncTable.glDisableVertexAttribArray(PRESENT_TEXCOORD_ATTRIBUTE);

    SDL_GL_SwapWindow(this->window);

    g_glFuncTable.glBindFramebuffer(GL_FRAMEBUFFER, this->framebuffer);
    g_glFuncTable.glUseProgram(gameProgram);
    g_glFuncTable.glBindTexture(GL_TEXTURE_2D, (GLuint)prevTexture);
    g_glFuncTable.glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
    g_glFuncTable.glBlendFunc((GLenum)prevBlendSrc, (GLenum)prevBlendDst);
    if (prevBlend)
    {
        g_glFuncTable.glEnable(GL_BLEND);
    }
    else
    {
        g_glFuncTable.glDisable(GL_BLEND);
    }
    if (prevDepthTest)
    {
        g_glFuncTable.glEnable(GL_DEPTH_TEST);
    }
    else
    {
        g_glFuncTable.glDisable(GL_DEPTH_TEST);
    }
    g_glFuncTable.glDepthMask(prevDepthMask ? GL_TRUE : GL_FALSE);
    g_glFuncTable.glClearColor(prevClear[0], prevClear[1], prevClear[2], prevClear[3]);
}
