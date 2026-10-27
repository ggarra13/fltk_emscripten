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

// Draws a single RGB-shaded triangle using only GLSL ES 1.00 shaders and a
// vertex buffer. That subset works on WebGL1 and WebGL2 (and desktop GLES2),
// so it exercises Fl_Emscripten_Gl_Window_Driver without needing
// LEGACY_GL_EMULATION, display lists or gl_draw() text.

#include "unittests.h"

#include <config.h>
#include <FL/Fl_Box.H>
#include <FL/Fl_Group.H>
#if HAVE_GL
#include <FL/Fl_Gl_Window.H>
#endif
#include <FL/fl_draw.H>
#include <stdio.h>

#if defined(__EMSCRIPTEN__)

#include <FL/gl.h>

// ----- GL window ----------------------------------------------------- MARK: -

#if HAVE_GL
class Ut_Gl_Triangle_Window : public Fl_Gl_Window {
  GLuint program_;
  GLuint vbo_;
  GLint  pos_loc_;
  GLint  col_loc_;
  bool   ok_;
  Fl_Box *info_;   // shows GL_VERSION once the context exists

  static GLuint compile(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint status = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &status);
    if (!status) {
      char log[512] = "";
      glGetShaderInfoLog(s, sizeof(log), NULL, log);
      fprintf(stderr, "unittest_gl: shader compile failed: %s\n", log);
      glDeleteShader(s);
      return 0;
    }
    return s;
  }

  void free_gl() {
    if (program_) { glDeleteProgram(program_); program_ = 0; }
    if (vbo_)     { glDeleteBuffers(1, &vbo_); vbo_ = 0; }
    ok_ = false;
  }

  // Called whenever valid() is 0: first draw, resize of the context,
  // or after a WebGL context loss/restore (the driver calls invalidate()).
  void init_gl() {
    free_gl();

    program_ = 0;
    vbo_ = 0;

    static const char *vs_src =
      "attribute vec2 a_pos;\n"
      "attribute vec3 a_col;\n"
      "varying vec3 v_col;\n"
      "void main() {\n"
      "  v_col = a_col;\n"
      "  gl_Position = vec4(a_pos, 0.0, 1.0);\n"
      "}\n";
    static const char *fs_src =
      "precision mediump float;\n"
      "varying vec3 v_col;\n"
      "void main() {\n"
      "  gl_FragColor = vec4(v_col, 1.0);\n"
      "}\n";

    GLuint vs = compile(GL_VERTEX_SHADER, vs_src);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fs_src);
    if (vs && fs) {
      program_ = glCreateProgram();
      glAttachShader(program_, vs);
      glAttachShader(program_, fs);
      glLinkProgram(program_);
      GLint status = 0;
      glGetProgramiv(program_, GL_LINK_STATUS, &status);
      if (status) {
        pos_loc_ = glGetAttribLocation(program_, "a_pos");
        col_loc_ = glGetAttribLocation(program_, "a_col");
        // x, y, r, g, b
        static const GLfloat verts[] = {
           0.0f,  0.7f,   1.0f, 0.0f, 0.0f,
          -0.7f, -0.7f,   0.0f, 1.0f, 0.0f,
           0.7f, -0.7f,   0.0f, 0.0f, 1.0f
        };
        glGenBuffers(1, &vbo_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
        ok_ = (pos_loc_ >= 0 && col_loc_ >= 0);
      } else {
        char log[512] = "";
        glGetProgramInfoLog(program_, sizeof(log), NULL, log);
        fprintf(stderr, "unittest_gl: program link failed: %s\n", log);
      }
    }
    if (vs) glDeleteShader(vs);   // flagged for deletion, freed with the program
    if (fs) glDeleteShader(fs);

    if (info_) {
      const char *ver = (const char *)glGetString(GL_VERSION);
      info_->copy_label(ver ? ver : "unknown GL version");
    }
  }

public:
  Ut_Gl_Triangle_Window(int X, int Y, int W, int H, Fl_Box *info)
    : Fl_Gl_Window(X, Y, W, H),
      program_(0), vbo_(0), pos_loc_(-1), col_loc_(-1), ok_(false), info_(info)
  {
    mode(FL_RGB);
  }

  ~Ut_Gl_Triangle_Window()
        {
            // Do not touch GL here unless you make_current() first.
            if (shown()) { make_current(); free_gl(); }
        }

  void hide() FL_OVERRIDE {
    if (shown()) {
      make_current();
      free_gl();        // delete while the context is still alive
    }
  }

  void draw() FL_OVERRIDE {
    if (!valid()) {
      init_gl();
      valid(1);
    }
    // pixel_w()/pixel_h() account for the high-DPI canvas scale.
    glViewport(0, 0, pixel_w(), pixel_h());
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (!ok_) return;

    glUseProgram(program_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    const GLsizei stride = 5 * sizeof(GLfloat);
    glEnableVertexAttribArray(pos_loc_);
    glVertexAttribPointer(pos_loc_, 2, GL_FLOAT, GL_FALSE, stride, (const void *)0);
    glEnableVertexAttribArray(col_loc_);
    glVertexAttribPointer(col_loc_, 3, GL_FLOAT, GL_FALSE, stride,
                          (const void *)(2 * sizeof(GLfloat)));
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDisableVertexAttribArray(pos_loc_);
    glDisableVertexAttribArray(col_loc_);

    // leave the context clean for anyone else
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
  }
};
#endif

// ----- test group ---------------------------------------------------- MARK: -

class Ut_Gl_Test : public Fl_Group {
#if HAVE_GL
    Ut_Gl_Triangle_Window* gl_;
#endif
public:
  Ut_Gl_Test(int X, int Y, int W, int H) : Fl_Group(X, Y, W, H) {
    box(FL_NO_BOX);
    Fl_Box *info = new Fl_Box(X, Y, W, 24, "");
    info->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    info->labelsize(12);
#if HAVE_GL
    gl_ = new Ut_Gl_Triangle_Window(X, Y + 28, W, H - 28, info);
    gl_->border(0);
    gl_->end();
    resizable(gl_);
#endif
    end();
  }

#if HAVE_GL
    // This is needed for the unittests to not break.
    void show() FL_OVERRIDE
        {
            Fl_Group::show();
            gl_->show();
        }

    void hide() FL_OVERRIDE
        {
            gl_->hide();
            Fl_Group::hide();
        }
#endif
};

#else // !__EMSCRIPTEN__

class Ut_Gl_Test : public Fl_Group {
public:
  Ut_Gl_Test(int X, int Y, int W, int H) : Fl_Group(X, Y, W, H) {
    box(FL_NO_BOX);
    Fl_Box *b = new Fl_Box(X, Y, W, H,
      "The GL triangle test uses WebGL-compatible GLES2 shaders\n"
      "and is only built for the Emscripten platform.");
    b->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE);
    end();
  }
};

#endif // __EMSCRIPTEN__

static Fl_Widget *gl_test_create() {
  return new Ut_Gl_Test(UT_TESTAREA_X, UT_TESTAREA_Y, UT_TESTAREA_W, UT_TESTAREA_H);
}

UnitTest ut_gl(UT_TEST_GL, "OpenGL3", gl_test_create);
