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

inline WGPUStringView make_stringview(const char* str) {
    WGPUStringView res = {0};
    if (str) {
        res.data = str;
        res.length = strlen(str);
    }
    return res;
}

//--------------------------------------------------
// vertex and fragment shaders
//--------------------------------------------------
static const char *wglsl_quad = SHADER_SOURCE(
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

class Ut_WebGPU_Quad_Window : public Fl_WebGPU_Window {
  Fl_Box *info_;   // shows WEBGPU_VERSION once the context exists

public:
  Ut_WebGPU_Quad_Window(int X, int Y, int W, int H, Fl_Box *info)
    : Fl_WebGPU_Window(X, Y, W, H, "WebGPU Quad Window")
  {
    info_ = info;
    mode(FL_RGB);
    clearValues = { 0.1, 0.3, 0.6, 1.0 };
  }

  ~Ut_WebGPU_Quad_Window()
        {
        }

  void init_webgpu()
    {
      if (info_)
      {
        WGPUAdapterInfo info = {0};
        wgpuAdapterGetInfo(wgpu.adapter, &info);

        char buf[1024];
        snprintf(buf, 1024, "Vendor: %s Architecture: %s Device: %s Description: %s",
                 info.vendor.data, info.architecture.data,
                 info.device.data, info.description.data);
        info_->copy_label(buf);

        wgpuAdapterInfoFreeMembers(info);
      }
    }

  void draw() FL_OVERRIDE {
    if (!valid())
    {
      init_webgpu();
    }
    // update rotation
    state.var.rot += 0.1f;
    state.var.rot = state.var.rot >= 360.f ? 0.0f : state.var.rot;
    wgpuQueueWriteBuffer(wgpu.queue(), state.res.ubuffer, 0, &state.var.rot, sizeof(state.var.rot));

    // draw quad (comment these five lines to simply clear the screen)
    wgpuRenderPassEncoderSetPipeline(wgpu.render_pass, wgpu.pipeline);
    wgpuRenderPassEncoderSetBindGroup(wgpu.render_pass, 0, state.res.bindgroup, 0, 0);
    wgpuRenderPassEncoderSetVertexBuffer(wgpu.render_pass, 0, state.res.vbuffer, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetIndexBuffer(wgpu.render_pass, state.res.ibuffer, WGPUIndexFormat_Uint16, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderDrawIndexed(wgpu.render_pass, 6, 1, 0, 0, 0);
  }


  WGPUShaderModule create_shader(const char* code, const char* label) {
    WGPUShaderSourceWGSL wgsl = {};
    wgsl.chain.sType = WGPUSType_ShaderSourceWGSL;
    wgsl.code = make_stringview(code);

    WGPUShaderModuleDescriptor shader_module_descriptor = {
        .nextInChain = (WGPUChainedStruct*)(&wgsl),
        .label = { label },
    };

    return wgpuDeviceCreateShaderModule(wgpu.device, &shader_module_descriptor);
  }

  WGPUBuffer create_buffer(const void* data, size_t size, WGPUBufferUsage usage) {
    WGPUBufferDescriptor wgpu_buffer_descriptor = {
        .usage = WGPUBufferUsage_CopyDst | usage,
        .size = size,
    };
    WGPUBuffer buffer = wgpuDeviceCreateBuffer(wgpu.device, &wgpu_buffer_descriptor);
    wgpuQueueWriteBuffer(wgpu.queue(), buffer, 0, data, size);
    return buffer;
  }

  void prepare_shaders()
    {

      // compile shaders
      WGPUShaderModule shader_quad = create_shader(wglsl_quad, NULL);

      // describe buffer layouts
      WGPUVertexAttribute vertex_attributes[2] = {
        // position: x, y
        {
          .format = WGPUVertexFormat_Float32x2,
          .offset = 0,
          .shaderLocation = 0,
        },
        // color: r, g, b
        {
          .format = WGPUVertexFormat_Float32x3,
          .offset = 2 * sizeof(float),
          .shaderLocation = 1,
        }
      };

      WGPUVertexBufferLayout vertex_buffer_layout = {
        .arrayStride = 5 * sizeof(float),
        .attributeCount = 2,
        .attributes = vertex_attributes,
      };

      // describe pipeline layout
      WGPUBindGroupLayoutEntry bindgroup_layout_entry =
        {
          .binding = 0,
          .visibility = WGPUShaderStage_Vertex,
          // buffer binding layout
          .buffer = {
            .type = WGPUBufferBindingType_Uniform,
          }
        };

      WGPUBindGroupLayoutDescriptor bindgroup_descriptor = {
        .entryCount = 1,
        // bind group layout entry
        .entries = &bindgroup_layout_entry
      };

      WGPUBindGroupLayout bindgroup_layout = wgpuDeviceCreateBindGroupLayout(wgpu.device,
                                                                             &bindgroup_descriptor);
      WGPUPipelineLayoutDescriptor pipeline_layout_descriptor = {
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = &bindgroup_layout,
      };

      WGPUPipelineLayout pipeline_layout =
        wgpuDeviceCreatePipelineLayout(wgpu.device,
                                       &pipeline_layout_descriptor);

      WGPUBlendState blend_state = {
          .color = {
            .operation = WGPUBlendOperation_Add,
            .srcFactor = WGPUBlendFactor_One,
            .dstFactor = WGPUBlendFactor_One,
          },
          .alpha = {
            .operation = WGPUBlendOperation_Add,
            .srcFactor = WGPUBlendFactor_One,
            .dstFactor = WGPUBlendFactor_One,
          }
      };

      WGPUColorTargetState target_state = {
        .format = wgpu.surface_format,
        // blend state
        .blend = &blend_state,
        .writeMask = WGPUColorWriteMask_All,
      };

      WGPUFragmentState fragment_state = {
        .module = shader_quad,
        .entryPoint = make_stringview("fs_main"),
        .targetCount = 1,
        .targets = &target_state,
      };

      WGPURenderPipelineDescriptor render_pipeline = {
        .label = make_stringview("Quad Pipeline"),
        // pipeline layout
        .layout = pipeline_layout,
        // vertex state
        .vertex = {
          .module = shader_quad,
          .entryPoint = make_stringview("vs_main"),
          .bufferCount = 1,
          .buffers = &vertex_buffer_layout,
        },
        // primitive state
        .primitive = {
          .topology = WGPUPrimitiveTopology_TriangleList,
          .stripIndexFormat = WGPUIndexFormat_Undefined,
          .frontFace = WGPUFrontFace_CCW,
          .cullMode = WGPUCullMode_None,
        },
        // depth-stencil state
        .depthStencil = NULL,
        // multi-sampling state
        .multisample = {
          .count = 1,
          .mask = 0xFFFFFFFF,
          .alphaToCoverageEnabled = false,
        },
        // fragment state
        .fragment = &fragment_state,
      };

      // create pipeline
      wgpu.pipeline = wgpuDeviceCreateRenderPipeline(wgpu.device, &render_pipeline);

      wgpuBindGroupLayoutRelease(bindgroup_layout);
      wgpuPipelineLayoutRelease(pipeline_layout);
      wgpuShaderModuleRelease(shader_quad);
    }

  void prepare_meshes()
    {
      // create the vertex buffer (x, y, r, g, b) and index buffer
      float const vertex_data[] = {
        // x, y          // r, g, b
        -0.5f, -0.5f,     1.0f, 0.0f, 0.0f, // bottom-left
        0.5f, -0.5f,     0.0f, 1.0f, 0.0f, // bottom-right
        0.5f,  0.5f,     0.0f, 0.0f, 1.0f, // top-right
        -0.5f,  0.5f,     1.0f, 1.0f, 0.0f, // top-left
      };
      uint16_t index_data[] = {
        0, 1, 2,
        0, 2, 3,
      };
      state.res.vbuffer = create_buffer(vertex_data, sizeof(vertex_data), WGPUBufferUsage_Vertex);
      state.res.ibuffer = create_buffer(index_data, sizeof(index_data), WGPUBufferUsage_Index);

      // create the uniform bind group
      state.res.ubuffer = create_buffer(&state.var.rot, sizeof(state.var.rot), WGPUBufferUsage_Uniform);


      WGPUBindGroupEntry bind_group_entry = {
        .binding = 0,
        .buffer = state.res.ubuffer,
        .offset = 0,
        .size = sizeof(state.var.rot),
      };

      WGPUBindGroupDescriptor bind_group_descriptor = {
          .layout = wgpuRenderPipelineGetBindGroupLayout(wgpu.pipeline, 0),
          .entryCount = 1,
          // bind group entry
          .entries = &bind_group_entry
      };

      state.res.bindgroup = wgpuDeviceCreateBindGroup(wgpu.device, &bind_group_descriptor);
    }

  void prepare() FL_OVERRIDE {
    //-----------------
    // setup pipeline
    //-----------------
    prepare_shaders();
    prepare_meshes();
  }
};


#endif

// ----- test group ---------------------------------------------------- MARK: -

class Ut_WebGPUTest : public Fl_Group {
#if HAVE_WEBGPU
    Ut_WebGPU_Quad_Window* webgpu_;
#endif
public:
  Ut_WebGPUTest(int X, int Y, int W, int H) : Fl_Group(X, Y, W, H) {
    box(FL_NO_BOX);
    Fl_Box *info = new Fl_Box(X, Y, W, 24, "");
    info->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    info->labelsize(12);
#if HAVE_WEBGPU
    webgpu_ = new Ut_WebGPU_Quad_Window(X, Y + 28, W, H - 28, info);
    webgpu_->border(0);
    webgpu_->end();
    resizable(webgpu_);
#endif
    end();
  }
};

#else // !__EMSCRIPTEN__

class Ut_WebGPUTest : public Fl_Group {
public:
  Ut_WebGPUTest(int X, int Y, int W, int H) : Fl_Group(X, Y, W, H) {
    box(FL_NO_BOX);
    Fl_Box *b = new Fl_Box(X, Y, W, H,
      "The WebGPU quad test uses WebGPU compatible shaders\n"
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
