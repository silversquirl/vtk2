// vtk2 - a lightweight, minimal GUI toolkit
// https://github.com/vktec/vtk2

// This is free and unencumbered software released into the public domain.
//
// Anyone is free to copy, modify, publish, use, compile, sell, or
// distribute this software, either in source code form or as a compiled
// binary, for any purpose, commercial or non-commercial, and by any
// means.
//
// In jurisdictions that recognize copyright laws, the author or authors
// of this software dedicate any and all copyright interest in the
// software to the public domain. We make this dedication for the benefit
// of the public at large and to the detriment of our heirs and
// successors. We intend this dedication to be an overt act of
// relinquishment in perpetuity of all present and future rights to this
// software under copyright law.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//
// For more information, please refer to <http://unlicense.org/>

// vtk2 embeds a modified version of Aileron Regular, created by Sora Sagano
// http://dotcolon.net/font/aileron/

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
	case VTK2_ERR_LOAD_FAILED:
		return "load failed";
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
	uint64_t color = splitmix64(splitmix64((uint64_t)&box->base));
	uint8_t r = (color >> 0) & 0xff;
	uint8_t g = (color >> 8) & 0xff;
	uint8_t b = (color >> 16) & 0xff;
	uint8_t a = 255;

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

//// Text block ////
const char aileron_data[];
const size_t aileron_size;
static enum vtk2_err _vtk2_text_init(struct vtk2_block *base) {
	struct vtk2_b_text *text = fieldParentPtr(struct vtk2_b_text, base, base);

	const char *font_name, *font_data = NULL;
	size_t data_size;
	if (text->font_file) {
		font_name = text->font_file;
		if (text->font_data) {
			font_data = text->font_data;
			data_size = text->data_size;
		}
	} else {
		font_name = "\xff_vtk2_font_aileron";
		font_data = aileron_data;
		data_size = aileron_size;
	}

	NVGcontext *vg = text->base.win->vg;
	text->font_handle = nvgFindFont(vg, font_name);
	if (text->font_handle == -1) {
		if (font_data) {
			text->font_handle = nvgCreateFontMem(vg, font_name, (unsigned char *)font_data, data_size, 0);
		} else {
			text->font_handle = nvgCreateFont(vg, font_name, font_name);
		}
		if (text->font_handle == -1) {
			return VTK2_ERR_LOAD_FAILED;
		}
	}

	return 0;
}

static void _vtk2_text_layout(struct vtk2_block *base, enum vtk2_shrink shrink) {
	struct vtk2_b_text *text = fieldParentPtr(struct vtk2_b_text, base, base);
	NVGcontext *vg = text->base.win->vg;

	nvgFontFaceId(vg, text->font_handle);
	nvgFontSize(vg, text->scale);

	float ascend;
	nvgTextMetrics(vg, &ascend, NULL, NULL);

	float rect[4];
	nvgTextBounds(vg, text->base.rect[0], text->base.rect[1] + ascend, text->text, NULL, rect);

	text->base.rect[2] = rect[2] - rect[0];
	text->base.rect[3] = rect[3] - rect[1];

	_vtk2_block_constrain(&text->base);
}

static void _vtk2_text_draw(struct vtk2_block *base) {
	struct vtk2_b_text *text = fieldParentPtr(struct vtk2_b_text, base, base);

	NVGcontext *vg = text->base.win->vg;
	nvgFontFaceId(vg, text->font_handle);
	nvgFontSize(vg, text->scale);
	nvgFillColor(vg, nvgRGBAf(UNPACK_4(text->color)));

	float ascend;
	nvgTextMetrics(vg, &ascend, NULL, NULL);

	nvgText(vg, text->base.rect[0], text->base.rect[1] + ascend, text->text, NULL);
}

struct vtk2_block *_vtk2_make_text(struct vtk2_text_settings settings) {
	struct vtk2_b_text *text = malloc(sizeof *text);
	if (!text) abort();
	*text = (struct vtk2_b_text){
		.text = settings.text,
		.scale = settings.scale,
		.color = {UNPACK_4(settings.color)},

		.font_file = settings.font_file,
		.font_data = settings.font_data,
		.data_size = settings.data_size,
		.font_handle = -1,

		.base = (struct vtk2_block){
			.grow = settings.grow,
			.margins = {UNPACK_4(settings.margins)},
			.size = {UNPACK_2(settings.size)},

			.init = _vtk2_text_init,
			.draw = _vtk2_text_draw,
			.layout = _vtk2_text_layout,
		},
	};
	return &text->base;
}

// Aileron Regular //
const char aileron_data[] = {
	79,84,84,79,0,12,0,128,0,3,0,64,67,70,70,32,81,71,116,38,0,0,0,212,0,0,22,111,71,68,69,70,0,17,0,53,
	0,0,26,80,0,0,0,22,71,80,79,83,0,25,0,12,0,0,26,104,0,0,0,16,71,83,85,66,0,25,0,12,0,0,26,120,0,0,0,
	16,79,83,47,50,104,183,135,107,0,0,25,32,0,0,0,96,99,109,97,112,8,96,7,70,0,0,25,136,0,0,0,166,104,101,
	97,100,2,220,157,78,0,0,23,68,0,0,0,54,104,104,101,97,7,93,3,39,0,0,24,252,0,0,0,36,104,109,116,120,
	195,16,19,145,0,0,23,124,0,0,1,128,109,97,120,112,0,96,80,0,0,0,0,204,0,0,0,6,110,97,109,101,0,6,0,0,0,
	0,25,128,0,0,0,6,112,111,115,116,255,184,0,50,0,0,26,48,0,0,0,32,0,0,80,0,0,96,0,0,1,0,4,2,0,1,1,1,16,
	65,105,108,101,114,111,110,45,82,101,103,117,108,97,114,0,1,1,1,35,248,27,0,248,28,1,248,29,2,248,24,4,
	251,42,251,115,250,186,250,19,5,247,5,15,145,29,0,0,22,105,18,247,108,17,0,3,1,1,8,27,42,48,48,49,46,49,
	48,50,78,111,32,82,105,103,104,116,115,32,82,101,115,101,114,118,101,100,46,65,105,108,101,114,111,110,
	32,82,101,103,117,108,97,114,0,0,1,0,34,25,0,66,25,0,17,9,0,11,0,0,61,0,0,27,0,0,13,0,0,2,0,0,4,0,0,15,
	0,0,32,0,0,3,0,0,104,0,0,28,0,0,16,0,0,64,0,0,92,0,0,94,0,0,60,0,0,62,0,0,9,1,0,14,0,0,1,0,0,5,0,0,95,
	0,0,30,1,0,29,0,0,6,0,0,12,0,0,7,0,0,63,0,0,33,0,0,93,0,0,124,0,0,96,2,0,1,0,4,0,46,0,169,0,253,1,81,
	1,108,1,131,1,216,1,243,1,255,2,58,2,96,2,112,2,181,2,224,3,41,3,120,3,220,4,69,4,157,4,176,4,245,5,
	17,5,71,5,116,5,150,5,180,6,26,6,104,6,166,6,240,7,65,7,122,7,243,8,51,8,86,8,152,8,189,8,234,9,76,9,
	136,9,196,10,17,10,93,10,133,10,215,11,14,11,73,11,100,11,150,11,198,11,252,12,27,12,94,12,123,12,188,
	13,28,13,71,13,151,13,244,14,12,14,111,14,203,14,251,15,14,15,58,15,74,15,112,15,198,15,222,16,49,16,
	80,16,98,16,135,16,153,16,165,17,16,17,122,17,142,17,160,17,206,17,251,18,8,18,11,18,134,18,184,18,205,
	18,234,19,5,19,108,19,133,19,247,20,18,20,173,20,187,20,205,251,7,14,163,248,92,247,136,21,221,251,136,
	5,232,6,251,130,249,70,5,251,8,6,251,140,253,70,5,231,6,220,247,136,5,163,209,21,246,247,200,239,251,
	200,5,14,151,248,82,248,3,21,210,163,185,183,222,26,237,78,194,251,1,154,30,143,106,115,139,106,27,251,
	95,253,70,247,114,6,171,161,139,144,173,31,230,153,226,197,247,18,26,234,88,187,48,160,30,251,152,247,
	145,21,247,16,6,155,160,139,137,157,31,201,132,183,107,71,26,78,113,97,68,128,30,136,119,122,138,121,27,
	251,25,6,247,89,251,213,21,138,126,120,138,123,27,251,41,247,145,247,35,6,155,155,138,136,162,31,205,
	130,185,108,54,26,64,92,98,73,133,30,14,223,248,9,249,84,21,251,88,251,22,251,44,251,102,251,94,247,4,
	251,46,247,93,247,74,232,247,1,247,17,152,31,140,149,5,53,6,137,129,5,40,119,78,72,251,7,27,251,28,53,
	247,14,247,62,247,59,226,247,15,247,33,247,7,200,72,40,159,31,141,129,5,225,6,138,149,5,247,27,125,47,
	238,251,67,27,14,233,248,34,249,66,21,143,104,123,139,102,27,251,102,253,70,247,97,6,176,155,139,143,
	174,31,247,52,159,247,10,247,18,247,95,26,247,98,251,5,246,251,52,159,30,125,252,247,21,135,117,113,138,
	103,27,251,6,248,186,247,11,6,175,165,138,135,161,31,247,9,120,210,49,251,45,26,251,55,63,43,251,9,120,
	30,14,111,248,195,209,21,252,9,247,132,247,225,209,251,225,247,132,247,255,209,252,85,253,70,248,95,6,
	14,91,247,78,249,0,21,247,255,209,252,85,253,70,225,247,202,247,215,209,251,215,6,14,234,248,35,247,
	225,21,69,247,53,7,251,15,56,53,251,4,251,28,53,247,14,247,62,247,59,226,247,15,247,33,247,7,200,72,40,
	159,30,141,129,5,225,6,138,149,5,247,27,125,47,238,251,67,27,251,88,251,22,251,44,251,102,251,94,247,4,
	251,46,247,93,218,200,165,183,178,31,170,203,5,251,12,215,247,225,7,14,247,7,249,32,249,70,21,53,251,
	208,252,16,247,208,53,253,70,225,247,194,248,16,251,194,225,6,14,251,243,247,78,22,249,70,53,253,70,7,
	14,68,248,124,249,70,21,53,252,59,6,103,139,114,138,119,30,58,133,99,88,59,27,57,94,184,234,134,31,137,
	180,5,53,6,140,98,5,251,21,142,215,60,247,20,27,247,38,209,218,247,8,146,31,141,173,139,162,165,26,14,
	133,247,207,247,245,21,247,205,247,229,5,38,6,251,233,252,3,5,248,3,53,253,70,225,247,116,7,205,210,247,
	129,251,187,5,247,13,6,14,99,248,195,209,21,252,9,249,0,53,253,70,248,95,6,14,247,143,249,168,249,70,21,
	251,31,6,251,95,252,253,251,96,248,253,5,251,34,253,70,225,247,247,6,224,139,234,137,214,30,161,65,171,
	43,167,57,247,16,251,250,24,247,0,6,247,14,247,248,168,223,171,236,162,214,25,137,64,139,43,53,26,251,
	247,225,7,14,242,249,20,22,137,249,70,5,55,251,237,6,58,140,40,141,58,30,251,217,248,242,5,251,24,253,
	70,225,247,237,6,220,138,238,137,220,30,247,217,252,242,5,14,247,14,248,9,249,84,21,251,102,251,8,251,
	40,251,103,251,102,247,8,251,41,247,102,247,102,247,8,247,41,247,102,247,103,251,8,247,40,251,102,31,
	253,30,4,251,37,54,247,4,247,71,247,72,224,247,3,247,37,247,37,224,251,3,251,72,251,71,54,251,4,251,37,
	31,14,122,248,40,249,66,21,143,108,115,139,104,27,251,106,253,70,225,247,174,247,20,6,174,163,139,143,
	170,31,239,152,218,209,247,9,26,247,9,70,208,251,2,153,30,134,251,219,21,135,120,112,138,108,27,251,28,
	247,156,247,28,6,170,166,138,135,158,31,188,128,182,103,59,26,59,96,103,90,128,30,14,247,10,248,209,211,
	21,221,199,183,236,247,8,26,247,103,251,8,247,40,251,102,251,102,251,8,251,40,251,103,251,102,247,8,
	251,41,247,102,165,162,141,144,161,30,145,167,156,142,167,27,174,171,131,93,207,31,188,198,5,97,174,99,
	162,93,146,8,252,66,247,165,21,247,72,224,247,3,247,37,247,37,224,251,3,251,72,251,71,54,251,4,251,37,
	251,37,54,247,4,247,71,30,14,143,248,190,247,73,21,133,208,112,202,83,165,8,203,159,198,191,230,26,247,
	8,67,194,39,152,30,143,108,115,139,104,27,251,118,253,70,225,247,189,247,22,6,155,156,139,138,155,31,
	207,134,166,92,147,68,148,54,148,95,158,95,8,234,6,111,176,129,193,131,229,8,252,4,247,78,21,247,145,
	247,32,7,170,158,138,136,158,31,203,129,176,104,62,26,61,92,98,75,133,30,137,120,122,139,108,27,14,109,
	247,206,249,84,21,251,19,36,64,251,11,251,6,224,90,247,14,111,31,247,13,111,187,101,55,26,67,75,96,60,
	49,76,190,230,119,30,47,6,251,25,154,235,62,247,44,27,247,25,243,217,247,13,247,9,63,201,251,44,174,31,
	34,163,96,171,214,26,211,197,173,212,226,190,89,52,147,30,231,6,247,10,126,64,226,251,35,27,14,136,248,
	248,249,0,21,209,252,214,69,247,138,253,0,225,249,0,7,14,206,248,248,249,70,21,53,252,63,6,117,139,118,
	137,121,30,52,131,82,78,33,27,33,82,200,226,131,31,137,157,139,160,161,26,248,63,53,252,71,7,117,139,
	119,141,119,30,251,7,149,217,47,247,61,27,247,61,217,231,247,7,149,31,141,159,139,159,161,26,14,146,248,
	22,22,247,140,249,70,5,47,6,251,104,253,5,251,97,249,5,5,46,6,247,130,253,70,5,14,247,229,250,69,249,
	70,21,48,6,251,45,252,250,251,48,248,240,5,251,8,6,251,52,252,239,5,251,37,248,249,5,41,6,247,74,253,
	70,5,247,6,6,247,54,248,231,247,48,252,231,5,247,5,6,14,149,248,158,22,240,6,251,142,247,243,247,132,
	247,231,5,40,6,251,82,251,174,251,75,247,174,5,41,6,247,120,251,234,251,138,251,240,5,238,6,247,88,247,
	184,5,14,105,247,241,247,164,21,247,136,248,54,5,42,6,251,78,251,227,251,84,247,227,5,40,6,247,136,252,
	54,5,251,164,225,7,14,115,248,205,209,21,252,57,6,248,51,248,187,5,208,252,125,69,248,25,7,252,53,252,
	190,5,73,248,159,7,14,55,248,14,22,225,6,131,198,137,175,179,26,247,91,7,162,138,165,137,160,30,226,130,
	86,192,251,18,27,251,9,60,74,251,5,131,31,229,6,212,145,175,178,211,27,210,167,107,90,143,31,141,119,
	139,118,116,26,125,105,7,251,100,49,77,251,12,31,41,214,86,235,190,185,147,164,175,30,168,196,5,251,25,
	115,21,74,98,167,203,230,231,170,247,38,128,31,85,7,43,87,86,59,30,14,135,247,232,248,180,21,85,83,122,
	114,109,31,99,82,5,247,182,53,7,253,115,225,7,212,7,174,87,5,117,166,195,126,183,27,247,29,247,14,230,
	247,88,247,47,53,247,8,251,47,31,121,252,128,21,63,53,191,247,29,31,168,7,247,44,218,195,227,245,193,44,
	251,12,251,31,67,67,46,30,14,53,247,171,248,180,21,251,25,251,8,38,251,71,251,86,241,55,247,26,247,10,
	227,199,247,15,165,31,49,6,62,121,90,99,69,27,60,71,203,247,46,247,35,209,204,222,210,185,98,54,158,31,
	229,6,247,8,123,58,215,251,19,27,14,139,248,173,249,115,21,53,251,170,6,104,191,5,161,112,83,152,95,27,
	251,29,251,14,48,251,88,251,47,225,251,8,247,47,193,195,156,164,169,31,179,196,5,54,225,7,251,145,191,
	21,33,85,234,247,12,247,31,211,211,232,215,225,87,251,29,31,110,7,251,44,60,83,51,30,14,73,248,145,247,
	172,21,247,47,58,247,1,251,38,251,37,32,251,1,251,64,251,79,242,49,247,30,247,1,240,196,247,20,160,30,
	49,6,63,127,83,96,70,27,61,73,186,247,32,131,31,248,20,6,141,158,139,153,147,26,251,121,247,90,21,226,
	186,76,251,2,147,31,251,190,6,247,5,149,201,199,223,27,14,251,217,247,59,249,12,21,146,182,182,154,204,
	127,8,210,7,142,121,118,141,117,27,60,87,96,62,130,31,137,121,138,113,119,26,95,82,73,196,252,96,225,
	248,96,247,1,205,251,1,191,7,154,140,164,141,153,30,14,139,248,173,248,166,21,53,6,66,7,104,191,5,161,
	112,83,152,95,27,251,29,251,14,48,251,88,251,47,225,251,8,247,47,193,195,156,164,169,31,179,196,5,130,7,
	123,139,122,138,114,30,37,135,82,79,34,27,48,87,184,204,132,31,138,148,5,52,6,140,129,5,251,4,150,214,
	76,247,43,27,247,45,233,221,247,47,144,31,140,168,139,180,172,26,251,145,61,21,33,85,234,247,12,247,31,
	211,211,232,215,225,87,251,29,31,110,7,251,44,60,83,51,30,14,117,248,156,248,25,21,232,131,74,201,251,8,
	27,85,100,130,104,94,31,106,78,5,247,193,53,253,120,225,247,125,7,247,73,209,199,222,141,30,221,141,
	173,91,144,77,8,141,112,139,117,112,26,251,184,225,247,190,7,182,139,160,137,166,30,14,252,13,247,9,249,
	84,21,108,115,115,108,108,163,115,170,170,163,163,170,170,115,163,108,31,182,253,84,21,248,166,53,252,
	166,7,14,252,18,247,4,249,84,21,108,115,115,108,108,163,115,170,170,163,163,170,170,115,163,108,31,182,
	251,66,21,53,252,216,6,124,139,124,137,125,30,135,107,118,122,84,144,8,69,7,137,152,151,139,155,27,221,
	174,175,203,146,31,141,157,140,165,159,26,14,46,248,39,22,247,9,6,251,142,247,172,247,139,247,142,5,251,
	2,6,251,139,251,159,5,248,108,53,253,115,225,247,61,7,189,194,5,14,252,4,247,48,249,115,21,53,252,222,
	6,110,139,116,140,121,30,72,142,176,123,189,27,159,161,140,142,158,31,201,7,92,134,123,150,137,173,8,
	138,156,139,156,155,26,14,247,132,249,159,248,51,21,212,127,85,195,44,27,92,101,130,117,112,31,92,71,5,
	197,119,87,180,58,27,92,101,130,117,112,31,105,79,5,216,53,252,166,225,247,154,7,247,53,188,192,206,199,
	177,110,79,147,30,142,112,140,110,92,26,251,176,225,247,154,7,247,53,188,192,206,199,177,110,79,147,30,
	142,112,140,110,92,26,251,176,225,247,216,7,182,138,160,135,166,30,14,117,248,156,248,25,21,232,131,74,
	201,251,8,27,85,100,130,104,94,31,106,78,5,230,53,252,166,225,247,125,7,247,73,209,201,222,221,173,93,
	77,144,30,141,112,139,117,112,26,251,184,225,247,190,7,182,139,160,137,166,30,14,110,247,185,248,180,
	21,251,72,56,251,21,251,42,251,41,222,251,22,247,72,247,72,222,247,22,247,41,247,42,56,247,21,251,72,
	31,252,128,4,32,77,228,247,16,247,17,201,227,246,246,201,51,251,17,251,16,77,50,32,31,14,135,247,232,
	248,180,21,85,83,122,114,109,31,99,82,5,224,53,253,106,225,247,161,7,174,87,5,117,166,195,126,183,27,
	247,29,247,14,230,247,88,31,247,47,53,247,8,251,47,30,121,252,128,21,63,53,191,247,29,31,168,7,247,44,
	218,195,227,245,193,44,251,12,251,31,67,67,46,30,14,139,248,173,248,166,21,53,6,66,7,104,191,5,161,112,
	83,152,95,27,251,29,251,14,48,251,88,251,47,225,251,8,247,47,193,195,156,164,169,31,179,196,5,251,173,
	225,7,251,145,247,140,21,33,85,234,247,12,247,31,211,211,232,215,225,87,251,29,31,110,7,251,44,60,83,51,
	30,14,251,177,247,174,248,178,21,114,112,130,124,117,31,91,68,5,222,53,252,166,225,247,194,7,247,13,
	207,192,229,133,30,204,7,144,129,127,141,125,27,14,251,48,247,120,248,180,21,251,1,65,81,42,46,215,100,
	234,113,31,228,115,175,112,88,26,88,93,113,82,78,90,166,216,125,30,49,6,251,12,155,222,89,247,1,27,247,
	4,224,189,239,229,81,187,251,14,171,31,67,158,95,160,192,26,185,167,172,199,201,177,102,77,151,30,229,
	6,236,128,74,207,251,11,27,14,251,194,247,69,248,96,21,238,205,40,247,15,53,251,15,68,73,210,251,201,
	6,109,139,117,140,125,30,73,144,173,112,205,27,163,163,140,143,164,31,202,7,72,131,118,150,135,190,8,
	138,153,139,148,157,26,14,116,248,150,248,166,21,53,251,125,6,251,73,73,77,56,67,96,180,199,131,30,136,
	166,138,168,156,26,247,194,53,251,210,7,96,140,118,143,112,30,66,151,203,77,247,2,27,193,178,148,172,
	184,31,172,201,5,49,225,7,14,38,248,144,248,166,21,48,6,251,51,252,103,251,54,248,103,5,44,6,247,93,252,
	166,5,244,6,14,247,62,249,159,248,166,21,47,6,251,15,252,81,251,10,248,81,5,32,6,251,8,252,84,251,15,
	248,84,5,40,6,247,61,252,166,5,241,6,247,10,248,82,247,10,252,82,5,241,6,14,251,25,247,174,247,162,21,
	247,78,247,152,5,40,6,251,25,251,97,251,25,247,97,5,37,6,247,75,251,154,251,82,251,160,5,238,6,247,30,
	247,105,247,31,251,105,5,239,6,14,37,248,143,248,166,21,45,6,251,49,252,102,251,50,248,102,5,42,6,247,
	103,252,193,132,120,5,69,112,104,116,86,27,130,130,139,140,126,31,69,7,137,152,151,139,158,27,225,196,
	192,247,4,182,31,14,251,45,248,69,207,21,251,203,6,247,198,248,25,5,208,252,22,71,247,178,7,251,200,252,
	28,5,73,248,49,7,14,247,182,249,84,21,251,82,80,251,50,251,93,251,92,198,251,51,247,82,247,82,198,247,
	51,247,92,247,93,80,247,50,251,82,31,253,30,4,251,2,96,247,3,247,72,247,73,182,247,2,247,2,247,2,182,
	251,2,251,73,251,72,96,251,3,251,2,31,14,248,42,249,70,21,84,6,115,115,134,122,112,31,251,38,48,5,48,7,
	247,82,247,5,5,252,235,225,7,14,247,77,209,21,247,65,247,63,5,247,9,247,7,190,203,233,26,242,70,226,251,
	33,251,30,53,63,251,36,125,30,138,129,5,225,6,140,149,5,236,149,182,194,228,27,222,180,78,68,72,100,89,
	58,56,31,251,142,251,147,5,90,248,99,209,7,14,248,32,248,3,21,211,160,189,201,218,26,243,59,210,251,19,
	251,32,54,61,251,17,126,30,138,129,5,225,6,140,149,5,230,148,198,183,212,27,212,188,91,70,52,73,91,57,
	31,122,69,156,6,247,4,191,93,58,55,78,90,54,55,73,182,236,129,31,138,149,5,53,6,140,129,5,251,41,154,
	231,80,247,23,27,247,25,247,9,216,247,21,229,86,200,54,163,31,14,248,184,247,66,21,209,52,248,82,53,7,
	251,234,252,109,5,96,247,234,251,66,225,247,66,7,53,209,21,251,125,6,247,68,247,135,162,171,157,168,155,
	166,25,14,247,199,248,91,21,97,105,130,119,106,31,96,93,170,247,132,5,247,214,209,252,28,6,91,252,37,5,
	221,6,210,173,185,166,203,27,231,194,74,35,35,79,79,48,53,96,189,218,123,31,137,149,5,53,6,140,129,5,
	251,22,152,220,72,247,31,27,247,20,247,10,225,247,44,247,23,63,239,251,46,31,14,247,194,248,102,21,89,
	100,126,108,102,31,100,77,5,247,93,146,205,214,232,27,209,180,99,61,155,31,141,129,5,231,6,138,149,5,
	247,5,128,55,212,251,10,27,251,34,251,17,251,6,251,170,251,121,229,42,247,46,247,32,243,238,247,39,247,
	29,54,236,251,36,31,124,252,48,21,52,82,213,235,247,7,205,198,217,244,182,59,43,39,77,71,53,31,14,248,
	174,249,70,21,252,124,69,248,19,6,251,223,253,0,5,231,6,247,236,249,30,5,14,248,40,248,1,21,203,162,189,
	193,215,26,247,6,52,211,251,27,251,27,52,67,251,6,64,184,84,201,115,30,65,116,76,74,56,26,251,43,247,7,
	82,247,23,247,13,247,17,196,247,43,222,81,204,65,163,30,251,0,247,163,21,226,181,84,73,69,95,86,53,53,
	97,192,209,205,181,194,226,31,135,252,219,21,38,88,196,218,210,183,203,246,246,181,75,68,60,92,82,38,
	31,14,247,185,249,84,21,251,32,35,40,251,39,251,29,224,42,247,36,189,178,152,170,176,31,178,201,5,251,
	93,132,73,64,46,27,69,98,179,217,123,31,137,149,5,47,6,140,129,5,251,5,150,223,66,247,10,27,247,34,247,
	17,247,6,247,170,247,121,49,236,251,46,31,252,48,4,34,96,219,235,239,201,207,225,226,196,65,43,251,7,
	73,80,61,31,14,84,247,193,247,188,21,149,247,71,5,77,6,149,251,71,251,47,200,120,80,247,51,98,251,5,
	251,30,189,102,238,247,45,238,251,45,189,176,251,5,247,30,247,51,180,120,198,5,14,251,204,247,200,109,
	21,251,143,249,130,5,77,6,247,143,253,130,5,14,252,31,240,248,180,21,102,112,112,102,102,166,112,176,
	176,166,166,176,176,112,166,102,31,252,66,4,102,112,112,102,102,166,112,176,176,166,166,176,176,112,166,
	102,31,14,252,31,247,32,229,21,149,46,7,61,251,112,5,220,6,14,252,9,247,52,247,72,21,151,248,146,5,37,6,
	151,252,146,5,178,73,21,102,112,112,102,102,166,112,176,176,166,166,176,176,112,166,102,31,14,248,44,
	248,51,21,241,195,49,6,186,247,111,5,81,6,92,251,111,5,48,6,186,247,111,5,81,6,92,251,111,5,251,5,83,
	240,6,110,251,32,5,37,83,229,6,92,251,111,5,197,6,186,247,111,5,230,6,92,251,111,5,197,6,186,247,111,5,
	247,5,195,38,6,111,247,32,21,109,251,32,5,48,6,169,247,32,5,14,252,31,240,247,6,21,102,112,112,102,102,
	166,112,176,176,166,166,176,176,112,166,102,31,14,251,8,247,152,249,84,21,251,23,49,57,251,20,135,31,
	229,6,232,145,190,188,214,27,211,182,93,70,79,116,99,68,58,31,83,75,121,102,133,80,8,225,6,142,188,154,
	166,200,200,8,210,210,173,202,209,26,242,65,219,251,26,30,120,252,226,21,102,112,112,102,102,166,112,
	176,176,166,166,176,176,112,166,102,31,14,251,168,247,58,248,121,21,149,247,142,5,49,6,149,251,142,5,
	247,102,22,149,247,142,5,49,6,149,251,142,5,14,252,75,247,15,248,121,21,149,247,142,5,49,6,149,251,142,
	5,14,252,31,240,248,180,21,102,112,112,102,102,166,112,176,176,166,166,176,176,112,166,102,31,179,252,
	82,21,151,45,7,61,251,122,5,221,6,14,251,204,196,109,21,247,143,249,130,5,77,6,251,143,253,130,5,14,
	251,7,248,86,53,21,195,252,36,83,7,14,251,204,247,162,249,104,21,195,128,7,53,89,115,57,118,141,122,149,
	68,31,147,84,142,112,111,26,95,117,112,79,133,30,83,7,199,133,161,112,95,26,111,136,112,131,84,30,129,
	68,137,122,118,26,57,189,115,225,30,150,195,127,6,80,121,157,170,162,141,152,148,203,31,149,209,142,
	161,170,26,197,106,177,85,146,30,194,146,171,175,197,26,170,136,161,129,209,30,130,203,137,152,162,26,
	170,157,157,198,30,14,251,204,247,131,248,14,21,79,145,117,166,183,26,167,142,166,147,194,30,149,210,
	141,156,160,26,221,89,163,53,30,128,83,151,6,198,157,121,108,116,137,126,130,75,31,129,69,136,117,108,
	26,81,172,101,193,132,30,84,132,107,103,81,26,108,142,117,149,69,30,148,75,141,126,116,26,108,121,121,
	80,30,127,83,150,6,225,189,163,221,160,137,156,129,210,31,131,194,136,166,167,26,183,161,166,199,145,
	30,14,251,200,247,153,249,160,21,251,55,253,240,247,55,195,34,249,128,244,6,14,251,212,247,89,59,21,
	249,240,251,55,83,244,253,128,34,83,7,14,251,192,247,113,249,160,21,64,251,5,93,251,51,251,50,26,251,50,
	185,251,51,214,251,5,30,197,6,65,247,17,96,247,47,247,42,26,247,42,182,247,47,213,247,17,30,14,251,192,
	169,249,160,21,213,251,17,182,251,47,251,42,26,251,42,96,251,47,65,251,17,30,197,6,214,247,5,185,247,51,
	247,50,26,247,50,93,247,51,64,247,5,30,14,251,233,247,134,247,147,21,195,251,102,83,7,14,252,35,14,248,
	162,247,73,21,242,86,204,251,40,186,30,247,130,7,203,129,170,92,146,71,8,231,6,127,240,76,222,251,11,
	149,8,216,81,61,7,251,2,128,60,72,251,5,26,251,7,212,87,247,8,107,30,251,148,7,68,150,97,187,130,210,8,
	47,6,144,251,8,221,64,247,19,131,8,62,197,217,7,247,5,150,227,209,247,4,26,251,248,247,235,21,202,176,
	176,199,149,30,251,121,7,72,160,109,170,206,26,247,47,251,115,21,221,112,166,94,81,26,67,95,103,74,131,
	30,14,248,82,247,248,21,92,128,110,101,111,110,154,159,102,30,167,87,114,152,100,27,65,103,92,53,31,
	195,6,186,150,168,177,167,168,124,119,176,30,111,191,164,126,178,27,213,175,186,225,31,14,248,136,248,
	23,21,195,252,56,83,7,248,56,251,92,21,195,252,56,83,7,14,248,105,247,191,21,252,37,247,93,5,69,7,247,
	234,251,58,251,234,251,58,5,71,7,248,37,247,93,5,14,248,140,248,68,21,207,7,252,37,251,93,5,71,7,248,37,
	251,93,5,209,7,251,234,247,58,5,14,247,17,247,81,249,80,21,49,81,78,41,42,197,77,229,229,197,201,236,
	237,81,200,49,31,140,253,80,21,248,84,249,70,5,52,6,252,84,253,70,5,225,249,20,21,177,165,105,74,75,113,
	104,101,101,113,174,203,204,165,173,177,31,247,254,251,228,21,49,81,78,41,42,197,77,229,229,197,201,236,
	237,81,200,49,31,79,4,177,165,105,74,75,113,104,101,101,113,174,203,204,165,173,177,31,14,248,136,247,
	179,21,195,251,73,247,98,81,251,98,251,73,83,247,73,251,94,197,247,94,7,14,178,249,2,248,21,21,43,247,4,
	53,251,4,79,6,117,117,139,140,121,31,65,143,83,166,233,26,216,193,187,230,188,172,136,131,180,30,208,7,
	146,101,106,142,90,27,251,35,38,59,251,10,58,193,84,215,121,31,41,118,79,64,56,26,251,38,247,4,98,247,
	53,206,204,147,152,196,30,247,200,235,7,252,98,251,21,21,203,183,192,247,7,149,30,141,159,158,139,161,
	27,199,251,144,6,132,105,112,137,99,27,251,17,85,175,235,31,14,248,68,247,107,21,207,6,251,68,248,37,5,
	71,6,251,68,252,37,5,209,6,247,33,247,237,5,14,247,129,249,14,248,101,21,58,6,122,78,130,187,5,160,120,
	106,149,103,27,251,19,50,251,16,251,23,251,14,216,100,203,31,175,171,151,166,171,31,160,180,5,88,147,
	179,118,191,27,235,247,1,214,247,73,247,91,251,24,247,26,251,81,251,97,251,102,251,49,251,156,251,126,
	247,57,251,5,247,69,219,240,162,205,236,31,115,188,5,85,66,42,113,58,27,251,47,251,20,235,247,86,247,
	123,247,74,247,23,247,60,247,34,247,12,46,251,76,251,26,76,77,79,106,134,157,157,147,140,150,142,152,31,
	66,189,21,79,125,95,81,84,27,101,109,166,213,231,186,230,220,31,187,175,108,43,117,31,14,252,1,247,46,
	251,92,21,250,124,81,254,124,7,14,251,207,247,74,248,216,21,78,247,2,5,59,6,224,251,2,5,14,248,216,20,
	248,251,21,0,0,1,0,0,0,1,26,28,240,219,222,59,95,15,60,245,0,3,3,232,0,0,0,0,208,26,159,217,0,0,0,0,208,
	26,159,217,255,106,255,33,4,38,3,127,0,0,0,3,0,2,0,0,0,0,0,0,1,244,0,0,2,127,0,29,2,115,0,100,2,187,0,
	47,2,197,0,100,2,75,0,100,2,55,0,100,2,198,0,47,2,218,0,100,1,8,0,100,2,32,0,55,2,97,0,100,2,63,0,100,
	3,98,0,100,2,206,0,100,2,225,0,47,2,86,0,100,2,221,0,47,2,107,0,100,2,73,0,52,2,100,0,34,2,170,0,94,2,
	110,0,32,3,184,0,26,2,113,0,25,2,69,0,19,2,79,0,46,2,19,0,36,2,99,0,74,2,17,0,30,2,103,0,30,2,37,0,30,1,
	34,0,21,2,103,0,30,2,81,0,74,0,238,0,62,0,233,255,243,2,10,0,74,0,247,0,70,3,87,0,74,2,81,0,74,2,74,0,
	30,2,99,0,74,2,103,0,30,1,74,0,74,1,203,0,24,1,57,0,20,2,80,0,66,2,2,0,1,3,17,0,1,1,226,255,250,2,1,0,
	1,1,206,0,20,2,68,0,41,2,68,0,130,2,68,0,60,2,68,0,45,2,68,0,33,2,68,0,57,2,68,0,43,2,68,0,50,2,68,0,
	44,2,68,0,49,2,48,0,85,1,47,255,251,0,220,0,37,0,220,255,225,0,242,0,57,2,68,0,70,0,220,0,37,1,243,0,
	35,1,83,0,86,0,176,0,43,0,220,255,225,1,47,255,251,1,244,0,50,1,47,0,64,1,47,0,33,1,51,0,98,1,39,0,34,
	1,59,0,100,1,59,0,30,1,18,0,32,0,216,0,0,2,68,0,53,2,68,0,77,2,68,0,80,2,68,0,68,2,68,0,103,2,228,0,
	41,2,68,0,80,2,142,0,64,2,68,0,80,3,84,0,54,0,250,0,96,1,44,0,41,0,1,0,0,3,202,255,26,0,0,4,66,255,106,
	255,106,4,38,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,96,0,3,2,30,1,144,0,5,0,8,2,138,2,88,0,0,0,75,2,138,2,88,
	0,0,1,94,0,50,1,62,0,0,0,0,5,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,85,75,87,78,0,64,0,32,0,
	126,3,2,255,26,0,200,3,202,0,230,32,0,0,147,0,0,0,0,2,18,2,178,0,0,0,32,0,3,0,0,0,0,0,6,0,0,0,0,0,2,0,0,
	0,3,0,0,0,20,0,3,0,1,0,0,0,20,0,4,0,146,0,0,0,16,0,16,0,3,0,0,0,47,0,57,0,64,0,90,0,96,0,122,0,126,
	255,255,0,0,0,32,0,48,0,58,0,65,0,91,0,97,0,123,255,255,0,0,0,5,0,0,255,192,0,0,255,186,0,0,0,1,0,16,0,
	0,0,44,0,0,0,54,0,0,0,62,0,0,0,83,0,67,0,71,0,68,0,84,0,89,0,91,0,72,0,80,0,81,0,63,0,90,0,66,0,82,0,
	69,0,74,0,65,0,73,0,88,0,86,0,87,0,70,0,93,0,78,0,64,0,79,0,92,0,75,0,95,0,76,0,94,0,77,0,85,0,0,0,3,0,
	0,0,0,0,0,255,181,0,50,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,12,0,0,0,0,0,0,0,2,0,1,0,1,0,
	52,0,1,0,0,0,1,0,0,0,10,0,12,0,14,0,0,0,0,0,0,0,1,0,0,0,10,0,12,0,14,0,0,0,0,0,0,
};
const size_t aileron_size = sizeof aileron_data;
