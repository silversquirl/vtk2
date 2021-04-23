#include <stddef.h>
#include <stdio.h>

#include <epoxy/gl.h>
#include <GLFW/glfw3.h>

#define LAY_IMPLEMENTATION
#define LAY_FLOAT 1

#if defined(NANOVG_GL2_IMPLEMENTATION) 
#	define NVG_IMPL GL2 
#elif defined(NANOVG_GLES2_IMPLEMENTATION)
#	define NVG_IMPL GLES2 
#elif defined(NANOVG_GLES3_IMPLEMENTATION)
#	define NVG_IMPL GLES3 
#else // NANOVG_GL3_IMPLEMENTATION
#	define NANOVG_GL3_IMPLEMENTATION
#	define NVG_IMPL GL3 
#endif

#define SPLAT_(a, b) a##b
#define SPLAT(a, b) SPLAT_(a, b)
#define nvgCreate SPLAT(nvgCreate, NVG_IMPL)
#define nvgDelete SPLAT(nvgDelete, NVG_IMPL)

#include "vtk2.h"
#include "deps/nanovg/src/nanovg_gl.h"



//// Event handlers ////
void _vtk2_ev_button(GLFWwindow *glfw_win, int button, int action, int mods) {
	struct vtk2_win *win = glfwGetWindowUserPointer(glfw_win);
}
void _vtk2_ev_enter(GLFWwindow *glfw_win, int entered) {
	struct vtk2_win *win = glfwGetWindowUserPointer(glfw_win);
}
void _vtk2_ev_key(GLFWwindow *glfw_win, int key, int scan, int action, int mods) {
	struct vtk2_win *win = glfwGetWindowUserPointer(glfw_win);
}
void _vtk2_ev_mouse(GLFWwindow *glfw_win, double x, double y) {
	struct vtk2_win *win = glfwGetWindowUserPointer(glfw_win);
}
void _vtk2_ev_resize(GLFWwindow *glfw_win, int fb_w, int fb_h) {
	struct vtk2_win *win = glfwGetWindowUserPointer(glfw_win);

	// Damage window
	win->damaged = 1;

	// Store framebuffer dimensions
	win->fb_w = fb_w;
	win->fb_h = fb_h;

	// Store window dimensions
	int w, h;
	glfwGetWindowSize(win->win, &w, &h);
	win->win_w = w;
	win->win_h = h;

	// Set OpenGL viewport
	glViewport(0, 0, win->fb_w, win->fb_h);

	// Resize root block
	lay_set_size_xy(&win->lay, 0, win->win_w, win->win_h);
}

//// Drawing ////
void _vtk2_window_draw(struct vtk2_win *win) {
	if (!win->damaged) return;
	win->damaged = 0;

	// Calculate block layout
	lay_run_context(&win->lay);

	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);

	// TODO: draw blocks

	glfwSwapBuffers(win->win);
}

//// Error handling ////
const char *vtk2_strerror(enum vtk2_err err) {
	switch (err) {
	case VTK2_ERR_SUCCESS:
		return NULL;
	case VTK2_ERR_ALLOC:
		return "allocation failed";
	case VTK2_ERR_PLATFORM:
		return "GLFW platform error";
	}
}

void vtk2_perror(const char *prefix, enum vtk2_err err) {
	fprintf(stderr, "%s: %s\n", prefix, vtk2_strerror(err));
}

//// Windowing API ////
enum vtk2_err vtk2_window_init(struct vtk2_win *win, const char *title, int w, int h) {
	glfwInit();

	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow *glfw_win = glfwCreateWindow(w, h, title, NULL, NULL);
	if (!glfw_win) return VTK2_ERR_PLATFORM;

	enum vtk2_err err = vtk2_window_init_glfw(win, glfw_win);
	if (err) {
		glfwDestroyWindow(glfw_win);
	}
	return err;
}

enum vtk2_err vtk2_window_init_glfw(struct vtk2_win *win, GLFWwindow *glfw_win) {
	// Start damaged, since we've not drawn anything yet
	win->damaged = 1;

	// Setup window
	win->win = glfw_win;
	glfwSetWindowUserPointer(win->win, win);
	glfwMakeContextCurrent(win->win);

	// Create nanovg context
	win->vg = nvgCreate(0);
	if (!win->vg) {
		return VTK2_ERR_ALLOC;
	}

	// Create layout context
	lay_init_context(&win->lay);
	lay_reserve_items_capacity(&win->lay, 32);
	if (!win->lay.items) {
		nvgDelete(win->vg);
		return VTK2_ERR_ALLOC;
	}

	// Create root block
	lay_item(&win->lay);

	// Set up event handlers
	glfwSetCursorEnterCallback(win->win, _vtk2_ev_enter);
	glfwSetCursorPosCallback(win->win, _vtk2_ev_mouse);
	glfwSetFramebufferSizeCallback(win->win, _vtk2_ev_resize);
	glfwSetKeyCallback(win->win, _vtk2_ev_key);
	glfwSetMouseButtonCallback(win->win, _vtk2_ev_button);

	// Set initial size
	int fb_w, fb_h;
	glfwGetFramebufferSize(win->win, &fb_w, &fb_h);
	_vtk2_ev_resize(win->win, fb_w, fb_h);

	return 0;
}

void vtk2_window_deinit(struct vtk2_win *win) {
	lay_destroy_context(&win->lay);
	nvgDelete(win->vg);
	glfwDestroyWindow(win->win);
}

//// Main loop ////
void vtk2_window_mainloop(struct vtk2_win *win) {
	while (!glfwWindowShouldClose(win->win)) {
		_vtk2_window_draw(win);
		glfwWaitEvents();
	}
}
