// USAGE NOTES:
//
// - You must manually call glfwTerminate() at the end of your program.
//
// - Most functions return an error code. This will be 0 if no error
//   occurred, or a positive integer otherwise.
// 
// - Though the contents of the vtk2_win struct are public, this is primarily
//   to allow using struct vtk2_win as a value type. You shouldn't mess with
//   anything it contains unless you really know what you're doing.
//

#ifndef VTK2_H
#define VTK2_H

#include <math.h>
#include <stdint.h>
#include <GLFW/glfw3.h>
#include "deps/FlexLayout/src/FlexLayout.h"
#include "deps/nanovg/src/nanovg.h"

//// Error handling ////

enum vtk2_err {
	VTK2_ERR_SUCCESS,
	VTK2_ERR_ALLOC, // Allocation failed
	VTK2_ERR_PLATFORM, // GLFW platform error
};

// Return a string message for the specified error code.
const char *vtk2_strerror(enum vtk2_err err);
// Print a string message for the specified error to stderr, preceded by the prefix and a colon.
void vtk2_perror(const char *prefix, enum vtk2_err err);

//// Base API ////

struct vtk2_block;
struct vtk2_win;

// Create a new window with the specified title, width and height.
// Some default GLFW window hints will be set.
// If you want to change these, use vtk2_window_init_glfw instead.
// - GLFW_RESIZABLE = GLFW_TRUE
// - GLFW_CLIENT_API = GLFW_OPENGL_API
// - GLFW_CONTEXT_VERSION_MAJOR = 3
// - GLFW_CONTEXT_VERSION_MINOR = 3
// - GLFW_OPENGL_PROFILE = GLFW_OPENGL_CORE_PROFILE
enum vtk2_err vtk2_window_init(struct vtk2_win *win, const char *title, int w, int h);

// Create a new window from an existing GLFW window.
// If this function succeeds, the GLFW window becomes owned by the vtk2 window,
// and should not be destroyed except through vtk2_window_deinit.
enum vtk2_err vtk2_window_init_glfw(struct vtk2_win *win, GLFWwindow *glfw_win);

// Destroy the specified window, cleaning up all resources associated with it.
void vtk2_window_deinit(struct vtk2_win *win);

// Initialize a block and set it as the root block of the specified window.
// If this function succeeds, the block becomes owned by the window.
enum vtk2_err vtk2_window_set_root(struct vtk2_win *win, struct vtk2_block *root);

// Initialize a block and add it as a child of the specified parent.
// If this function succeeds, the block becomes owned by the window.
enum vtk2_err vtk2_window_add_child(struct vtk2_win *win, struct vtk2_block *parent, struct vtk2_block *child);

// Process events and redraws for the specified window until it is closed.
void vtk2_window_mainloop(struct vtk2_win *win);

//// Block settings ////
#define VTK2_BLOCK_SETTINGS \
	float grow, shrink; \
	float margins[4]; \
	float size[2]
#define VTK2_BLOCK_DEFAULTS \
	.grow = 0, .shrink = 1, \
	.margins = {0}, \
	.size = {NAN, NAN}
#pragma GCC diagnostic ignored "-Winitializer-overrides"

enum vtk2_direction {
	VTK2_ROW,
	VTK2_COL,
};
struct vtk2_box_settings {
	struct vtk2_block **children;
	enum vtk2_direction direction;
	VTK2_BLOCK_SETTINGS;
};
#define VTK2_BOX_DEFAULTS \
	.children = NULL, \
	.direction = VTK2_ROW

#define _vtk2_make(name, ...) _vtk2_make_##name((struct vtk2_##name##_settings){VTK2_BLOCK_DEFAULTS, __VA_ARGS__})

//// Block constructors ////
// THESE WILL ABORT IF ALLOCATION FAILS - USE ONCE AT PROGRAM START
struct vtk2_block *_vtk2_make_box(struct vtk2_box_settings settings);
#define vtk2_make_box(...) _vtk2_make(box, VTK2_BOX_DEFAULTS, __VA_ARGS__)

//// Type definitions (advanced users only) ////
struct vtk2_win {
	// Try not to mess with these directly
	_Bool damaged; // Does the window need redrawn?
	NVGcontext *vg;
	GLFWwindow *win;
	struct vtk2_block *focused;

	// Don't write to these
	float cx, cy; // Cursor pos
	unsigned fb_w, fb_h; // Framebuffer size
	float win_w, win_h; // Window size
	struct vtk2_block *root;
};

struct vtk2_block {
	// Public-ish fields - prefer constructors over directly accessing these
	float grow, shrink;
	float margins[4];
	float size[2];

	// Only touch this stuff if you're defining custom block types
	enum vtk2_err (*init)(struct vtk2_block *);
	void (*deinit)(struct vtk2_block *);
	void (*draw)(struct vtk2_block *);
	_Bool (*ev_button)(struct vtk2_block *, int button, int action, int mods);
	_Bool (*ev_enter)(struct vtk2_block *, _Bool entered);
	_Bool (*ev_key)(struct vtk2_block *, int key, int scancode, int action, int mods);
	_Bool (*ev_mouse)(struct vtk2_block *, float new_x, float new_y, float old_x, float old_y);
	_Bool (*ev_text)(struct vtk2_block *, unsigned rune);

	// Read-only, avoid using if possible
	FlexNodeRef flex;
	struct vtk2_win *win;
};

//// Internal block type definitions, don't touch except for language bindings ////
struct vtk2_b_box {
	struct vtk2_block base;
	enum vtk2_direction direction;
	struct vtk2_block **children;
};

//// Helpers ////
#define fieldParentPtr(T, field_name, value) ((T *)((char *)(value) - offsetof(T, field_name)))

#endif
