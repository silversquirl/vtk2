#include <stddef.h>
#include <stdio.h>

#include <epoxy/gl.h>
#include <GLFW/glfw3.h>

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
#include "deps/FlexLayout/src/FlexLayout.c"
#include "deps/nanovg/src/nanovg_gl.h"
#include "deps/nanovg/src/nanovg.c"

//// Event handlers ////
static void _vtk2_ev_button(GLFWwindow *glfw_win, int button, int action, int mods) {
	struct vtk2_win *win = glfwGetWindowUserPointer(glfw_win);
	if (win->root && win->root->ev_button) {
		win->root->ev_button(win->root, button, action, mods);
	}
}
static void _vtk2_ev_damage(GLFWwindow *glfw_win) {
	struct vtk2_win *win = glfwGetWindowUserPointer(glfw_win);
	win->damaged = 1;
}
static void _vtk2_ev_enter(GLFWwindow *glfw_win, int entered) {
	struct vtk2_win *win = glfwGetWindowUserPointer(glfw_win);
	if (win->root && win->root->ev_enter) {
		win->root->ev_enter(win->root, entered);
	}
}
static void _vtk2_ev_key(GLFWwindow *glfw_win, int key, int scancode, int action, int mods) {
	struct vtk2_win *win = glfwGetWindowUserPointer(glfw_win);
	if (win->root && win->root->ev_key) {
		win->root->ev_key(win->root, key, scancode, action, mods);
	}
}
static void _vtk2_ev_mouse(GLFWwindow *glfw_win, double x, double y) {
	struct vtk2_win *win = glfwGetWindowUserPointer(glfw_win);
	if (win->root && win->root->ev_mouse) {
		win->root->ev_mouse(win->root, x, y, win->cx, win->cy);
	}
	win->cx = x;
	win->cy = y;
}
static void _vtk2_ev_resize(GLFWwindow *glfw_win, int fb_w, int fb_h) {
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
}
static void _vtk2_ev_text(GLFWwindow *glfw_win, unsigned rune) {
	struct vtk2_win *win = glfwGetWindowUserPointer(glfw_win);
	if (win->root && win->root->ev_text) {
		win->root->ev_text(win->root, rune);
	}
}

//// Drawing ////
static void _vtk2_window_draw(struct vtk2_win *win) {
	if (!win->damaged) return;
	win->damaged = 0;

	float fb_scale = win->win_w / (float)win->fb_w;

	// Calculate block layout
	if (win->root) {
		Flex_layout(win->root->flex, win->win_w, win->win_h, fb_scale);
	}

	glViewport(0, 0, win->fb_w, win->fb_h);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);
	nvgBeginFrame(win->vg, win->win_w, win->win_h, fb_scale);

	if (win->root && win->root->draw) {
		win->root->draw(win->root);
	}

	nvgEndFrame(win->vg);
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

	// Initialize internal properties
	win->cy = win->cx = NAN;
	win->root = win->focused = NULL;

	// Set up event handlers
	glfwSetCursorEnterCallback(win->win, _vtk2_ev_enter);
	glfwSetCursorPosCallback(win->win, _vtk2_ev_mouse);
	glfwSetFramebufferSizeCallback(win->win, _vtk2_ev_resize);
	glfwSetKeyCallback(win->win, _vtk2_ev_key);
	glfwSetMouseButtonCallback(win->win, _vtk2_ev_button);
	glfwSetWindowRefreshCallback(win->win, _vtk2_ev_damage);

	// Set initial size
	int fb_w, fb_h;
	glfwGetFramebufferSize(win->win, &fb_w, &fb_h);
	_vtk2_ev_resize(win->win, fb_w, fb_h);

	return 0;
}

static void _vtk2_block_deinit(struct vtk2_block *block) {
	if (!block) return;
	if (block->deinit) block->deinit(block);
	Flex_freeNodeRecursive(block->flex);
}

void vtk2_window_deinit(struct vtk2_win *win) {
	_vtk2_block_deinit(win->root);
	nvgDelete(win->vg);
	glfwDestroyWindow(win->win);
}

static enum vtk2_err _vtk2_block_init(struct vtk2_win *win, struct vtk2_block *block) {
	block->win = win;

	Flex_setFlexGrow(block->flex, block->grow);
	Flex_setFlexShrink(block->flex, block->shrink);

	Flex_setMarginTop(block->flex, block->margins[0]);
	Flex_setMarginRight(block->flex, block->margins[1]);
	Flex_setMarginBottom(block->flex, block->margins[2]);
	Flex_setMarginLeft(block->flex, block->margins[3]);

	Flex_setWidth(block->flex, block->size[0]);
	Flex_setHeight(block->flex, block->size[1]);

	enum vtk2_err err = 0;
	if (block->init) {
		err = block->init(block);
	}
	return err;
}

enum vtk2_err vtk2_window_set_root(struct vtk2_win *win, struct vtk2_block *root) {
	_vtk2_block_deinit(win->root);

	// Create flex node
	root->flex = Flex_newNode();
	// FIXME: FlexLayout does not handle malloc failure, and will segfault before this check
	if (!root->flex) return VTK2_ERR_ALLOC;

	// Initialize root block
	enum vtk2_err err = _vtk2_block_init(win, root);
	if (err == 0) {
		win->root = root;
	}

	return err;
}

enum vtk2_err vtk2_window_add_child(struct vtk2_win *win, struct vtk2_block *parent, struct vtk2_block *child) {
	// Create flex node
	child->flex = Flex_newNode();
	// FIXME: FlexLayout does not handle malloc failure, and will segfault before this check
	if (!child->flex) return VTK2_ERR_ALLOC;

	// Initialize the block
	enum vtk2_err err = _vtk2_block_init(win, child);
	if (err) {
		return err;
	}

	Flex_addChild(parent->flex, child->flex);

	return 0;
}

//// Main loop ////
void vtk2_window_mainloop(struct vtk2_win *win) {
	while (!glfwWindowShouldClose(win->win)) {
		_vtk2_window_draw(win);
		glfwWaitEvents();
	}
}

//// Box block ////
enum vtk2_err _vtk2_box_init(struct vtk2_block *base) {
	struct vtk2_b_box *box = fieldParentPtr(struct vtk2_b_box, base, base);
	Flex_setDirection(box->base.flex, (FlexDirection)box->direction);
	for (struct vtk2_block **child = box->children; child && *child; child++) {
		enum vtk2_err err = vtk2_window_add_child(box->base.win, &box->base, *child);
		if (err) return err;
	}
	return 0;
}

void _vtk2_box_deinit(struct vtk2_block *base) {
	struct vtk2_b_box *box = fieldParentPtr(struct vtk2_b_box, base, base);
	for (struct vtk2_block **child = box->children; child && *child; child++) {
		if ((*child)->deinit) {
			(*child)->deinit(*child);
		}
	}
}

#ifdef VTK2_BOX_DEBUG
// SplitMix64
static uint64_t splitmix64(uint64_t x) {
	x += 0x9e3779b97f4a7c15;
	x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9;
	x = (x ^ (x >> 27)) * 0x94d049bb133111eb;
	return x ^ (x >> 31);
}
#endif

void _vtk2_box_draw(struct vtk2_block *base) {
	struct vtk2_b_box *box = fieldParentPtr(struct vtk2_b_box, base, base);

#ifdef VTK2_BOX_DEBUG
	struct vtk2_win *win = box->base.win;
	float rect[4] = {
		Flex_getResultLeft(box->base.flex),
		Flex_getResultTop(box->base.flex),
		Flex_getResultWidth(box->base.flex),
		Flex_getResultHeight(box->base.flex),
	};
	uint64_t color = splitmix64((uint64_t)box->base.flex);
	uint8_t r = (color >> 0) & 0xff;
	uint8_t g = (color >> 8) & 0xff;
	uint8_t b = (color >> 16) & 0xff;
	uint8_t a = (color >> 24) & 0xff;

	nvgBeginPath(win->vg);
	nvgRect(win->vg, rect[0], rect[1], rect[2], rect[3]);
	nvgFillColor(win->vg, nvgRGBA(r, g, b, a));
	nvgFill(win->vg);
	nvgStrokeColor(win->vg, nvgRGBA(255, 255, 255, 255));
	nvgStrokeWidth(win->vg, 3);
	nvgStroke(win->vg);
#endif

	for (struct vtk2_block **child = box->children; child && *child; child++) {
		if ((*child)->draw) {
			(*child)->draw(*child);
		}
	}
}

struct vtk2_block *_vtk2_box_child(struct vtk2_b_box *box, float x, float y) {
	for (struct vtk2_block **child = box->children; child && *child; child++) {
		float x0 = Flex_getResultLeft((*child)->flex);
		float y0 = Flex_getResultTop((*child)->flex);
		float x1 = x0 + Flex_getResultWidth((*child)->flex);
		float y1 = y0 + Flex_getResultHeight((*child)->flex);

		if (x0 <= x && x < x1 && y0 <= y && y < y1) {
			return *child;
		}
	}

	return NULL;
}

_Bool _vtk2_box_ev_button(struct vtk2_block *base, int button, int action, int mods) {
	struct vtk2_b_box *box = fieldParentPtr(struct vtk2_b_box, base, base);
	struct vtk2_block *child = _vtk2_box_child(box, box->base.win->cx, box->base.win->cy);
	if (child && child->ev_button) {
		return child->ev_button(child, button, action, mods);
	}
	return false;
}

_Bool _vtk2_box_ev_enter(struct vtk2_block *base, _Bool entered) {
	struct vtk2_b_box *box = fieldParentPtr(struct vtk2_b_box, base, base);
	struct vtk2_block *child = _vtk2_box_child(box, box->base.win->cx, box->base.win->cy);
	if (child && child->ev_enter) {
		return child->ev_enter(child, entered);
	}
	return false;
}

_Bool _vtk2_box_ev_key(struct vtk2_block *base, int key, int scancode, int action, int mods) {
	struct vtk2_b_box *box = fieldParentPtr(struct vtk2_b_box, base, base);
	if (!box->base.win->focused) {
		box->base.win->focused = _vtk2_box_child(box, box->base.win->cx, box->base.win->cy);
	}
	struct vtk2_block *focused = box->base.win->focused;
	if (focused && focused != base && focused->ev_key) {
		return focused->ev_key(focused, key, scancode, action, mods);
	}
	return false;
}

_Bool _vtk2_box_ev_mouse(struct vtk2_block *base, float new_x, float new_y, float old_x, float old_y) {
	struct vtk2_b_box *box = fieldParentPtr(struct vtk2_b_box, base, base);
	struct vtk2_block *child = _vtk2_box_child(box, new_x, new_y);
	struct vtk2_block *old_child = _vtk2_box_child(box, old_x, old_y);
	if (child != old_child && old_child && old_child->ev_enter) {
		old_child->ev_enter(old_child, 0);
	}
	if (child && child->ev_mouse) {
		return child->ev_mouse(child, new_x, new_y, old_x, old_y);
	}
	if (child != old_child && child && child->ev_enter) {
		child->ev_enter(child, 1);
	}
	return false;
}

_Bool _vtk2_box_ev_text(struct vtk2_block *base, unsigned rune) {
	struct vtk2_b_box *box = fieldParentPtr(struct vtk2_b_box, base, base);
	if (!box->base.win->focused) {
		box->base.win->focused = _vtk2_box_child(box, box->base.win->cx, box->base.win->cy);
	}
	struct vtk2_block *focused = box->base.win->focused;
	if (focused && focused != base && focused->ev_text) {
		return focused->ev_text(focused, rune);
	}
	return false;
}

#define unpack_4(a) (a)[0], unpack_3((a)+1)
#define unpack_3(a) (a)[0], unpack_2((a)+1)
#define unpack_2(a) (a)[0], (a)[1]

struct vtk2_block *_vtk2_make_box(struct vtk2_box_settings settings) {
	struct vtk2_b_box *box = malloc(sizeof *box);
	if (!box) abort();
	*box = (struct vtk2_b_box){
		.children = settings.children,
		.direction = settings.direction,
		.base = (struct vtk2_block){
			.grow = settings.grow,
			.shrink = settings.shrink,
			.margins = {unpack_4(settings.margins)},
			.size = {unpack_2(settings.size)},

			.init = _vtk2_box_init,
			.deinit = _vtk2_box_deinit,
			.draw = _vtk2_box_draw,
			.ev_button = _vtk2_box_ev_button,
			.ev_enter = _vtk2_box_ev_enter,
			.ev_key = _vtk2_box_ev_key,
			.ev_mouse = _vtk2_box_ev_mouse,
			.ev_text = _vtk2_box_ev_text,
		},
	};
	return &box->base;
}
