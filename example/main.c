#include "../vtk2.h"

int main() {
	struct vtk2_block *level2[] = {
		vtk2_make_box(.margins = {20, 10, 10, 20}, .size = {50, 50}),
		vtk2_make_box(.margins = {10, 10, 10, 20}, .grow = 1, .size = {100, NAN}),
		vtk2_make_box(.margins = {10, 10, 0, 20}, .size = {100, 200}),
		NULL
	};
	struct vtk2_block *level1[] = {
		vtk2_make_box(.margins = {10, 10, 10, 10}, .grow = 1, .direction = VTK2_COL, .children = level2),
		vtk2_make_box(.margins = {10, 10, 10, 10}, .size = {400, 400}),
		NULL
	};
	struct vtk2_block *root = vtk2_make_box(.children = level1);

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
