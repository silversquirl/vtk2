#include <stdatomic.h>
#include <stdio.h>
#include <threads.h>
#include "../vtk2.h"

struct worker_data {
	atomic_int x;
	struct vtk2_win *win;
};

int worker(void *data_p) {
	struct worker_data *data = data_p;
	for (;;) {
		struct timespec ts = {.tv_sec = 1};
		while (thrd_sleep(&ts, &ts) == -1);
		data->x++;
		printf("%d\n", data->x);
		vtk2_window_redraw(data->win);
	}
}

int main() {
	struct vtk2_block *level2[] = {
		vtk2_make_text(.text = "Hello, world!", .scale = 60),
		vtk2_make_box(.margins = {10, 10, 10, 10}, .size = {50, 50}),
		vtk2_make_box(.margins = {10, 10, 10, 10}, .grow = 2, .size = {100, NAN}),
		vtk2_make_box(.margins = {10, 10, 10, 10}, .grow = 1, .size = {300, 50}),
		vtk2_make_box(.margins = {10, 10, 10, 10}, .size = {100, 200}),
		NULL
	};
	struct vtk2_block *level1[] = {
		vtk2_make_box(.margins = {10, 10, 10, 10}, .grow = 1, .direction = VTK2_COL, .children = level2),
		vtk2_make_box(.margins = {10, 10, 10, 10}, .size = {400, 400}),
		vtk2_make_text(.text = "This is an example program", .color = {0, 1, 0, 1}, .margins = {10, 10, 10, 10}),
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

	struct worker_data data = {
		.x = 0,
		.win = &win,
	};
	thrd_t thr;
	int thr_err = thrd_create(&thr, worker, &data);
	if (thr_err != thrd_success) {
		fprintf(stderr, "thread error: %d\n", thr_err);
		vtk2_window_deinit(&win);
		glfwTerminate();
		return 1;
	}

	vtk2_window_mainloop(&win);

	vtk2_window_deinit(&win);
	glfwTerminate();

	return 0;
}
