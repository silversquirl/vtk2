// USAGE NOTES:
//
// - You must manually call glfwTerminate() at the end of your program
//
// - Most functions return an error code. This will be 0 if no error
//   occurred, or a positive integer otherwise
// 
// - Though the contents of the vtk2_win struct are public, you probably
//   shouldn't mess with anything other than the GLFW window unless you
//   really know what you're doing
//

#ifndef VTK2_H
#define VTK2_H

#include <GLFW/glfw3.h>
#include "deps/layout/layout.h"
#include "deps/nanovg/src/nanovg.h"

enum vtk2_err {
	VTK2_ERR_SUCCESS,
	VTK2_ERR_ALLOC, // Allocation failed
	VTK2_ERR_PLATFORM, // GLFW platform error
};

// Return a string message for the specified error code
const char *vtk2_strerror(enum vtk2_err err);
// Print a string message for the specified error to stderr, preceded by the prefix and a colon
void vtk2_perror(const char *prefix, enum vtk2_err err);

struct vtk2_win {
	lay_context ctx;
	NVGcontext *vg;
	GLFWwindow *win;
};

// Create a new window with the specified title, width and height
// Some default GLFW window hints will be set.
// If you want to change these, use vtk2_window_init_glfw instead.
// - GLFW_RESIZABLE = GLFW_TRUE
// - GLFW_CLIENT_API = GLFW_OPENGL_API
// - GLFW_CONTEXT_VERSION_MAJOR = 3
// - GLFW_CONTEXT_VERSION_MINOR = 3
// - GLFW_OPENGL_PROFILE = GLFW_OPENGL_CORE_PROFILE
enum vtk2_err vtk2_window_init(struct vtk2_win *win, const char *title, int w, int h);

// Create a new window from an existing GLFW window.
// If this function succeeds, the GLFW window will become owned by the vtk2 window,
// and should not be destroyed except through vtk2_window_deinit.
enum vtk2_err vtk2_window_init_glfw(struct vtk2_win *win, GLFWwindow *glfw_win);

// Destroy the specified window, cleaning up all resources associated with it
void vtk2_window_deinit(struct vtk2_win *win);

#endif
