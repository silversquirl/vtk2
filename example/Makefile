OBJS = main.o ../vtk2.o
LDFLAGS = -lm -lpthread $(shell pkg-config --libs epoxy glfw3)
CFLAGS = $(shell pkg-config --cflags epoxy glfw3) -g -std=c11 -DVTK2_BOX_DEBUG -Wall

example: $(OBJS) ../vtk2.h
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

../vtk2.o: ../vtk2.h
main.o: ../vtk2.h
