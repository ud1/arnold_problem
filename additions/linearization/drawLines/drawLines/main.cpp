#include <stdlib.h>
#include <stdio.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <windows.h>

#include <gl/gl.h>		// OpenGL
#include "glut.h"	// GLUT

double size;
GLfloat fAspect;

int N, k;
double *a, *b;
int mousex, mousey;
bool LmousePressed = false;
bool RmousePressed = false;
double cenx = 0.0, ceny = 0.0;
double alpha = 0.0;

void GLUTRenderScene(void)
{
	int i;
	double len2, len, x, y;
	double L = 10.0;
	//RenderScene();
	glClear(GL_COLOR_BUFFER_BIT);
	glLoadIdentity();
	glRotatef(alpha, 0,0,1);
	glBegin(GL_LINES);
	for (i=0; i<N; ++i) {
		len2 = a[i]*a[i]+b[i]*b[i];
		len = sqrt(len2)/(size*200);
		if (len) {
			x = a[i]/len2 + cenx;
			y = b[i]/len2 + ceny;
			glVertex2f(x + b[i]/len, y - a[i]/len);
			glVertex2f(x - b[i]/len, y + a[i]/len);
		}
	}

	glEnd();


	// Copy image to window
	glutSwapBuffers();
	Sleep(10);
}

void GLUTIdleFunction(void)
{
	//IdleFunction();
	GLUTRenderScene();
}

void changeProj() {
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-size*fAspect,size*fAspect,-size,size,-10,10);
	glMatrixMode(GL_MODELVIEW);
}

void GLUTKeyDown( unsigned char key, int x, int y )
{
	// Key Bindings
	switch( key )
	{
	case '+': size/=1.1;changeProj();				break;
	case '-': size*=1.1;changeProj();				break;
		//case 's': KeyBackward();			break;
		//case 'd': KeyRight();				break;

		//case 'f': KeyAnimateToggle();		break;
		//case 'o': KeyObserveToggle();		break;
		//case 'q': KeyDrawModeSurf();		break;
		//case 'r': KeyDrawFrustumToggle();	break;

		//case '0': KeyMoreDetail();			break;
		//case '9': KeyLessDetail();			break;

		//case '1': KeyFOVDown();				break;
		//case '2': KeyFOVUp();				break;
	}
}

void GLUTKeySpecialDown( int key, int x, int y )
{
	// More key bindings
	switch( key )
	{
		//case GLUT_KEY_UP:	KeyUp();	break;
		//case GLUT_KEY_DOWN:	KeyDown();	break;
	}
}

void GLUTMouseClick(int button, int state, int x, int y)
{
	if ( button == GLUT_LEFT_BUTTON ) {
		if ( state == GLUT_DOWN ) {
			LmousePressed = true;
		} else {
			LmousePressed = false;
		}
	}
	if ( button == GLUT_RIGHT_BUTTON ) {
		if ( state == GLUT_DOWN ) {
			RmousePressed = true;
		} else {
			RmousePressed = false;
		}
	}
	mousex = x;
	mousey = y;
}
// Perspective & Window defines
#define FOV_ANGLE     (90.0f)
#define NEAR_CLIP     (1.0f)
#define FAR_CLIP      (2500.0f)
float gFovX = 90.0f;

void ChangeSize(GLsizei w, GLsizei h)
{
	// Prevent a divide by zero, when window is too short
	// (you cant make a window of zero width).
	if(h == 0)
		h = 1;

	// Set the viewport to be the entire window
	glViewport(0, 0, w, h);

	fAspect = (GLfloat)w/(GLfloat)h;
	changeProj();
	GLUTRenderScene();
}
void MouseMove(int mouseX, int mouseY) {
	double koef = size/500.0;
	if (LmousePressed) {
		cenx+=(mouseX-mousex)*koef;
		ceny-=(mouseY-mousey)*koef;
	}
	if (RmousePressed) {
		alpha+=mouseX-mousex;
	}
	mousex = mouseX;
	mousey = mouseY;
}
void SetupRC()
{
	glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
}

int main(int argc, char **argv) {
	size = 10.0;
	fAspect = 1.0;
	FILE *file;
	if (argc > 1) {
		printf("%s\n", argv[1]);
		file = fopen(argv[1], "rb");
		if (!file)
			return 0;
	} else return 0;

	fread(&N, sizeof(N), 1, file);
	fread(&k, sizeof(k), 1, file);
	printf("---%d lines, %d parallels---\n", N, k);
	a = new double[N];
	b = new double[N];
	int i;
	for (i=0; i<N; ++i) {
		fread(&a[i], sizeof(double), 1, file);
		fread(&b[i], sizeof(double), 1, file);
	}
	int *generators = NULL;
	if (!feof(file)) {
		int count = N*(N-1)/2-k;
		generators = new int[count];
		size_t cnt = fread(generators, sizeof(generators[0]), count, file);
		if (cnt == count) {
			printf("generators (%d):\n", count);
			int i;
			for (i=0; i<count; ++i) {
				printf("%d ", generators[i]);
			}
			printf("\n");
			int *l = new int[N];
			for (i=0; i<N; ++i) {
				l[i] = i;
			}
			for (i=0; i<count; ++i) {
				int gen = generators[i];
				int temp = l[gen];
				l[gen] = l[gen+1];
				l[gen+1] = temp;
			}
			int j;
			for (j=0; j<N; ++j) {
				if (j < 10)
					printf("%d ", j);
				else 
					printf("%d", j);
			}
			printf("\n");
			for (j=0; j<N; ++j) {
				printf("| ");
			}
			printf("\n");
			for (i=0; i<count;++i) {
				for (j=0; j<N; ++j) {
					if (generators[i] == j) {
						printf(" >");
					} else if (generators[i] == j-1) {
						printf("< ");
					} else if (i+1 < count && generators[i] == j-2 && generators[i+1]-2 >= generators[i]) {
						++i;--j;
					} else {
						printf("| ");
					}
				}
				printf("\n");
			}
			for (j=0; j<N; ++j) {
				if (l[j] < 10)
					printf("%d ", l[j]);
				else 
					printf("%d", l[j]);
			}
			printf("\n\n");
		}
	}
	fclose(file);
	//glutInit(&argc, argv);
	glutInitDisplayMode( GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutInitWindowPosition(0, 0);
	glutInitWindowSize(512, 512);
	if ( glutCreateWindow("Lines") < 0) {
		printf("ERROR: Create Window Failed!\n");
		exit(0);
	}
	glutReshapeFunc(ChangeSize);		// Set function to call when window is resized
	glutIdleFunc(GLUTIdleFunction);		// Set function to call when program is idle
	glutKeyboardFunc(GLUTKeyDown);
	glutSpecialFunc(GLUTKeySpecialDown);
	glutMouseFunc(GLUTMouseClick);
	glutMotionFunc(MouseMove);
	glutDisplayFunc(GLUTRenderScene);

	// Setup OpenGL
	SetupRC();
	glutMainLoop();
}