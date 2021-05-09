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
#include "deps/nanovg/src/nanovg_gl.h"
#include "deps/nanovg/src/nanovg.c"

#define UNPACK_4(a) (a)[0], UNPACK_3((a)+1)
#define UNPACK_3(a) (a)[0], UNPACK_2((a)+1)
#define UNPACK_2(a) (a)[0], (a)[1]

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
	vtk2_block_layout(win->root, (float [4]){0, 0, win->win_w, win->win_h}, VTK2_SHRINK_NONE);

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
}

void vtk2_window_deinit(struct vtk2_win *win) {
	_vtk2_block_deinit(win->root);
	nvgDelete(win->vg);
	glfwDestroyWindow(win->win);
}

enum vtk2_err vtk2_window_set_root(struct vtk2_win *win, struct vtk2_block *root) {
	_vtk2_block_deinit(win->root);

	// Initialize root block
	enum vtk2_err err = vtk2_block_init(win, root);
	if (err == 0) {
		win->root = root;
	}

	return err;
}

//// Main loop ////
void vtk2_window_mainloop(struct vtk2_win *win) {
	while (!glfwWindowShouldClose(win->win)) {
		_vtk2_window_draw(win);
		glfwWaitEvents();
	}
}

//// Block functions ////
enum vtk2_err vtk2_block_init(struct vtk2_win *win, struct vtk2_block *block) {
	block->win = win;

	enum vtk2_err err = 0;
	if (block->init) {
		err = block->init(block);
	}
	return err;
}

static inline float _vtk2_forf(float a, float b) {
	return isnan(a) ? b : a;
}
static void _vtk2_block_constrain(struct vtk2_block *block) {
	float w = block->rect[2], h = block->rect[3];

	if (block->grow == 0) {
		w = _vtk2_forf(block->size[0], w);
		h = _vtk2_forf(block->size[1], h);
	} else {
		w = fmaxf(block->size[0], w);
		h = fmaxf(block->size[1], h);
	}

	block->rect[2] = fmaxf(0, w);
	block->rect[3] = fmaxf(0, h);
}

void vtk2_block_layout(struct vtk2_block *block, float rect[4], enum vtk2_shrink shrink) {
	if (!block) return;

	// Compute margins
	float mx = block->margins[0];
	float my = block->margins[1];
	float mw = block->margins[2] + mx;
	float mh = block->margins[3] + my;

	// Set initial rect size
	block->rect[0] = rect[0] + mx;
	block->rect[1] = rect[1] + my;
	block->rect[2] = fmaxf(0, rect[2] - mw);
	block->rect[3] = fmaxf(0, rect[3] - mh);

	// Compute layout
	if (block->layout) {
		block->layout(block, shrink);
	} else {
		// Default, very simple sizing algorithm
		_vtk2_block_constrain(block);
	}
}

//// Box block ////
static enum vtk2_err _vtk2_box_init(struct vtk2_block *base) {
	struct vtk2_b_box *box = fieldParentPtr(struct vtk2_b_box, base, base);

	for (struct vtk2_block **child = box->children; child && *child; child++) {
		enum vtk2_err err = vtk2_block_init(box->base.win, *child);
		if (err) return err;
	}
	return 0;
}

static void _vtk2_box_deinit(struct vtk2_block *base) {
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

static void _vtk2_box_draw(struct vtk2_block *base) {
	struct vtk2_b_box *box = fieldParentPtr(struct vtk2_b_box, base, base);

#ifdef VTK2_BOX_DEBUG
	struct vtk2_win *win = box->base.win;
	uint64_t color = splitmix64((uint64_t)&box->base);
	uint8_t r = (color >> 0) & 0xff;
	uint8_t g = (color >> 8) & 0xff;
	uint8_t b = (color >> 16) & 0xff;
	uint8_t a = 100;

	nvgBeginPath(win->vg);
	nvgRect(win->vg, box->base.rect[0], box->base.rect[1], box->base.rect[2], box->base.rect[3]);
	nvgFillColor(win->vg, nvgRGBA(r, g, b, a));
	nvgFill(win->vg);
	nvgStrokeColor(win->vg, nvgRGBA(255, 255, 255, 150));
	nvgStrokeWidth(win->vg, 1);
	nvgStroke(win->vg);
#endif

	for (struct vtk2_block **child = box->children; child && *child; child++) {
		if ((*child)->draw) {
			(*child)->draw(*child);
		}
	}
}

static inline float _vtk2_block_dimsize(struct vtk2_block *block, int dim) {
	return block->rect[2 + dim] + block->margins[dim] + block->margins[2 + dim];
}
// FIXME: This is O(2^n) on nesting depth. That is bad, and I should feel bad
static void _vtk2_box_layout(struct vtk2_block *base, enum vtk2_shrink shrink) {
	struct vtk2_b_box *box = fieldParentPtr(struct vtk2_b_box, base, base);

	int dim = box->direction;
	enum vtk2_shrink box_shrink = VTK2_SHRINK_X + dim;

	_vtk2_block_constrain(&box->base);

	// Compute minimum sizes
	float rect[4] = {UNPACK_4(box->base.rect)};
	float cross = 0, grow_total = 0;
	for (struct vtk2_block **child = box->children; child && *child; child++) {
		vtk2_block_layout(*child, rect, box_shrink);

		float csize = _vtk2_block_dimsize(*child, dim);
		rect[dim] += csize;
		rect[2 + dim] -= csize;

		cross = fmax(cross, _vtk2_block_dimsize(*child, 1 - dim));
		grow_total += (*child)->grow;
	}

	if (shrink == box_shrink) {
		float *size = &box->base.rect[2 + dim];
		*size -= fmaxf(0, rect[2 + dim]); // Remove leftover space
		_vtk2_block_constrain(&box->base);
	} else {
		if (shrink != VTK2_SHRINK_NONE) {
			// Shrink cross size
			box->base.rect[3 - dim] = fmin(box->base.rect[3 - dim], cross);
			_vtk2_block_constrain(&box->base);
		}

		// Divide leftover space
		float unit = grow_total == 0 ? 0 : rect[2 + dim] / grow_total;

		memcpy(rect, box->base.rect, sizeof rect);
		for (struct vtk2_block **child = box->children; child && *child; child++) {
			// Set rect size, allocating extra space based on grow factor
			rect[2 + dim] = _vtk2_block_dimsize(*child, dim) + unit * (*child)->grow;

			vtk2_block_layout(*child, rect, shrink);
			rect[dim] += _vtk2_block_dimsize(*child, dim);
		}
	}
}

static struct vtk2_block *_vtk2_box_child(struct vtk2_b_box *box, float x, float y) {
	for (struct vtk2_block **child = box->children; child && *child; child++) {
		float x0 = (*child)->rect[0];
		float y0 = (*child)->rect[1];
		float x1 = x0 + (*child)->rect[2];
		float y1 = y0 + (*child)->rect[3];

		if (x0 <= x && x < x1 && y0 <= y && y < y1) {
			return *child;
		}
	}

	return NULL;
}

static _Bool _vtk2_box_ev_button(struct vtk2_block *base, int button, int action, int mods) {
	struct vtk2_b_box *box = fieldParentPtr(struct vtk2_b_box, base, base);
	struct vtk2_block *child = _vtk2_box_child(box, box->base.win->cx, box->base.win->cy);
	if (child && child->ev_button) {
		return child->ev_button(child, button, action, mods);
	}
	return false;
}

static _Bool _vtk2_box_ev_enter(struct vtk2_block *base, _Bool entered) {
	struct vtk2_b_box *box = fieldParentPtr(struct vtk2_b_box, base, base);
	struct vtk2_block *child = _vtk2_box_child(box, box->base.win->cx, box->base.win->cy);
	if (child && child->ev_enter) {
		return child->ev_enter(child, entered);
	}
	return false;
}

static _Bool _vtk2_box_ev_key(struct vtk2_block *base, int key, int scancode, int action, int mods) {
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

static _Bool _vtk2_box_ev_mouse(struct vtk2_block *base, float new_x, float new_y, float old_x, float old_y) {
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

static _Bool _vtk2_box_ev_text(struct vtk2_block *base, unsigned rune) {
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

struct vtk2_block *_vtk2_make_box(struct vtk2_box_settings settings) {
	struct vtk2_b_box *box = malloc(sizeof *box);
	if (!box) abort();
	*box = (struct vtk2_b_box){
		.children = settings.children,
		.direction = settings.direction,
		.base = (struct vtk2_block){
			.grow = settings.grow,
			.margins = {UNPACK_4(settings.margins)},
			.size = {UNPACK_2(settings.size)},

			.init = _vtk2_box_init,
			.deinit = _vtk2_box_deinit,
			.draw = _vtk2_box_draw,
			.layout = _vtk2_box_layout,
			.ev_button = _vtk2_box_ev_button,
			.ev_enter = _vtk2_box_ev_enter,
			.ev_key = _vtk2_box_ev_key,
			.ev_mouse = _vtk2_box_ev_mouse,
			.ev_text = _vtk2_box_ev_text,
		},
	};
	return &box->base;
}
