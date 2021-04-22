#include "../vtk2.h"

int main() {
	enum vtk2_err err;
	struct vtk2_win win;
	if ((err = vtk2_window_init(&win, "Hello, world!", 800, 600))) {
		vtk2_perror("error creating window", err);
		return 1;
	};

	vtk2_window_deinit(&win);
	glfwTerminate();

	return 0;
}
