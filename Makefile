CC=/opt/local/bin/clang
LIBS=-framework OpenGL -framework GLUT
CFLAGS=-O2 -I/opt/local/include -Wall
LDFLAGS=$(LIBS) -L/opt/local/lib -lgmp

MANDEL_OBJ := \
	main.o

MANDEL_TARG := mandelbrot

$(MANDEL_TARG) : $(MANDEL_OBJ)
	$(CC) $(LDFLAGS) -o $(MANDEL_TARG) $(MANDEL_OBJ)

%.o : %.c
	$(CC) $(CFLAGS) -o $@ -c $<

all: $(MANDEL_TARG)

run : all
	./$(MANDEL_TARG)
