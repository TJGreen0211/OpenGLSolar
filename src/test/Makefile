CC = gcc
CFLAGS = -Wall
PROG = cube

SRCS = Main.c Common.c LoadShader.c GUI.c LinearAlg.c Camera.c Sphere.c Obj.c

INCLUDE_PATHS = -I/usr/local/include -I/opt/X11/include -I/opt/local/include -I
LIBRARY_PATHS = -L/usr/local/lib -lglfw -lglew -I/opt/X11/lib -L/usr/local/opt -L/opt/local/lib -lSOIL 

ifeq ($(shell uname),Darwin)
	LIBS = -framework OpenGL -framework GLUT
else
	LIBS = -lglut
endif

all: $(PROG)

$(PROG):	$(SRCS)
	$(CC) $(CFLAGS) -o $(PROG) $(SRCS) $(LIBS) $(INCLUDE_PATHS) $(LIBRARY_PATHS) $(RELATIVE_PATHS)

clean:
	rm -f $(PROG)
