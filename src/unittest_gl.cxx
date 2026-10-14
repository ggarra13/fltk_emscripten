//
// WebGL / OpenGL ES unit tests for fltk_emscripten.
//
// Drop this file into src/ and add it to the `unittests` target in
// CMakeLists.txt (see the bottom of this file). The tests use the same
// TEST()/EXPECT_*() mini framework as the other unittest_*.cxx files and
// show up under the "Core" test in the unittests app (or run headless in
// the browser with the `--core` argument).
//
// NOTE: the FLTK fork used by fltk_emscripten has no Fl_Gl_Window driver
// (only the Canvas 2D drawing driver) and FLTK_BUILD_GL is OFF, so an
// Fl_Gl_Window cannot be used. Instead these tests create a real WebGL
// context with Emscripten's <emscripten/html5.h> API on a dedicated canvas
// and verify that it works side by side with FLTK windows.
//

#include "unittests.h"

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Terminal.H>

#include <emscripten.h>
#include <emscripten/html5.h>
#include <GLES2/gl2.h>

#include <string.h>

namespace {

const char *const kCanvasId = "ut_gl_canvas";
const char *const kCanvasSel = "#ut_gl_canvas";
const int kSize = 64;

// RAII helper: creates a hidden <canvas> + WebGL context and makes it current.
// Its destructor destroys the context and removes the canvas, so a failing
// EXPECT_*() (which returns early) never leaks state into the next test.
class Gl_Fixture {
public:
  Gl_Fixture() : ctx_(0) {
    EM_ASM({
      var c = document.createElement("canvas");
      c.id = UTF8ToString($0);
      c.width = $1;
      c.height = $1;
      c.style.display = "none";
      document.body.appendChild(c);
    }, kCanvasId, kSize);

    EmscriptenWebGLContextAttributes attr;
    emscripten_webgl_init_context_attributes(&attr);
    attr.alpha = true;
    attr.depth = true;
    attr.antialias = false;
    attr.preserveDrawingBuffer = true;   // allow glReadPixels after drawing
    attr.majorVersion = 1;               // WebGL 1 == GLES 2.0
    attr.minorVersion = 0;
    ctx_ = emscripten_webgl_create_context(kCanvasSel, &attr);
    if (ctx_ > 0)
      emscripten_webgl_make_context_current(ctx_);
  }
  ~Gl_Fixture() {
    if (ctx_ > 0) {
      emscripten_webgl_make_context_current(0);
      emscripten_webgl_destroy_context(ctx_);
    }
    EM_ASM({
      var c = document.getElementById(UTF8ToString($0));
      if (c) c.remove();
    }, kCanvasId);
  }
  bool ok() const { return ctx_ > 0; }
  EMSCRIPTEN_WEBGL_CONTEXT_HANDLE handle() const { return ctx_; }
private:
  EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx_;
};

// Read back one RGBA pixel from the framebuffer.
void read_pixel(int x, int y, unsigned char rgba[4]) {
  glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
}

GLuint compile(GLenum type, const char *src) {
  GLuint sh = glCreateShader(type);
  glShaderSource(sh, 1, &src, NULL);
  glCompileShader(sh);
  GLint ok = 0;
  glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
  if (!ok) { glDeleteShader(sh); return 0; }
  return sh;
}

} // anonymous namespace

// TEST() defines a global Ut_Test named test__<__LINE__>. Without internal linkage
// that name can collide with a TEST() on the same line in another .cxx file
// (duplicate symbol at link time), so keep all tests in an anonymous namespace.
namespace {

/* A WebGL context can be created, made current and queried. */
TEST(OpenGL, ContextCreation) {
  Gl_Fixture gl;
  EXPECT_TRUE(gl.ok());
  EXPECT_EQ((int)emscripten_webgl_get_current_context() == (int)gl.handle(), 1);
  EXPECT_EQ((int)emscripten_is_webgl_context_lost(gl.handle()), 0);

  const char *version = (const char *)glGetString(GL_VERSION);
  EXPECT_TRUE(version != NULL);
  EXPECT_TRUE(strstr(version, "OpenGL ES") != NULL);   // WebGL reports GLES
  EXPECT_EQ((int)glGetError(), (int)GL_NO_ERROR);

  GLint maxTex = 0;
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTex);
  EXPECT_GE(maxTex, 1024);
  return true;
}

/* glClear writes the requested color into the drawing buffer. */
TEST(OpenGL, ClearAndReadback) {
  Gl_Fixture gl;
  EXPECT_TRUE(gl.ok());

  glViewport(0, 0, kSize, kSize);
  glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  unsigned char px[4] = {0, 0, 0, 0};
  read_pixel(kSize / 2, kSize / 2, px);
  EXPECT_EQ(px[0], 255);
  EXPECT_EQ(px[1], 0);
  EXPECT_EQ(px[2], 0);
  EXPECT_EQ(px[3], 255);

  glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  read_pixel(0, 0, px);
  EXPECT_EQ(px[0], 0);
  EXPECT_EQ(px[2], 255);
  EXPECT_EQ((int)glGetError(), (int)GL_NO_ERROR);
  return true;
}

/* Shaders compile and link, and a full-screen triangle renders correctly. */
TEST(OpenGL, ShaderAndDraw) {
  Gl_Fixture gl;
  EXPECT_TRUE(gl.ok());

  const char *vs =
    "attribute vec2 pos;\n"
    "void main() { gl_Position = vec4(pos, 0.0, 1.0); }\n";
  const char *fs =
    "precision mediump float;\n"
    "void main() { gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0); }\n";

  GLuint v = compile(GL_VERTEX_SHADER, vs);
  GLuint f = compile(GL_FRAGMENT_SHADER, fs);
  EXPECT_NE((int)v, 0);
  EXPECT_NE((int)f, 0);

  GLuint prog = glCreateProgram();
  glAttachShader(prog, v);
  glAttachShader(prog, f);
  glBindAttribLocation(prog, 0, "pos");
  glLinkProgram(prog);
  GLint linked = 0;
  glGetProgramiv(prog, GL_LINK_STATUS, &linked);
  EXPECT_EQ((int)linked, 1);

  // One oversized triangle that covers the whole viewport.
  static const GLfloat tri[] = { -1.f, -1.f,  3.f, -1.f,  -1.f, 3.f };
  GLuint vbo = 0;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(tri), tri, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

  glViewport(0, 0, kSize, kSize);
  glClearColor(0.f, 0.f, 0.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);
  glUseProgram(prog);
  glDrawArrays(GL_TRIANGLES, 0, 3);

  unsigned char px[4] = {0, 0, 0, 0};
  read_pixel(1, 1, px);
  EXPECT_EQ(px[0], 0);
  EXPECT_EQ(px[1], 255);
  EXPECT_EQ(px[2], 0);
  read_pixel(kSize - 2, kSize - 2, px);
  EXPECT_EQ(px[1], 255);
  EXPECT_EQ((int)glGetError(), (int)GL_NO_ERROR);

  glDeleteBuffers(1, &vbo);
  glDeleteProgram(prog);
  glDeleteShader(v);
  glDeleteShader(f);
  return true;
}

/* WebGL and an FLTK window coexist, and GL pixels can be handed to FLTK. */
TEST(OpenGL, CoexistsWithFltkWindow) {
  Gl_Fixture gl;
  EXPECT_TRUE(gl.ok());

  Fl_Window *win = new Fl_Window(50, 50, 200, 150, "GL coexistence");
  Fl_Box *box = new Fl_Box(10, 10, 180, 130, "FLTK widgets next to WebGL");
  box->box(FL_BORDER_BOX);
  win->end();
  win->show();
  Fl::flush();                       // draw now (avoids re-entrant Fl::wait under ASYNCIFY)
  EXPECT_TRUE(win->shown());
  EXPECT_TRUE(win->visible());
  EXPECT_EQ(win->w(), 200);
  EXPECT_EQ(win->h(), 150);

  // The GL context must still be alive and usable after FLTK created windows.
  EXPECT_EQ((int)emscripten_is_webgl_context_lost(gl.handle()), 0);
  emscripten_webgl_make_context_current(gl.handle());
  glViewport(0, 0, kSize, kSize);
  glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  static unsigned char pixels[kSize * kSize * 4];
  glReadPixels(0, 0, kSize, kSize, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

  // Wrap the GL framebuffer in an FLTK image (e.g. to draw it with fl_draw_image
  // or Fl_Image::draw in a widget).
  Fl_RGB_Image img(pixels, kSize, kSize, 4);
  EXPECT_EQ(img.w(), kSize);
  EXPECT_EQ(img.h(), kSize);
  EXPECT_EQ(img.d(), 4);
  EXPECT_EQ(img.fail(), 0);
  const char *d = img.data()[0];
  EXPECT_EQ(d[0], 0);
  EXPECT_EQ(d[1], 255);
  EXPECT_EQ(d[2], 0);

  win->hide();
  Fl::flush();
  EXPECT_TRUE(!win->shown());
  delete win;
  EXPECT_EQ((int)glGetError(), (int)GL_NO_ERROR);
  return true;
}


// ----- Browser tab ---------------------------------------------------------
//
// TEST() cases are only listed in the app via the "Core Functionality" tab
// (which runs every registered suite). To get an entry of its own in the
// list on the left, register a UnitTest. This tab shows a terminal and runs
// only the "OpenGL" suite the first time it is shown.
//
const int UT_TEST_GL = UT_TEST_CORE + 1;   // next free slot in the list

class Ut_GL_Test : public Fl_Group {
  Fl_Terminal *tty_;
  bool ran_;
public:
  static Fl_Widget *create() {
    return new Ut_GL_Test(UT_TESTAREA_X, UT_TESTAREA_Y, UT_TESTAREA_W, UT_TESTAREA_H);
  }
  Ut_GL_Test(int x, int y, int w, int h)
  : Fl_Group(x, y, w, h), tty_(NULL), ran_(false) {
    tty_ = new Fl_Terminal(x + 4, y + 4, w - 8, h - 8, "OpenGL / WebGL Test Log");
    tty_->ansi(true);
    end();
  }
  // Run from a timeout, not from inside the browser's selection callback.
  static void run_cb(void *v) {
    Ut_GL_Test *self = (Ut_GL_Test *)v;
    Fl_Terminal *saved = Ut_Suite::tty;   // keep the Core tab's terminal intact
    Ut_Suite::tty = self->tty_;
    int failed = Ut_Suite::locate("OpenGL")->run();
    Ut_Suite::printf("\n%s\n", failed ? "OpenGL tests: FAILED" : "OpenGL tests: all passed");
    Ut_Suite::tty = saved;
  }
  void show() FL_OVERRIDE {
    Fl_Group::show();
    if (!ran_) {
      ran_ = true;
      Fl::add_timeout(0.3, run_cb, this);
    }
  }
};

UnitTest gl_tab(UT_TEST_GL, "OpenGL (WebGL)", Ut_GL_Test::create);

} // anonymous namespace (tests)

// -----------------------------------------------------------------------------
// CMakeLists.txt changes
// -----------------------------------------------------------------------------
//   add_executable(unittests
//       ...
//       src/unittest_text.cxx
//       src/unittest_gl.cxx)                       # <-- new
//   target_link_libraries(unittests PRIVATE fltk::fltk fltk::images GL)  # <-- add GL
//
// LINK_FLAGS: the existing flags already contain -sOFFSCREENCANVAS_SUPPORT=1
// and -sASYNCIFY. No extra flags are required for WebGL 1; for WebGL 2 add
// -sMAX_WEBGL_VERSION=2 and set attr.majorVersion = 2 above.
