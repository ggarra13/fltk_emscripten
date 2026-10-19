//
// Unit tests for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2025 by Bill Spitzak and others.
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file.  If this
// file is missing or damaged, see the license at:
//
//     https://www.fltk.org/COPYING.php
//
// Please see the following page on how to report bugs and issues:
//
//     https://www.fltk.org/bugs.php
//

// Draws a single RGB-shaded triangle using WebGPU
// so it exercises Fl_Emscripten_WebGPU_Window_Driver.

#include "unittests.h"

#include <config.h>
#include <FL/Fl_Box.H>
#include <FL/Fl_Group.H>
#if HAVE_WEBGPU
#include <FL/Fl_WebGPU_Context.H>
#include <FL/Fl_WebGPU_Window.H>
#endif
#include <FL/fl_draw.H>
#include <stdio.h>

#if defined(__EMSCRIPTEN__)

//--------------------------------------------------
// defines
//--------------------------------------------------
#define SHADER_SOURCE(...) #__VA_ARGS__

// ----- WebGPU window ------------------------------------------- MARK: -

#if HAVE_WEBGPU

// global state
typedef struct state_t {

    // resources
    struct {
        WGPUBuffer vbuffer, ibuffer, ubuffer;
        WGPUBindGroup bindgroup;
    } res;

    // vars
    struct {
        float rot;
    } var;
} state_t;

// state
static state_t state;

// helper functions
static void             create_surface();
static WGPUShaderModule create_shader(const char* code, const char* label);
static WGPUBuffer       create_buffer(const void* data, size_t size, WGPUBufferUsage usage);

//--------------------------------------------------
// vertex and fragment shaders
//--------------------------------------------------
static const char *vs_src = SHADER_SOURCE(
    // attribute/uniform decls

    struct VertexIn {
        @location(0) aPos : vec2<f32>,
        @location(1) aCol : vec3<f32>,
    };
    struct VertexOut {
        @location(0) vCol : vec3<f32>,
        @builtin(position) Position : vec4<f32>,
    };
    struct Rotation {
        @location(0) degs : f32,
    };
    @group(0) @binding(0) var<uniform> uRot : Rotation;

    // vertex shader

    @vertex
    fn vs_main(input : VertexIn) -> VertexOut {
        var rads : f32 = radians(uRot.degs);
        var cosA : f32 = cos(rads);
        var sinA : f32 = sin(rads);
        var rot : mat3x3<f32> = mat3x3<f32>(
            vec3<f32>( cosA, sinA, 0.0),
            vec3<f32>(-sinA, cosA, 0.0),
            vec3<f32>( 0.0,  0.0,  1.0));
        var output : VertexOut;
        output.Position = vec4<f32>(rot * vec3<f32>(input.aPos, 1.0), 1.0);
        output.vCol = input.aCol;
        return output;
    }

    // fragment shader
    @fragment
    fn fs_main(@location(0) vCol : vec3<f32>) -> @location(0) vec4<f32> {
        return vec4<f32>(vCol, 1.0);
    }
    );

class Ut_WebGPU_Triangle_Window : public Fl_WebGPU_Window {
  Fl_Box *info_;   // shows WEBGPU_VERSION once the context exists


  // Called whenever valid() is 0: first draw, resize of the context,
  // or after a WebGL context loss/restore (the driver calls invalidate()).
  void init_webgpu() {
  }

public:
  Ut_WebGPU_Triangle_Window(int X, int Y, int W, int H, Fl_Box *info)
    : Fl_WebGPU_Window(X, Y, W, H)
  {
    mode(FL_RGB);
  }

  ~Ut_WebGPU_Triangle_Window()
        {
        }

  void draw() FL_OVERRIDE {
  }
};


#endif

// ----- test group ---------------------------------------------------- MARK: -

class Ut_WebGPUTest : public Fl_Group {
#if HAVE_WEBGPU
    Ut_WebGPU_Triangle_Window* webgpu_;
#endif
public:
  Ut_WebGPUTest(int X, int Y, int W, int H) : Fl_Group(X, Y, W, H) {
    box(FL_NO_BOX);
    Fl_Box *info = new Fl_Box(X, Y, W, 24, "");
    info->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    info->labelsize(12);
#if HAVE_WEBGPU
    webgpu_ = new Ut_WebGPU_Triangle_Window(X, Y + 28, W, H - 28, info);
    webgpu_->border(0);
    webgpu_->end();
    resizable(webgpu_);
#endif
    end();
  }

#if HAVE_WEBGPU
  void show() FL_OVERRIDE {
    Fl_Group::show();
    if (webgpu_) webgpu_->show();
  }

  void hide() FL_OVERRIDE {
    if (webgpu_) webgpu_->hide();          // this finally runs delete_div()
    Fl_Group::hide();
  }
#endif
};

#else // !__EMSCRIPTEN__

class Ut_WebGPUTest : public Fl_Group {
public:
  Ut_WebGPUTest(int X, int Y, int W, int H) : Fl_Group(X, Y, W, H) {
    box(FL_NO_BOX);
    Fl_Box *b = new Fl_Box(X, Y, W, H,
      "The GL triangle test uses WebGL-compatible GLES2 shaders\n"
      "and is only built for the Emscripten platform.");
    b->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE);
    end();
  }
};

#endif // __EMSCRIPTEN__

static Fl_Widget *webgpu_test_create() {
  return new Ut_WebGPUTest(UT_TESTAREA_X, UT_TESTAREA_Y, UT_TESTAREA_W, UT_TESTAREA_H);
}

UnitTest ut_webgpu(UT_TEST_WEBGPU, "WebGPU", webgpu_test_create);
