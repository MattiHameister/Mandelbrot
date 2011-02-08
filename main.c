#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <math.h>
#include <OpenGL/gl.h>
#include <GLUT/glut.h>
#include <pthread.h>
#include <gmp.h>

int32_t sizeX;
int32_t sizeY;

int32_t selectZoomX=0;
int32_t selectZoomY=0;
int32_t selectZoomW=0;
int32_t selectZoomH=0;

int32_t selectMoveX=0;
int32_t selectMoveY=0;
int32_t selectMoveW=0;
int32_t selectMoveH=0;

uint32_t mousePosX=0;
uint32_t mousePosY=0;
uint8_t rightMouse=0;

uint32_t maxIterations;
int32_t gmpBit = 64;

typedef struct rangeTag {
	mpf_t minRe;
	mpf_t maxRe;
	mpf_t minIm;
	mpf_t maxIm;
} range;
range mandelRange;

typedef struct screemTilesTag {
	int32_t fromX;
	int32_t fromY;
	int32_t toX;
	int32_t toY;
} screenTiles;

//###############Liste########################
typedef struct pixelElementTag *pixelListPtr;
typedef struct pixelElementTag {
	int32_t x;
	int32_t y;
	float red;
	float green;
	float blue;
	pixelListPtr next;
} pixelElement;

typedef struct pixelHeadTag *pixelHeadPtr;
typedef struct pixelHeadTag {
	pixelListPtr first;
	pixelListPtr last;
} pixelHead;

pixelListPtr createPixelElement() {
	pixelListPtr p = calloc(1, sizeof(pixelElement));
	p->next = NULL;
	return p;	
}

void insertInto(pixelHeadPtr head, pixelListPtr pixel) {
	if(head->first == NULL) {
		head->first = pixel;
		head->last = pixel;
	} else {
		head->last->next = pixel;
		head->last = pixel;
	}
}

void deletePixelList(pixelHeadPtr head) {
	if (head->first != NULL) {
		pixelListPtr p = NULL;
		while(head->first->next != NULL) {
			p = head->first->next;
			head->first->next = p->next;
			free(p);
		}
		p = head->first;
		free(p);
	}
	head->first = NULL;
	head->last = NULL;
}
//###############Liste########################

//################File########################
uint8_t fileMode = 0;
FILE **files = NULL;

void writeToFile(uint32_t num, uint32_t x, uint32_t y, float r, float g, float b) {
	FILE *f = files[num];
	fwrite(&x, sizeof(x), 1, f);
	fwrite(&y, sizeof(y), 1, f);
	fwrite(&r, sizeof(r), 1, f);
	fwrite(&g, sizeof(g), 1, f);
	fwrite(&b, sizeof(b), 1, f);
}

//################File########################

//###############Thread########################
void renderImage(screenTiles *tile, uint32_t threadNum);

uint32_t numThreads = 16;
pthread_t *threads = NULL;
uint32_t *thread_args  = NULL;
pixelHead *pixelListHeads  = NULL;
uint8_t threadStop = 0;
screenTiles *tiles = NULL;

void cleanMemory() {
	free(tiles);
	free(files);
	tiles = NULL;
	free(thread_args);
	thread_args = NULL;
	if(pixelListHeads != NULL) {
		for (int i = 0; i<numThreads; i++) {
			deletePixelList(&pixelListHeads[i]);
		}
		free(pixelListHeads);
		pixelListHeads = NULL;
	}
}

void waitForThreads() {
	for (int i=0; i<numThreads; ++i) {
		pthread_join(threads[i], NULL);
	}	
}

void stopThreads() {
	threadStop = 1;
	if(threads!= NULL) {
		waitForThreads();
	}
	threadStop = 0;
}

void *ThreadCode(void *argument)
{
	uint32_t tid;
	tid = *((int *) argument);
	printf("Thread %i gestartet\n", tid);
	
	if (fileMode) {
		//file oeffnen
		char buf[255];
		snprintf(buf, 255, "%d.xyrgb", tid);
		files[tid] = fopen(buf,"wb");
	}	

	renderImage(&(tiles[tid]),tid);
	
	//file schliessen
	if (fileMode) {
		fclose(files[tid]);
	}
	return NULL;
}

void startThreads() {
	stopThreads();
	cleanMemory();
	threadStop = 0;
	files = calloc(numThreads, sizeof(FILE));
	threads = calloc(numThreads, sizeof(pthread_t));
	thread_args = calloc(numThreads, sizeof(uint32_t));
	pixelListHeads = calloc(numThreads, sizeof(pixelHead));
	tiles = calloc(numThreads*numThreads, sizeof(screenTiles));
	
	int32_t numSplit=numThreads;
	int32_t offSetX=0;
	//	int32_t offSetY=0;
	int32_t dX = sizeX / numSplit;
	//	int32_t dY = sizeY / numSplit;
	int num = 0;
	for (int32_t x = 0; x<numSplit; x++) {
		//		for (int32_t y = 0; y<numSplit; y++) {
		tiles[num].fromX = offSetX;
		tiles[num].toX = offSetX+dX;
		tiles[num].fromY = 0;
		tiles[num].toY = sizeY;
		//			offSetY+=dY;
		num++;
		//		}			
		//		offSetY=0;
		offSetX+=dX;
	}
	
	for (int i=0; i<numThreads; i++) {
		thread_args[i] = i;
		printf("Erstelle Thread %d\n", i);
		pthread_create(&threads[i], NULL, ThreadCode, (void *) &thread_args[i]);
	}
	
}

//###############Thread########################
void initGL() {
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glDisable(GL_DEPTH_TEST);
}

void init() {
	mpf_init2(mandelRange.minRe,gmpBit);
	mpf_init2(mandelRange.maxRe,gmpBit);
	mpf_init2(mandelRange.minIm,gmpBit);
	mpf_init2(mandelRange.maxIm,gmpBit);
	
	mpf_set_d(mandelRange.minRe,-2.0);
	mpf_set_d(mandelRange.maxRe,1.0);
	mpf_set_d(mandelRange.minIm,-1.2);
	
	maxIterations = 100;
}

void setBits(int32_t bits) {
	mpf_set_prec(mandelRange.minRe,bits);
	mpf_set_prec(mandelRange.maxRe,bits);
	mpf_set_prec(mandelRange.minIm,bits);
	mpf_set_prec(mandelRange.maxIm,bits);
}

void restart() {
	stopThreads();
	startThreads();
}

void recalcZoom() {
	int32_t diffX = selectZoomW - selectZoomX;
	int32_t diffY = selectZoomH - selectZoomY;
	if(abs(diffX) >= 10 || abs(diffY) >= 10) {
		long double x = sizeX;
		long double y = sizeY;
		long double factorLeft = ((selectZoomX*100.0)/x)/100.0;
		long double factorRight = 1.0-((selectZoomW*100.0)/x)/100.0;
		long double factorBottom = 1.0-((selectZoomH*100.0)/y)/100.0;
		
		mpf_t facLeft;
		mpf_init2(facLeft,gmpBit);
		mpf_set_d(facLeft,factorLeft);
		mpf_t facRight;
		mpf_init2(facRight,gmpBit);
		mpf_set_d(facRight,factorRight);
		mpf_t facBottom;
		mpf_init2(facBottom,gmpBit);
		mpf_set_d(facBottom,factorBottom);
		mpf_t tmpDiff;
		mpf_init2(tmpDiff,gmpBit);
		mpf_t tmp;
		mpf_init2(tmp,gmpBit);
		
		//(maxRe - minRe)
		mpf_sub(tmpDiff,mandelRange.maxRe,mandelRange.minRe);
		
		//minRe+=(maxRe - minRe)*factorLeft;
		mpf_mul(tmp,tmpDiff,facLeft);
		mpf_add(mandelRange.minRe,mandelRange.minRe,tmp);
		
		//maxRe-=(maxRe - minRe)*factorRight;
		mpf_mul(tmp,tmpDiff,facRight);
		mpf_sub(mandelRange.maxRe,mandelRange.maxRe,tmp);
		
		
		//minIm+=(maxIm - minIm)*factorBottom;
		mpf_sub(tmpDiff,mandelRange.maxIm,mandelRange.minIm);
		mpf_mul(tmp,tmpDiff,facBottom);
		mpf_add(mandelRange.minIm,mandelRange.minIm,tmp);
		
		
		mpf_clear(tmp);
		mpf_clear(tmpDiff);
		mpf_clear(facLeft);
		mpf_clear(facRight);
		mpf_clear(facBottom);
		
		restart();
	}
}

void recalcView() {
	long double x = sizeX;
	long double y = sizeY;
	int32_t diffX = selectMoveW - selectMoveX;
	int32_t diffY = selectMoveH - selectMoveY;
	if(abs(diffX) >= 1 || abs(diffY) >= 1) {
		long double factorX = ((diffX*100.0)/x)/100.0;
		long double factorY = ((diffY*100.0)/y)/100.0;
		
		mpf_t facX;
		mpf_init2(facX,gmpBit);
		mpf_set_d(facX,factorX);
		mpf_t facY;
		mpf_init2(facY,gmpBit);
		mpf_set_d(facY,factorY);		
		mpf_t tmpDiff;
		mpf_init2(tmpDiff,gmpBit);		
		mpf_t tmp;
		mpf_init2(tmp,gmpBit);
		
		
		mpf_sub(tmpDiff,mandelRange.maxRe,mandelRange.minRe);
		
		//minRe-=(maxRe - minRe)*factorX;
		mpf_mul(tmp,tmpDiff,facX);
		mpf_sub(mandelRange.maxRe,mandelRange.maxRe,tmp);
		//maxRe-=(maxRe - minRe)*factorX;
		mpf_sub(mandelRange.minRe,mandelRange.minRe,tmp);
		
		//minIm+=(maxIm - minIm)*factorY;
		mpf_sub(tmpDiff,mandelRange.maxIm,mandelRange.minIm);
		mpf_mul(tmp,tmpDiff,facY);
		mpf_add(mandelRange.minIm,mandelRange.minIm,tmp);
		
		mpf_clear(tmp);
		mpf_clear(tmpDiff);
		mpf_clear(facX);
		mpf_clear(facY);
		
		restart();
	}
}

void keyboard(uint8_t key, int32_t x, int32_t y)
{
	FILE *file;
	int32_t base = 10;
	
	switch (key) {
		case 27: // ESC
			exit(0);
			break;
			
		case 105: // i
			maxIterations+=100;
			printf("Iterationen: %u\n",maxIterations);
			restart();
			break;
			
		case 115: // s
			printf("Speichere Position\n");
			file = fopen("minRe.val", "w");
			mpf_out_str(file, base, 0, mandelRange.minRe);
			fclose(file);
			file = fopen("maxRe.val", "w");
			mpf_out_str(file, base, 0, mandelRange.maxRe);
			fclose(file);
			file = fopen("minIm.val", "w");
			mpf_out_str(file, base, 0, mandelRange.minIm);
			fclose(file);
			file = fopen("iter.val", "wb");
			fwrite(&maxIterations, sizeof(maxIterations), 1, file);
			fclose(file);
			file = fopen("bits.val", "wb");
			fwrite(&gmpBit, sizeof(gmpBit), 1, file);
			fclose(file);
			printf("Position gespeichert\n");
			break;
			
		case 108: // l
			printf("Lade Position\n");
			file = fopen("minRe.val", "r");
			if (file != NULL) {
				mpf_inp_str(mandelRange.minRe, file, base);
			}
			
			if (file != NULL) {
				file = fopen("maxRe.val", "r");
				mpf_inp_str(mandelRange.maxRe, file, base);
			}
			fclose(file);
			
			if (file != NULL) {
				file = fopen("minIm.val", "r");
				mpf_inp_str(mandelRange.minIm,file, base);
			}
			fclose(file);
			file = fopen("iter.val", "rb");
			if (file != NULL) {
				fread(&maxIterations, sizeof(maxIterations), 1, file);
			}
			fclose(file);
			file = fopen("bits.val", "rb");
			if (file != NULL) {
				fread(&gmpBit, sizeof(gmpBit), 1, file);
			}
			fclose(file);
			setBits(gmpBit);
			printf("Position geladen\n");
			restart();
			break;
			
		case 98: //b
			gmpBit+=1;
			printf("Bit: %i\n",gmpBit);
			setBits(gmpBit);
			restart();
			break;
			
		case 99: // c
			printf("aktuelle Bits: %lu\n",mpf_get_prec(mandelRange.minRe));
			break;
			
			//		case 103: // g
			//			useGMP = useGMP ? 0 : 1;
			//			printf("benutze GMP: %i\n",useGMP);
			//			break;
			
		default:
			printf("%i\n",key);
			break;
	}
}

void mouse (int32_t button, int32_t state, int32_t x, int32_t y) {
    switch (button) {
        case GLUT_LEFT_BUTTON:
			rightMouse=0;
            switch (state) {
				case GLUT_DOWN:
					selectZoomX = x;
					selectZoomY = y;
					break;
				case GLUT_UP:
					selectZoomW = x;
					selectZoomH = y;
					mousePosX = 0;
					mousePosY = 0;
					recalcZoom();
					break;
			}
            break;
			
        case GLUT_RIGHT_BUTTON:
			rightMouse=1;
            switch (state) {
				case GLUT_DOWN:
					selectMoveX = x;
					selectMoveY = y;
					break;
				case GLUT_UP:
					selectMoveW = x;
					selectMoveH = y;
					mousePosX = 0;
					mousePosY = 0;
					recalcView();
					break;
			}
            break;
    }
}

void mousemove(int x, int y) {
	mousePosX = x;
	mousePosY = y;
}

void renderImage(screenTiles *tile, uint32_t threadNum) {
	printf("Rendere: %ix%ix%ix%i\n",tile->fromX,tile->toX,tile->fromY,tile->toY);
	mpf_t tmpDiff;
	mpf_t tmp;
	mpf_t re_factor;
	mpf_t im_factor;
	mpf_t c_im;
	mpf_t c_re;
	mpf_t z_re;
	mpf_t z_im;
	mpf_t z_re2;
	mpf_t z_im2;
	
	mpf_init2(tmpDiff,gmpBit);
	mpf_init2(tmp,gmpBit);
	mpf_init2(re_factor,gmpBit);
	mpf_init2(im_factor,gmpBit);
	mpf_init2(c_im,gmpBit);
	mpf_init2(c_re,gmpBit);
	mpf_init2(z_im,gmpBit);
	mpf_init2(z_re,gmpBit);
	mpf_init2(z_im2,gmpBit);
	mpf_init2(z_re2,gmpBit);
	
	mpf_sub(tmpDiff,mandelRange.maxRe,mandelRange.minRe);
	
	long double sX = sizeX;
	long double sY = sizeY;
	mpf_set_d(tmp, sY/sX);
	//	maxIm = minIm + (maxRe - minRe) * sizeY / sizeX;
	mpf_mul(tmp,tmpDiff,tmp);
	mpf_add(mandelRange.maxIm,mandelRange.minIm,tmp);
	//	re_factor = (maxRe - minRe) / (sizeX - 1);
	mpf_div_ui(re_factor, tmpDiff, sX-1.0);
	//	im_factor = (maxIm - minIm) / (sizeY - 1);	
	mpf_sub(tmpDiff,mandelRange.maxIm,mandelRange.minIm);
	mpf_div_ui(im_factor,tmpDiff, sY-1.0);
	
	
	//	glBegin(GL_POINTS);
	for (int32_t y = tile->fromY; y < tile->toY; ++y) {
		//c_im = maxIm - y * im_factor;
		mpf_mul_ui(tmpDiff, im_factor, y);
		mpf_sub(c_im,mandelRange.maxIm,tmpDiff);
		for (int32_t x = tile->fromX; x < tile->toX; ++x) {
			//c_re = minRe + x * re_factor;
			mpf_mul_ui(tmpDiff, re_factor, x);
			mpf_add(c_re,mandelRange.minRe,tmpDiff);
			
			mpf_set(z_re,c_re);
			mpf_set(z_im,c_im);
			
			int32_t isInside = 1;
			for (uint32_t n = 0; n < maxIterations; ++n) {
				//Z_re2 = Z_re * Z_re;
				mpf_mul(z_re2,z_re,z_re);
				//Z_im2 = Z_im * Z_im;
				mpf_mul(z_im2,z_im,z_im);
				
				//Z_re2 + Z_im2 > 4
				mpf_add(tmp, z_re2, z_im2);
				if (mpf_cmp_ui(tmp,4) > 0) {
					isInside = 0;
					float red;
					float green;
					float blue;
					//#############################################################
					if (n <= maxIterations / 2 - 1) {
						//blackToRed
						float full = (maxIterations / 2 - 1);
						float r = ((n * 100) / full) / 100;
						red = r;
						green = 0.0f;
						blue = 0.0f;						
					} else if (n >= maxIterations / 2 && n <= maxIterations - 1) {
						//redToWhite
						float full = maxIterations - 1;
						float r = ((n * 100) / full) / 100;
						red = 1.0f;
						green = r;
						blue = r;						
					}
					if (fileMode) {
						writeToFile(threadNum, x,y,red,green,blue);
					} else {
						pixelListPtr pointPtr = createPixelElement();
						pointPtr->x = x;
						pointPtr->y = y;
						pointPtr->red = red;
						pointPtr->green = green;
						pointPtr->blue = blue;					
						insertInto(&pixelListHeads[threadNum], pointPtr);
					}
					//#############################################################
					break;
				}
				//Z_im = 2 * Z_re * Z_im + c_im;
				mpf_mul_ui(tmp, z_re, 2);
				mpf_mul(tmp,tmp,z_im);
				mpf_add(z_im,tmp,c_im);
				
				//Z_re = Z_re2 - Z_im2 + c_re;
				mpf_sub(tmp,z_re2,z_im2);
				mpf_add(z_re,tmp,c_re);
				
				// Thread hat den Stop-Befehl bekommen.
				if(threadStop == 1) {
					return;
				}
			}
			if (isInside) {
				if (fileMode) {
					writeToFile(threadNum, x,y,0.0f,0.0f,0.0f);
				} else {
					pixelListPtr pointPtr = createPixelElement();
					pointPtr->red = 0.0f;
					pointPtr->green = 0.0f;
					pointPtr->blue = 0.0f;						
					pointPtr->x = x;
					pointPtr->y = y;
					insertInto(&pixelListHeads[threadNum], pointPtr);
				}
			}
		}
	}
	mpf_clear(c_im);
	mpf_clear(c_re);
	mpf_clear(z_im);
	mpf_clear(z_re);
	mpf_clear(z_im2);
	mpf_clear(z_re2);
	mpf_clear(re_factor);
	mpf_clear(im_factor);
	mpf_clear(tmpDiff);
	mpf_clear(tmp);
	printf("Fertig mit: %ix%ix%ix%i\n",tile->fromX,tile->toX,tile->fromY,tile->toY);
	
}

void render(void) {
	glClear(GL_COLOR_BUFFER_BIT);
	glLoadIdentity();
	
	glBegin(GL_POINTS);
	for (int i = 0; i<numThreads; i++) {
		if(pixelListHeads != NULL) {
			pixelListPtr *p = &(pixelListHeads[i].first);
			while ((*p) != NULL) {
				glColor3f((*p)->red, (*p)->green, (*p)->blue);
				glVertex2f((*p)->x,(*p)->y);
				p = &(*p)->next;
			}
		}
	}
	glEnd();
	if (mousePosX > 0) {
		glBegin(GL_LINES);
		glColor3f(1.0f,1.0f,1.0f);
		if(!rightMouse) {
			glVertex2f(selectZoomX,selectZoomY);
			glVertex2f(selectZoomX,mousePosY);
			
			glVertex2f(selectZoomX,mousePosY);
			glVertex2f(mousePosX,mousePosY);
			
			glVertex2f(mousePosX,mousePosY);
			glVertex2f(mousePosX,selectZoomY);

			glVertex2f(mousePosX,selectZoomY);
			glVertex2f(selectZoomX,selectZoomY);
		} else {
			glVertex2f(selectMoveX,selectMoveY);
			glVertex2f(mousePosX,mousePosY);
		}
		glEnd();
	}
	glutSwapBuffers();
}

void repaint() {
	usleep(100000);
	glutPostRedisplay();
}

void freeMem() {
	stopThreads();
	mpf_clear(mandelRange.minRe);
	mpf_clear(mandelRange.maxRe);
	mpf_clear(mandelRange.minIm);
	mpf_clear(mandelRange.maxIm);
	cleanMemory();
}

void reshape(int32_t w, int32_t h)
{
	sizeX = w;
	sizeY = h;
	glViewport(0, 0, w, h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	if (h == 0) h = 1;
	gluOrtho2D(0.0, w, h, 0.0); 	
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	restart();
}

int main(int argc, char* argv[]) {
	init();
	atexit(freeMem);
	if(argc <= 1) { // rendern zu OpenGL
		fileMode = 0;
		glutInit(&argc, argv);
		glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
		glutInitWindowSize(480, 480);
		glutCreateWindow("Mandelbrot");	
		initGL();
		glutKeyboardFunc(keyboard);
		glutMouseFunc(mouse);
		glutMotionFunc(mousemove);
		glutDisplayFunc(render);
		glutIdleFunc(repaint);
		glutReshapeFunc(reshape);
		glutMainLoop();
	} else { // rendern zur Datei
		fileMode = 1;
		sizeX = 1000;
		sizeY = 1000;
		startThreads();
		waitForThreads();
	}
	return EXIT_SUCCESS;	
}
