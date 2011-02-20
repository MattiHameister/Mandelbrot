CC=/opt/local/bin/clang
LIBS=-framework OpenGL -framework GLUT
CFLAGS=-O2 -I/opt/local/include -Wall
LDFLAGS=$(LIBS) -Wl,/opt/local/lib/libgmp.a

MANDEL_OBJ := \
	doubleGMP.o \
	main.o

MANDEL_TARG := mandelbrot

$(MANDEL_TARG) : $(MANDEL_OBJ)
	$(CC) $(LDFLAGS) -o $(MANDEL_TARG) $(MANDEL_OBJ)

%.o : %.c
	$(CC) $(CFLAGS) -o $@ -c $<

all: $(MANDEL_TARG)

clean :
	$(RM) $(MANDEL_TARG) $(MANDEL_OBJ)
	$(RM) *.xyrgb

clean-all : clean
	$(RM) *.val
	
run : all
	./$(MANDEL_TARG)
