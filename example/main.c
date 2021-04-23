#include "../vtk2.h"

int main() {
	struct vtk2_block *level2[] = {
		vtk2_make_box({.margins = (lay_vec4){10, 10, 10, 10}, .size = (lay_vec2){50, 50}}, NULL),
		vtk2_make_box({.layout_flags = LAY_FILL, .margins = (lay_vec4){10, 10, 10, 10}}, NULL),
		vtk2_make_box({.margins = (lay_vec4){10, 10, 10, 10}, .size = (lay_vec2){100, 200}}, NULL),
		NULL
	};
	struct vtk2_block *level1[] = {
		vtk2_make_box({.box_flags = LAY_ROW, .margins = (lay_vec4){10, 10, 10, 10}}, level2),
		vtk2_make_box({.layout_flags = LAY_FILL, .margins = (lay_vec4){10, 10, 10, 10}, .size = (lay_vec2){400, 400}}, level2),
		NULL
	};
	struct vtk2_block *root = vtk2_make_box({.box_flags = LAY_COLUMN}, level1);

	enum vtk2_err err;
	struct vtk2_win win;
	if ((err = vtk2_window_init(&win, "Hello, world!", 800, 600))) {
		vtk2_perror("error creating window", err);
		return 1;
	};
	if ((err = vtk2_window_set_root(&win, root))) {
		vtk2_perror("error setting root element", err);
		return 1;
	}

	vtk2_window_mainloop(&win);

	vtk2_window_deinit(&win);
	glfwTerminate();

	return 0;
}
