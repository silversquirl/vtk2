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
	win->win = glfw_win;
	glfwMakeContextCurrent(win->win);

	win->vg = nvgCreate(0);
	if (!win->vg) {
		return VTK2_ERR_ALLOC;
	}

	lay_init_context(&win->ctx);
	lay_reserve_items_capacity(&win->ctx, 32);
	if (!win->ctx.items) {
		nvgDelete(win->vg);
		return VTK2_ERR_ALLOC;
	}

	return 0;
}

void vtk2_window_deinit(struct vtk2_win *win) {
	lay_destroy_context(&win->ctx);
	nvgDelete(win->vg);
	glfwDestroyWindow(win->win);
}
