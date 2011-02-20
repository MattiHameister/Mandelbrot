#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
#include <math.h>
#include <OpenGL/gl.h>
#include <GLUT/glut.h>
#include <pthread.h>
#include <gmp.h>
//#include "doubleGMP.h"

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

uint32_t maxIterations=100;
uint32_t iterDelta=25;
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
	uint8_t red;
	uint8_t green;
	uint8_t blue;
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

void writeToFile(uint32_t num, uint32_t x, uint32_t y, uint8_t red, uint8_t green, uint8_t blue) {
	FILE *f = files[num];
	//	fwrite(&x, sizeof(x), 1, f);
	//	fwrite(&y, sizeof(y), 1, f);
	//	fwrite(&r, sizeof(r), 1, f);
	//	fwrite(&g, sizeof(g), 1, f);
	//	fwrite(&b, sizeof(b), 1, f);
	//	uint8_t red = 255*r;
	//	uint8_t green = 255*g;
	//	uint8_t blue = 255*b;
	// TODO swappen, wenn BigEndian
	fwrite(&blue, sizeof(blue), 1, f);
	fwrite(&green, sizeof(green), 1, f);
	fwrite(&red, sizeof(red), 1, f);	
}

//################File########################

//###############Thread########################
void renderImage(screenTiles *tile, uint32_t threadNum);

uint32_t numThreads = 8;
pthread_t *threads = NULL;
uint32_t *thread_args  = NULL;
pixelHead *pixelListHeads  = NULL;
uint8_t threadStop = 0;
screenTiles *tiles = NULL;

void cleanMemory() {
	free(tiles);
	free(files);
	free(threads);
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
	int32_t offSetY=0;
	int32_t dY = sizeY / numSplit;
	int32_t rest = sizeY % numSplit;
	int num = 0;
	for (int32_t y = 0; y<numSplit; y++) {
		tiles[num].fromX = 0;
		tiles[num].toX = sizeX;
		tiles[num].fromY = offSetY;
		tiles[num].toY = offSetY+dY;
		// Wenn ein Teilungsrest da ist, muss der aufaddiert werden, sonst fehlt was vom Bild
		if(y == numSplit-1) {
			tiles[num].toY += rest;
		}
		offSetY+=dY;
		num++;
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

void loadPosition() {
	FILE *file;
	int32_t base = 10;
	
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
		setBits(gmpBit);
	}
	fclose(file);
	printf("Position geladen\n");
	
}

void keyboard(uint8_t key, int32_t x, int32_t y)
{
	FILE *file;
	int32_t base = 10;
	
	switch (key) {
		case 27: // ESC
			stopThreads();
			exit(0);
			break;
			
		case 105: // i
			maxIterations+=iterDelta;
			printf("Iterationen: %u\n",maxIterations);
			restart();
			break;
			
		case 111: // o
			maxIterations-=iterDelta;
			if (maxIterations <= 0) {
				maxIterations+=iterDelta;
			}
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
			loadPosition();
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

void savePixel(uint32_t threadNum, uint32_t x, uint32_t y, uint8_t red, uint8_t green, uint8_t blue) {
	if (fileMode) {
		writeToFile(threadNum, x, y, red, green, blue);
	} else {
		pixelListPtr pointPtr = createPixelElement();
		pointPtr->x = x;
		pointPtr->y = y;
		pointPtr->red = red;
		pointPtr->green = green;
		pointPtr->blue = blue;
		insertInto(&pixelListHeads[threadNum], pointPtr);
	}
}

//void renderPixel(uint32_t x, uint32_t y, uint32_t threadNum, mpf_t z_re, mpf_t z_im, mpf_t z_re2, mpf_t z_im2, mpf_t c_re, mpf_t c_im, mpf_t tmp, uint8_t *red, uint8_t *green, uint8_t *blue) {
//	mpf_set(z_re,c_re);
//	mpf_set(z_im,c_im);
//	
//	int32_t isInside = 1;
//	for (uint32_t n = 0; n < maxIterations; ++n) {
//		//Z_re2 = Z_re * Z_re;
//		mpf_mul(z_re2,z_re,z_re);
//		//Z_im2 = Z_im * Z_im;
//		mpf_mul(z_im2,z_im,z_im);
//		
//		//Z_re2 + Z_im2 > 4
//		mpf_add(tmp, z_re2, z_im2);
//		if (mpf_cmp_ui(tmp,4) > 0) {
//			isInside = 0;
//			*red = 0.0f;
//			*green = 0.0f;
//			*blue = 0.0f;
//			//#############################################################
//			if (n <= maxIterations / 2 - 1) {
//				//blackToRed
//				float full = (maxIterations / 2 - 1);
//				float f = (((n * 100) / full) / 100)*255;
//				*red = 0;
//				*green = 0;
//				*blue = f;						
//			} else if (n >= maxIterations / 2 && n <= maxIterations - 1) {
//				//redToWhite
//				float full = maxIterations - 1;
//				float f = (((n * 100) / full) / 100)*255;
//				*red = f;
//				*green = f;
//				*blue = 255;
//			}
//			savePixel(threadNum, x, y, *red, *green, *blue);
//			//#############################################################
//			break;
//		}
//		//Z_im = 2 * Z_re * Z_im + c_im;
//		mpf_mul_ui(tmp, z_re, 2);
//		mpf_mul(tmp,tmp,z_im);
//		mpf_add(z_im,tmp,c_im);
//		
//		//Z_re = Z_re2 - Z_im2 + c_re;
//		mpf_sub(tmp,z_re2,z_im2);
//		mpf_add(z_re,tmp,c_re);
//		
//		// Thread hat den Stop-Befehl bekommen.
//		if(threadStop == 1) {
//			return;
//		}
//		
//	}
//	if (isInside) {
//		savePixel(threadNum, x, y, 0, 0, 0);
//	}
//}

//void calcCRe(mpf_t tmpDiff, mpf_t re_factor, uint32_t x, mpf_t minRe, mpf_t c_re) {
//	mpf_mul_ui(tmpDiff, re_factor, x);
//	mpf_add(c_re, minRe, tmpDiff);
//}
//
//void calcCIm(mpf_t tmpDiff, mpf_t im_factor, uint32_t y, mpf_t maxIm, mpf_t c_im) {
//	mpf_mul_ui(tmpDiff, im_factor, y);
//	mpf_sub(c_im, maxIm, tmpDiff);
//}

//void renderImage(screenTiles *tile, uint32_t threadNum) {
//	printf("Rendere: %ix%ix%ix%i\n",tile->fromX,tile->toX,tile->fromY,tile->toY);
//	
//	mpf_t tmpDiff;
//	mpf_t tmp;
//	mpf_t re_factor;
//	mpf_t im_factor;
//	mpf_t c_im;
//	mpf_t c_re;
//	mpf_t z_re;
//	mpf_t z_im;
//	mpf_t z_re2;
//	mpf_t z_im2;
//	
//	mpf_init2(tmpDiff,gmpBit);
//	mpf_init2(tmp,gmpBit);
//	mpf_init2(re_factor,gmpBit);
//	mpf_init2(im_factor,gmpBit);
//	mpf_init2(c_im,gmpBit);
//	mpf_init2(c_re,gmpBit);
//	mpf_init2(z_im,gmpBit);
//	mpf_init2(z_re,gmpBit);
//	mpf_init2(z_im2,gmpBit);
//	mpf_init2(z_re2,gmpBit);
//	
//	int pixelX = tile->toX - tile->fromX;
//	int pixelY = tile->toY - tile->fromY;
//	
//	int splitX = 10;//sizeX / 100;
//	int splitY = 10;//pixelY / numThreads/2;
//	int boxPixelCountX = pixelX / splitX;
//	int boxPixelCountXLeft = pixelX % splitX;
//	int boxPixelCountY = pixelY / splitY;
//	int boxPixelCountYLeft = pixelY % splitY;
//	printf("SplitX %i, SplitY %i, Boxes: %i+%i | %i+%i\n",splitX, splitY, boxPixelCountX,boxPixelCountXLeft,boxPixelCountY,boxPixelCountYLeft);
//	
//	// Einteilung in einzelne Bereiche
//	int count = splitX*splitY;
//	screenTiles *boxes = calloc(count, sizeof(screenTiles));
//	
//	uint32_t xOffset = 0;
//	uint32_t yOffset = 0;
//	int box = 0;
//	for (int y = 0; y < splitY; y++) {
//		for (int x = 0; x < splitX; x++) {
//			if(box >= count) {
//				printf("SPEICHERUEBERLAUF!%i <> %i\n", box, count);
//				exit(1);
//			}
//			boxes[box].fromX = tile->fromX + xOffset;
//			boxes[box].toX = boxes[box].fromX + boxPixelCountX;
//			if(x == splitX-1) {
//				boxes[box].toX += boxPixelCountXLeft;
//			}
//			
//			boxes[box].fromY = tile->fromY + yOffset;
//			boxes[box].toY = boxes[box].fromY + boxPixelCountY;
//			if(y == splitY-1) {
//				boxes[box].toY += boxPixelCountYLeft;
//			}
//			printf("BOX: %i|%i|%i|%i\n",boxes[box].fromX,boxes[box].toX,boxes[box].fromY,boxes[box].toY);
//			xOffset += boxPixelCountX;
//			box++;
//		}
//		yOffset += boxPixelCountY;
//		xOffset = 0;
//	}
//	
//	mpf_sub(tmpDiff,mandelRange.maxRe,mandelRange.minRe);
//	
//	long double sX = sizeX;
//	long double sY = sizeY;
//	mpf_set_d(tmp, sY/sX);
//	//	maxIm = minIm + (maxRe - minRe) * sizeY / sizeX;
//	mpf_mul(tmp,tmpDiff,tmp);
//	mpf_add(mandelRange.maxIm,mandelRange.minIm,tmp);
//	//	re_factor = (maxRe - minRe) / (sizeX - 1);
//	mpf_set_d(tmp, sX-1.0);
//	mpf_div(re_factor, tmpDiff, tmp);
//	//	im_factor = (maxIm - minIm) / (sizeY - 1);	
//	mpf_sub(tmpDiff,mandelRange.maxIm,mandelRange.minIm);
//	mpf_set_d(tmp, sY-1.0);
//	mpf_div(im_factor,tmpDiff, tmp);
//	
//	uint8_t red = 0;
//	uint8_t green = 0;
//	uint8_t blue = 0;
//	
//	uint32_t lead = 0;
//	
//	for(int b = 0; b<count; b++) {
//		uint8_t finished = 0;
//		while (!finished) {
//			int xk = boxes[b].fromX+lead;
//			if((boxes[b].toY-lead) - (boxes[b].fromX+lead) < 6) {
//				calcCRe(tmpDiff, re_factor, xk, mandelRange.minRe, c_re);		
//				for(uint32_t y = boxes[b].fromY+lead; y < boxes[b].toY-lead; y++) {	
//					calcCIm(tmpDiff, im_factor, y, mandelRange.maxIm, c_im);			
//					renderPixel(xk, y, threadNum, z_re, z_im, z_re2, z_im2, c_re, c_im, tmp, &red, &green, &blue);
//				}
//				
//				xk = boxes[b].toX-lead-1;
//				calcCRe(tmpDiff, re_factor, xk, mandelRange.minRe, c_re);		
//				for(uint32_t y = boxes[b].fromY+lead; y < boxes[b].toY-lead; y++) {			
//					calcCIm(tmpDiff, im_factor, y, mandelRange.maxIm, c_im);			
//					renderPixel(xk, y, threadNum, z_re, z_im, z_re2, z_im2, c_re, c_im, tmp, &red, &green, &blue);
//				}
//				
//				int yk = boxes[b].fromY+lead;
//				calcCIm(tmpDiff, im_factor, yk, mandelRange.maxIm, c_im);			
//				for(uint32_t x = boxes[b].fromX+lead+1; x < boxes[b].toX-lead-1; x++) {			
//					calcCRe(tmpDiff, re_factor, x, mandelRange.minRe, c_re);		
//					renderPixel(x, yk, threadNum, z_re, z_im, z_re2, z_im2, c_re, c_im, tmp, &red, &green, &blue);
//				}
//				
//				yk = boxes[b].toY-lead-1;
//				calcCIm(tmpDiff, im_factor, yk, mandelRange.maxIm, c_im);			
//				for(uint32_t x = boxes[b].fromX+lead+1; x < boxes[b].toX-lead-1; x++) {			
//					calcCRe(tmpDiff, re_factor, x, mandelRange.minRe, c_re);		
//					renderPixel(x, yk, threadNum, z_re, z_im, z_re2, z_im2, c_re, c_im, tmp, &red, &green, &blue);
//				}
//				
//				lead++;
//				//			printf("%i>=%i\n",lead*2+boxes[b].fromX,boxes[b].toX-lead);
//				if(lead*2+boxes[b].fromX>=boxes[b].toX-lead) {
//					finished = 1;
//				}
//			} else {
//				for(int b = 0; b<count; b++) {
//					for (int32_t y = boxes[b].fromY+lead; y < boxes[b].toY-lead; ++y) {
//						//c_im = maxIm - y * im_factor;
//						calcCIm(tmpDiff, im_factor, y, mandelRange.maxIm, c_im);			
//						for (int32_t x = boxes[b].fromX+lead; x < boxes[b].toX-lead; ++x) {
//							printf("HAAAAAAAALLLLOOO\n%ix%i",x,y);
//							//c_re = minRe + x * re_factor;
//							calcCRe(tmpDiff, re_factor, x, mandelRange.minRe, c_re);		
//							
//							renderPixel(x, y, threadNum, z_re, z_im, z_re2, z_im2, c_re, c_im, tmp, &red, &green, &blue);
//						}
//					}
//				}	
//				finished = 1;
//			}
//			
//			if(threadStop == 1) {
//				break;
//			}
//		}
//		lead = 0;
//		if(threadStop == 1) {
//			break;
//		}
//	}
//	
//	//	for(int b = 0; b<count; b++) {
//	//		for (int32_t y = boxes[b].fromY; y < boxes[b].toY; ++y) {
//	//			//c_im = maxIm - y * im_factor;
//	//			mpf_mul_ui(tmpDiff, im_factor, y);
//	//			mpf_sub(c_im,mandelRange.maxIm,tmpDiff);
//	//			for (int32_t x = boxes[b].fromX; x < boxes[b].toX; ++x) {
//	//				//c_re = minRe + x * re_factor;
//	//				mpf_mul_ui(tmpDiff, re_factor, x);
//	//				mpf_add(c_re,mandelRange.minRe,tmpDiff);
//	//				
//	//				renderPixel(x, y, threadNum, z_re, z_im, z_re2, z_im2, c_re, c_im, tmp, &red, &green, &blue);
//	//			}
//	//		}
//	//	}
//	
//	free(boxes);
//	mpf_clear(c_im);
//	mpf_clear(c_re);
//	mpf_clear(z_im);
//	mpf_clear(z_re);
//	mpf_clear(z_im2);
//	mpf_clear(z_re2);
//	mpf_clear(re_factor);
//	mpf_clear(im_factor);
//	mpf_clear(tmpDiff);
//	mpf_clear(tmp);
//	printf("Fertig mit: %ix%ix%ix%i\n",tile->fromX,tile->toX,tile->fromY,tile->toY);
//}

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
	mpf_set_d(tmp, sX-1.0);
	mpf_div(re_factor, tmpDiff, tmp);
	//	im_factor = (maxIm - minIm) / (sizeY - 1);	
	mpf_sub(tmpDiff,mandelRange.maxIm,mandelRange.minIm);
	mpf_set_d(tmp, sY-1.0);
	mpf_div(im_factor,tmpDiff, tmp);
	
	
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
					uint8_t red = 0;
					uint8_t green = 0;
					uint8_t blue = 0;
					//#############################################################
					if (n <= maxIterations / 2 - 1) {
						//blackToRed
						float full = (maxIterations / 2 - 1);
						float f = (((n * 100) / full) / 100)*255;
						red = f;
						green = 0;
						blue = 0;						
					} else if (n >= maxIterations / 2 && n <= maxIterations - 1) {
						//redToWhite
						float full = maxIterations - 1;
						float f = (((n * 100) / full) / 100)*255;
						red = 255;
						green = f;
						blue = f;
					}
					savePixel(threadNum, x, y, red, green, blue);
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
				savePixel(threadNum, x, y, 0, 0, 0);
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
				float r = (*p)->red / 255.0f;
				float g = (*p)->green / 255.0f;
				float b = (*p)->blue / 255.0f;
				glColor3f(r, g, b);
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
	stopThreads();
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

void tgaSaveHeader(FILE *file, short int width, short int height, unsigned char pixelDepth) {
	unsigned char cGarbage = 0, type, mode;
	short int iGarbage = 0;
	
	mode = pixelDepth / 8;
	if ((pixelDepth == 24) || (pixelDepth == 32))
		type = 2;
	else
		type = 3;
	
	fwrite(&cGarbage, sizeof(unsigned char), 1, file);
	fwrite(&cGarbage, sizeof(unsigned char), 1, file);
	
	fwrite(&type, sizeof(unsigned char), 1, file);
	
	fwrite(&iGarbage, sizeof(short int), 1, file);
	fwrite(&iGarbage, sizeof(short int), 1, file);
	fwrite(&cGarbage, sizeof(unsigned char), 1, file);
	fwrite(&iGarbage, sizeof(short int), 1, file);
	fwrite(&iGarbage, sizeof(short int), 1, file);
	
	fwrite(&width, sizeof(short int), 1, file);
	fwrite(&height, sizeof(short int), 1, file);
	fwrite(&pixelDepth, sizeof(unsigned char), 1, file);
	
	fwrite(&cGarbage, sizeof(unsigned char), 1, file);
}

#define BUFFER_SIZE 32768

// Einzelne Threads schrieben TGA Daten
// Diese muessen jetzt nur noch zusammengefuegt werden
void writeTGA() {
	printf("speichere TGA\n");
	FILE *file = fopen("Ergebnis.tga", "wb");
	if (file != NULL) {
		tgaSaveHeader(file, sizeX,sizeY, 24);
		uint8_t counter = 0;
		for(int i=0; i<numThreads; i++) {
			char buf[255];
			snprintf(buf, 255, "%d.xyrgb", i);
			printf("verarbeite Datei %s\n",buf);
			FILE *result = fopen(buf, "rb");
			if(result != NULL) {
				char buffer[BUFFER_SIZE];
				rewind(result);
				uint32_t read = 0;
				uint32_t write = 0;
				do {
					read = fread(&buffer, 1, BUFFER_SIZE, result);
					if(read > 0) {
						write = fwrite(buffer, 1, read, file);
						if(read != write) {
							printf("Es ist ein Fehler aufgetreten %d, %d\n",read, write);
						}
					}
					// Datei ab und zu mal auf Festplatte schreiben
					counter++;
					if (counter >= 100) {
						counter = 0;
						fflush(file);
					}
				} while (read > 0);
			}
			fclose(result);
			remove(buf);
		}	
	}
	fclose(file);
}

int main(int argc, char* argv[]) {
	init();
	atexit(freeMem);
	if(argc <= 1) { // rendern zu OpenGL
		fileMode = 0;
		glutInit(&argc, argv);
		glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
		glutInitWindowSize(633, 641);
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
		sizeX = 1024;
		sizeY = 1024;
		maxIterations=500;
		//		loadPosition();
		startThreads();
		waitForThreads();
		writeTGA();
	}
	return EXIT_SUCCESS;	
}
