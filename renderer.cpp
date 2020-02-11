#pragma once

/*
  TODO: Explain rendering method
  1- Render to offscreen framebuffer
  2- Extract pixels that are higher than 1.0f to secondary color buffer
  3- Blurr that secondary color buffer
  4- Merge secondary color buffer into regulat buffer
  5- Display merged colorbuffer to screen
*/

// STB Libs
#define STB_IMAGE_IMPLEMENTATION
#include "external/stb_image.h"

// Freetype
#include <ft2build.h>
#include FT_FREETYPE_H

#include "shared.h"
#include "renderer.h"
#include "game.h"

// TODO: Step through renderer and set global configuration variables
// Global renderer settings
global f32 Exposure__ = 2.0f;
global f32 EnableVSync = 0;
global i32 EnableBloom = 1;
global u32 BlurPassCount = 10; // How many times should we blurr the image

void R_DrawQuad(renderer *Renderer)
{ // TODO: This function is suspiciously too short, wtf?
    glBindVertexArray(Renderer->QuadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

u32 R_CreateShader(char *Filename)
{
    Assert(Filename);

    u32 Result;
    char *SourceFile = ReadTextFile(Filename);

    // Compile Vertex Shader
    u32 VertexShaderObject = glCreateShader(GL_VERTEX_SHADER);
    char *VertexSource[2] = {"#version 330 core\n#define VERTEX_SHADER\n", SourceFile};
    glShaderSource(VertexShaderObject, 2, VertexSource, NULL);
    glCompileShader(VertexShaderObject);
    i32 Compiled;
    glGetShaderiv(VertexShaderObject, GL_COMPILE_STATUS, &Compiled);
    if (Compiled != GL_TRUE)
    {
        i32 LogLength = 0;
        char ErrorMessage[1024];
        glGetShaderInfoLog(VertexShaderObject, 1024, &LogLength, ErrorMessage);
        fprintf(stderr, "%s-%s\n", Filename, ErrorMessage);
        VertexShaderObject = 0;
    }

    // Compile Fragment Shader
    u32 FragmentShaderObject = glCreateShader(GL_FRAGMENT_SHADER);
    char *FragmentSource[2] = {"#version 330 core\n#define FRAGMENT_SHADER\n", SourceFile};
    glShaderSource(FragmentShaderObject, 2, FragmentSource, NULL);
    glCompileShader(FragmentShaderObject);
    // i32 Compiled;
    glGetShaderiv(FragmentShaderObject, GL_COMPILE_STATUS, &Compiled);
    if (Compiled != GL_TRUE)
    {
        i32 LogLength = 0;
        char ErrorMessage[1024];
        glGetShaderInfoLog(FragmentShaderObject, 1024, &LogLength, ErrorMessage);
        fprintf(stderr, "%s-%s\n", Filename, ErrorMessage);
        FragmentShaderObject = 0;
    }

    // Link program
    Result = glCreateProgram();
    glAttachShader(Result, VertexShaderObject);
    glAttachShader(Result, FragmentShaderObject);
    glLinkProgram(Result);
    i32 IsLinked = 0;
    glGetProgramiv(Result, GL_LINK_STATUS, (GLint *)&IsLinked);
    if (IsLinked == GL_FALSE)
    {
        i32 MaxLogLength = 1024;
        char InfoLog[1024] = {0};
        glGetProgramInfoLog(Result, MaxLogLength, &MaxLogLength, &InfoLog[0]);
        printf("%s: SHADER PROGRAM FAILED TO COMPILE/LINK\n", Filename);
        printf("%s\n", InfoLog);
        glDeleteProgram(Result);
        Result = 0;
    }

    glDeleteShader(VertexShaderObject);
    glDeleteShader(FragmentShaderObject);
    Free(SourceFile);

    return Result;
}

void R_SetUniform(u32 Shader, char *Name, i32 Value)
{
    Assert(Name);
    glUniform1i(glGetUniformLocation(Shader, Name), Value);
}

void R_SetUniform(u32 Shader, char *Name, f32 Value)
{
    Assert(Name);
    glUniform1f(glGetUniformLocation(Shader, Name), Value);
}

void R_SetUniform(u32 Shader, char *Name, glm::mat4 *Value)
{
    Assert(Name);
    glUniformMatrix4fv(glGetUniformLocation(Shader, Name), 1, GL_FALSE, glm::value_ptr(*Value));
}

void R_SetUniform(u32 Shader, char *Name, glm::mat4 Value)
{
    Assert(Name);
    glUniformMatrix4fv(glGetUniformLocation(Shader, Name), 1, GL_FALSE, glm::value_ptr(Value));
}

void R_SetUniform(u32 Shader, char *Name, f32 X, f32 Y, f32 Z)
{
    Assert(Name);
    glUniform3f(glGetUniformLocation(Shader, Name), X, Y, Z);
}

void R_SetUniform(u32 Shader, char *Name, glm::vec3 Value)
{
    Assert(Name);
    glUniform3f(glGetUniformLocation(Shader, Name), Value.x, Value.y, Value.z);
}

void R_BeginFrame(renderer *Renderer)
{
    // NOTE: We need to clear the color buffer black, or else
    // the extracted brightness texture has another color
    // besides black, making the whole background glow
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, Renderer->Framebuffer);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void R_EndFrame(renderer *Renderer)
{
    // Blur bright fragments with two-pass Gaussian Blur
    // --------------------------------------------------
    b32 Horizontal = true, FirstIteration = true;
    glUseProgram(Renderer->Shaders.Blur);
    for (u32 i = 0; i < BlurPassCount; i++)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, Renderer->PingPongFBO[Horizontal]);
        R_SetUniform(Renderer->Shaders.Blur, "Horizontal", Horizontal);
        // R_SetUniform(Renderer->BlurShader, "Test", Angle);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, FirstIteration ? Renderer->BrightnessBuffer : Renderer->PingPongBuffer[!Horizontal]);  // bind texture of other framebuffer (or scene if first iteration)
        R_DrawQuad(Renderer);
        Horizontal = !Horizontal;
        if (FirstIteration)
        {
            FirstIteration = false;
        }
    }
    // Render floating point color buffer to 2D quad and tonemap HDR colors to default framebuffer's (clamped) color range
    // --------------------------------------------------------------------------------------------------------------------------
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(Renderer->Shaders.Bloom);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, Renderer->ColorBuffer);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, Renderer->PingPongBuffer[!Horizontal]);
    R_SetUniform(Renderer->Shaders.Bloom, "Bloom", EnableBloom);
    R_SetUniform(Renderer->Shaders.Bloom, "Exposure", Renderer->Exposure);
    R_DrawQuad(Renderer);

    SDL_GL_SwapWindow(Renderer->Window);
}

void R_ResizeRenderer(renderer *Renderer, i32 Width, i32 Height)
{
    /*
      This function deletes and recreates the opengl textures that we
      are using/rendering to. Framebuffers do not need to be resized,
      only the textures. The following textures are deleted and
      recreated to reflect the new width and height.

      u32 ColorBuffer;
      u32 BrightnessBuffer;
      u32 DepthStencilRenderbuffer;
      u32 PingPongBuffer[2];
    */

    Assert(Renderer);
    Assert(Width > 0);
    Assert(Height > 0);

    // Delete old texture and depth+stencil renderbuffer
    glDeleteTextures(1, &Renderer->ColorBuffer);
    glDeleteTextures(1, &Renderer->BrightnessBuffer);
    glDeleteRenderbuffers(1, &Renderer->DepthStencilRenderbuffer);

    glBindFramebuffer(GL_FRAMEBUFFER, Renderer->Framebuffer);
    // Colorbuffer
    glGenTextures(1, &Renderer->ColorBuffer);
    glBindTexture(GL_TEXTURE_2D, Renderer->ColorBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, Width, Height, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // BrightnessBuffer
    glGenTextures(1, &Renderer->BrightnessBuffer);
    glBindTexture(GL_TEXTURE_2D, Renderer->BrightnessBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, Width, Height, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // Renderbuffer
    glGenRenderbuffers(1, &Renderer->DepthStencilRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, Renderer->DepthStencilRenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, Width, Height);
    // Attach Buffers to Framebuffer
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, Renderer->ColorBuffer, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, Renderer->BrightnessBuffer, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, Renderer->DepthStencilRenderbuffer);
    // Tell opengl we are rendering to multiple buffers
    u32 Attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, Attachments);

    // Check if framebuffer is complete
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        printf("Framebuffer not complete, exiting!\n");
        exit(-1);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    { // SUBSECTION: PingPongFramebuffers
        glDeleteTextures(1, &Renderer->PingPongBuffer[0]);
        glDeleteTextures(1, &Renderer->PingPongBuffer[1]);

        glGenTextures(2, Renderer->PingPongBuffer);
        for (u32 i = 0; i < 2; i++)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, Renderer->PingPongFBO[i]);
            glBindTexture(GL_TEXTURE_2D, Renderer->PingPongBuffer[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, Width, Height, 0, GL_RGB, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, Renderer->PingPongBuffer[i], 0);
            if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            {
                printf("PingPong Framebuffer %d is not complete, exiting!\n", i);
            }
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    glViewport(0, 0, Width, Height);
}

renderer *R_CreateRenderer(window *Window)
{
    renderer *Result = (renderer*)Malloc(sizeof(renderer));
    Result->Window = Window->Handle;

    { // SECTION: OpenGL "Configuration"

        glFrontFace(GL_CCW);
        glEnable(GL_MULTISAMPLE);
        glEnable(GL_DEPTH_TEST); // NOTE: Disabling the depth test makes the red border cube look fine
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // No V-Sync
        if(EnableVSync)
        {
            SDL_GL_SetSwapInterval(1);
        }
        else
        {
            SDL_GL_SetSwapInterval(0);
        }

        Result->HardwareVendor = glGetString(GL_VENDOR);
        Result->HardwareModel = glGetString(GL_RENDERER);
        Result->OpenGLVersion = glGetString(GL_VERSION);
        Result->GLSLVersion = glGetString(GL_SHADING_LANGUAGE_VERSION);

        Result->Exposure = Exposure__;
    }

    glViewport(0, 0, Window->Width, Window->Height);

    { // SUBSECTION: Shader compilation
        Result->Shaders.Blur = R_CreateShader("shaders/blur.glsl");
        glUseProgram(Result->Shaders.Blur);
        R_SetUniform(Result->Shaders.Blur, "Image", 0);

        Result->Shaders.Bloom = R_CreateShader("shaders/bloom_final.glsl");
        glUseProgram(Result->Shaders.Bloom);
        R_SetUniform(Result->Shaders.Bloom, "Scene", 0);
        R_SetUniform(Result->Shaders.Bloom, "BloomBlur", 1);

        Result->Shaders.Cube = R_CreateShader("shaders/cube_shader.glsl"); // IMPORTANT: This shader also outputs to the brightness texture

        Result->Shaders.Hdr = R_CreateShader("shaders/HDR.glsl");
        R_SetUniform(Result->Shaders.Hdr, "HDRBuffer", 0);

        Result->Shaders.Texture = R_CreateShader("shaders/draw_texture.glsl");
        R_SetUniform(Result->Shaders.Texture, "Image", 0);

        Result->Shaders.Text = R_CreateShader("shaders/text.glsl");
        R_SetUniform(Result->Shaders.Text, "Text", 0);
        R_SetUniform(Result->Shaders.Text, "TextColor", glm::vec3(1.0f, 0.0f, 1.0f));
    }

    { // SUBSECTION: Upload vertex data to GPU
        // Upload Quad Data to the GPU
        glGenVertexArrays(1, &Result->QuadVAO);
        glGenBuffers(1, &Result->QuadVBO);
        glBindVertexArray(Result->QuadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, Result->QuadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(QuadVertices__), &QuadVertices__, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(f32), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(f32), (void*)(3 * sizeof(f32)));

        // Upload Cube data to the gpu
        glGenVertexArrays(1, &Result->CubeVAO);
        glGenBuffers(1, &Result->CubeVBO);
        glBindBuffer(GL_ARRAY_BUFFER, Result->CubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(CubeVertices__), CubeVertices__, GL_STATIC_DRAW);
        glBindVertexArray(Result->CubeVAO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(f32), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(f32), (void*)(3 * sizeof(f32)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(f32), (void*)(6 * sizeof(f32)));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        // Upload Text Data to GPU
        {
            glGenVertexArrays(1, &Result->TextVAO);
            glBindVertexArray(Result->TextVAO);
            glGenBuffers(1, &Result->TextVertexBuffer);
            glBindBuffer(GL_ARRAY_BUFFER, Result->TextVertexBuffer);
            glBufferData(GL_ARRAY_BUFFER, sizeof(f32) * 6 * 3, NULL, GL_DYNAMIC_DRAW); // 6 Vertices, 3 floats each
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(f32), 0);

            // f32 TextTexCoords__[6][2] =
            glGenBuffers(1, &Result->TextTexCoordsBuffer);
            glBindBuffer(GL_ARRAY_BUFFER, Result->TextTexCoordsBuffer);
            glBufferData(GL_ARRAY_BUFFER, sizeof(f32) * 6 * 2, TextTexCoords__, GL_DYNAMIC_DRAW); // 6 Vertices, 2 floats(UV) each
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(f32), 0);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        }
    }

    { // SECTION: HDR+Bloom setup
        // Main Framebuffer
        glGenFramebuffers(1, &Result->Framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, Result->Framebuffer);

        // Colorbuffer texture attachment
        glGenTextures(1, &Result->ColorBuffer);
        glBindTexture(GL_TEXTURE_2D, Result->ColorBuffer);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, Window->Width, Window->Height, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // BrightnessBuffer, Texture attachment needed to store every
        // HDR color > 1.0f. This buffer is needed for Bloom
        glGenTextures(1, &Result->BrightnessBuffer);
        glBindTexture(GL_TEXTURE_2D, Result->BrightnessBuffer);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, Window->Width, Window->Height, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenRenderbuffers(1, &Result->DepthStencilRenderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, Result->DepthStencilRenderbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, Window->Width, Window->Height);

        // Attach Buffers to Framebuffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, Result->ColorBuffer, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, Result->BrightnessBuffer, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, Result->DepthStencilRenderbuffer);
        // Tell opengl we are rendering to multiple buffers
        u32 Attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
        glDrawBuffers(2, Attachments);

        // Check if framebuffer is complete
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            printf("Framebuffer not complete, exiting!\n");
            exit(-1);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        { // SUBSECTION: PingPong Framebuffers creation

            glGenFramebuffers(2, Result->PingPongFBO);
            glGenTextures(2, Result->PingPongBuffer);
            for (u32 i = 0; i < 2; i++)
            {
                glBindFramebuffer(GL_FRAMEBUFFER, Result->PingPongFBO[i]);
                glBindTexture(GL_TEXTURE_2D, Result->PingPongBuffer[i]);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, Window->Width, Window->Height, 0, GL_RGB, GL_FLOAT, NULL);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, Result->PingPongBuffer[i], 0);
                if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                {
                    printf("PingPong Framebuffer %d is not complete, exiting!\n", i);
                }
            }
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
    }

    return (Result);
}

font *R_CreateFont(renderer *Renderer, char *Filename, i32 Width, i32 Height)
{
    Assert(Renderer);
    Assert(Filename);
    Assert(Width >= 0);
    Assert(Height >= 0);

    font *Result = (font*)Malloc(sizeof(font));
    Result->Filename = Filename;
    Result->CharacterWidth = Width;
    Result->CharacterHeight = Height;

    FT_Library FT;
    FT_Face Face;
    if(FT_Init_FreeType(&FT) == 0)
    {
        if(FT_New_Face(FT, Result->Filename, 0, &Face) == 0)
        {
            // Once we've loaded the face, we should define the font size we'd like to extract from this face
            FT_Set_Pixel_Sizes(Face, Result->CharacterWidth, Result->CharacterHeight);

            // Disable byte-alignment restriction
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // NOTE: Do we need to set it back to default?

            for(u8 CurrentChar = 0; CurrentChar < 255; CurrentChar++)
            {
                // Load character glyph
                if(FT_Load_Char(Face, CurrentChar, FT_LOAD_RENDER) == 0)
                {
                    // Generate Texture
                    u32 Texture;
                    glGenTextures(1, &Texture);
                    glBindTexture(GL_TEXTURE_2D, Texture);
                    glTexImage2D(GL_TEXTURE_2D,
                                 0,
                                 GL_RED,
                                 Face->glyph->bitmap.width,
                                 Face->glyph->bitmap.rows,
                                 0,
                                 GL_RED,
                                 GL_UNSIGNED_BYTE,
                                 Face->glyph->bitmap.buffer);

                    // Set Texture Options
                    // NOTE: What's better? clamp to edge or clam to border?
                    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

                    // Now store character for later use
                    character Character =
                    {
                        Texture,
                        glm::ivec2(Face->glyph->bitmap.width, Face->glyph->bitmap.rows),
                        glm::ivec2(Face->glyph->bitmap_left, Face->glyph->bitmap_top),
                        (u32)Face->glyph->advance.x,
                    };

                    Result->Characters[CurrentChar] = Character;
                    // Result->Characters.insert(std::pair<char, character>(CurrentChar, Character));
                }
                else
                {
                    printf("FT_Load_Char: Error, Freetype Failed to load Glyph %c\n", CurrentChar);
                }
            }
        }
        else
        {
            printf("FT_New_Face failed miserably while loading font %s\n", Result->Filename);
            Free(Result);
            return NULL;
        }
    }
    else
    {
        printf("FT_Init_FreeType failed miserably, could not init FreeType Library\n");
        Free(Result);
        return NULL;
    }
    // Destroy a given FreeType library object and all of its children, including resources, drivers, faces, sizes, etc.
    FT_Done_FreeType(FT);
    // glPixelStorei(GL_UNPACK_ALIGNMENT, 4); // NOTE: Do we need to reset it?
#if 0
    glGenVertexArrays(1, &Result->VAO);
    glBindVertexArray(Result->VAO);
    // Upload Vertex Data
    glGenBuffers(1, &Result->VertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, Result->VertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(f32) * 6 * 3, NULL, GL_DYNAMIC_DRAW); // 6 Vertices, 3 floats each
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(f32), 0);

    // Upload TexCoords
    f32 TexCoords[6][2] =
    {
        {0.0, 0.0f},
        {0.0f, 1.0f},
        {1.0, 1.0},
        {0.0, 0.0},
        {1.0, 1.0},
        {1.0, 0.0},
    };
    glGenBuffers(1, &Result->TexCoordsBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, Result->TexCoordsBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(f32) * 6 * 2, TexCoords, GL_DYNAMIC_DRAW); // 6 Vertices, 2 floats(UV) each
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(f32), 0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
#endif
    return Result;
}


u32 R_CompileShaderObject(const char *Source, GLenum ShaderType)
{
    Assert(Source);

    u32 Shader;
    Shader = glCreateShader(ShaderType);

    if(Shader)
    {
        glShaderSource(Shader, 1, &Source, NULL);
        glCompileShader(Shader);

        i32 Compiled;
        glGetShaderiv(Shader, GL_COMPILE_STATUS, &Compiled);
        if (Compiled != GL_TRUE)
        {
            i32 LogLength = 0;
            char ErrorMessage[1024];
            glGetShaderInfoLog(Shader, 1024, &LogLength, ErrorMessage);

            if(ShaderType == GL_VERTEX_SHADER)
            {
                fprintf(stderr, "Error compiling VERTEX SHADER\n");
            }
            else if(ShaderType == GL_FRAGMENT_SHADER)
            {
                fprintf(stderr, "Error compiling FRAGMENT SHADER\n");
            }
            else if(ShaderType == GL_GEOMETRY_SHADER)
            {
                fprintf(stderr, "Error compiling GEOMETRY SHADER\n");
            }
            fprintf(stderr, "%s\n", ErrorMessage);

            glDeleteShader(Shader);
            Shader = 0;
        }
    }
    else
    {
        fprintf(stderr, "glCreateShader Failed!\n");
    }

    return (Shader);
}

u32 R_CreateShader(char *VertexFile, char *FragmentFile)
{
    Assert(VertexFile);
    Assert(FragmentFile);

    char *VertexSource = ReadTextFile(VertexFile);
    Assert(VertexSource);
    char *FragmentSource = ReadTextFile(FragmentFile);
    Assert(FragmentSource);

    u32 Result;
    u32 VertexShader;
    u32 FragmentShader;
    VertexShader = R_CompileShaderObject(VertexSource, GL_VERTEX_SHADER);
    FragmentShader = R_CompileShaderObject(FragmentSource, GL_FRAGMENT_SHADER);

    // This function returns 0 if an error occurs creating the program object.
    Result = glCreateProgram();
    if(Result)
    {
        glAttachShader(Result, VertexShader);
        glAttachShader(Result, FragmentShader);
        glLinkProgram(Result);

        i32 IsLinked = 0;
        glGetProgramiv(Result, GL_LINK_STATUS, (GLint *)&IsLinked);
        if (IsLinked == GL_FALSE)
        {
            i32 MaxLogLength = 2048;
            // The maxLength includes the NULL character
            char InfoLog[2048] = {0};
            glGetProgramInfoLog(Result, MaxLogLength, &MaxLogLength, &InfoLog[0]);

            // We don't need the program anymore.
            glDeleteProgram(Result);

            // Use the infoLog as you see fit.
            printf("SHADER PROGRAM FAILED TO COMPILE\\LINK\n");
            printf("%s\n", InfoLog);
        }

    }

    glDeleteShader(VertexShader);
    glDeleteShader(FragmentShader);

    Free(VertexSource);
    Free(FragmentSource);

    return (Result);
}

texture *R_CreateTexture(char *Filename)
{
    Assert(Filename);

    texture *Result = (texture*)Malloc(sizeof(texture));

    i32 RequestedChannelCount = 0;
    i32 FlipVertically = 1;
    stbi_set_flip_vertically_on_load(FlipVertically);
    u8 *Data = stbi_load(Filename, &Result->Width, &Result->Height, &Result->ChannelCount, RequestedChannelCount);
    if(Data)
    {
        glGenTextures(1, &Result->Handle);
        glBindTexture(GL_TEXTURE_2D, Result->Handle);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        if(Result->ChannelCount == 3)
        {
            Result->Format = GL_RGB;
            Result->InternalFormat = GL_SRGB;
        }
        else if(Result->ChannelCount == 4)
        {
            Result->Format = GL_RGBA;
            Result->InternalFormat = GL_SRGB;
            // Result->InternalFormat = GL_SRGB_ALPHA;
        }
        else
        {
            fprintf(stderr, "%s Texture format is not GL_RGB or GL_RGBA\n", Filename);
            stbi_image_free(Data);
            return 0;
        }

        i32 MipMapDetailLevel = 0;
        // REMINDER: Textures to be used for data should not be uploaded as GL_SRGB!
        // NOTE: InternalFormat is the format we want to store the data, Format is the input format
        glTexImage2D(GL_TEXTURE_2D, MipMapDetailLevel, Result->InternalFormat, Result->Width, Result->Height, 0, Result->Format, GL_UNSIGNED_BYTE, Data);
        // NOTE(Jorge): Set custom MipMaps filtering values here!
        // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glGenerateMipmap(GL_TEXTURE_2D);

        stbi_image_free(Data);
    }
    else
    {
        printf("Could not load image file: %s\n", Filename);
    }

    return Result;
}

// TODO(Jorge): Remove the camera from functions parameters, maybe pass them in UBO? Or set the uniform to all shaders in R_BeginFrame();
void R_DrawTexture(renderer *Renderer, camera *Camera, texture *Texture, glm::vec3 Position, glm::vec3 Size, glm::vec3 RotationAxis, f32 RotationAngle)
{
    glUseProgram(Renderer->Shaders.Texture);
    glm::mat4 Model = glm::mat4(1.0f);
    Model = glm::translate(Model, Position);
    Model = glm::scale(Model, Size);
    Model = glm::rotate(Model, RotationAngle, RotationAxis);
    R_SetUniform(Renderer->Shaders.Texture, "Model", Model);
    R_SetUniform(Renderer->Shaders.Texture, "View", Camera->View);
    R_SetUniform(Renderer->Shaders.Texture, "Projection", Camera->Projection);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, Texture->Handle);
    glBindVertexArray(Renderer->QuadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}