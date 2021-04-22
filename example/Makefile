.POSIX:

OBJS = main.o ../vtk2.o ../deps/nanovg/src/nanovg.o
LDFLAGS = -lm $(shell pkg-config --libs epoxy glfw3)
CFLAGS = $(shell pkg-config --cflags epoxy glfw3) -g

example: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^
