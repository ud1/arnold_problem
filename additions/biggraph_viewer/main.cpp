#include <Windows.h>
#include <gl/GL.h>
#include <stdio.h>
#include <vector>
#include <map>

using namespace std;

#pragma comment( lib, "opengl32.lib" )

HDC hDC;
int width, height;

void Reshape() {
	glViewport (0, 0, (GLsizei)(width), (GLsizei)(height));
	glMatrixMode (GL_PROJECTION);
	glLoadIdentity ();	
	glOrtho(0.0, (GLdouble)1.0, 
		(GLdouble)1.0, 0.0,
		-100.0,100.0);
	glMatrixMode (GL_MODELVIEW);
	glLoadIdentity ();
	glDisable(GL_DEPTH_TEST);
	glDepthMask(0);
}

float cenx = 0, ceny = 0;
float zoom = 1.0f;
bool mouse_pressed = false;
int mx, my;

void lbdown(int x, int y) {
	mouse_pressed = true;
	mx = x;
	my = y;
}

void lbup(int x, int y) {
	mouse_pressed = false;
}

void mmove(int x, int y) {
	if (mouse_pressed) {
		cenx += (float) (x - mx) / zoom / width;
		ceny += (float) (y - my) / zoom / height;

		mx = x;
		my = y;
	}
}

void on_char(char ch) {
	if (ch == '+')
		zoom *= 1.1;
	if (ch == '-')
		zoom /= 1.1;
}

LRESULT CALLBACK WindowProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg){
		case WM_SIZE:
			width = LOWORD(lParam);
			height = HIWORD(lParam);
			Reshape();
			break;
		case WM_LBUTTONDOWN:
			lbdown(LOWORD(lParam), HIWORD(lParam));
			break;

		case WM_LBUTTONUP:
			lbup(LOWORD(lParam), HIWORD(lParam));
			break;

		case WM_MOUSEMOVE:
			mmove(LOWORD(lParam), HIWORD(lParam));
			break;
		case WM_CHAR:
			on_char(wParam);
			break;
	}
	return DefWindowProc (hWnd, uMsg, wParam, lParam);
}

// Регистрирует класс окна
BOOL RegisterWindowClass() {
	WNDCLASSEX windowClass;												// Window Class
	ZeroMemory( &windowClass, sizeof( WNDCLASSEX ) );					// Make Sure Memory Is Cleared
	windowClass.cbSize			= sizeof (WNDCLASSEX);					// Size Of The windowClass Structure
	windowClass.style			= CS_HREDRAW | CS_VREDRAW | CS_OWNDC;	// Redraws The Window For Any Movement / Resizing
	windowClass.lpfnWndProc		= (WNDPROC)(WindowProc);				// WindowProc Handles Messages
	windowClass.hInstance		= GetModuleHandle(NULL);				// Set The Instance
	windowClass.hbrBackground	= (HBRUSH)(COLOR_APPWORKSPACE);			// Class Background Brush Color
	windowClass.hCursor			= LoadCursor(NULL, IDC_ARROW);			// Load The Arrow Pointer
	windowClass.lpszClassName	= TEXT("gl_test");	// Sets The Applications Classname
	if (RegisterClassEx (&windowClass) == 0)							// Did Registering The Class Fail?
	{
		return FALSE;													// Return False (Failure)
	}
	return TRUE;														// Return True (Success)
}

BOOL BaseInitialize(HWND hWnd) {
	PIXELFORMATDESCRIPTOR pfd;
	pfd.nSize			= sizeof (PIXELFORMATDESCRIPTOR);	
	pfd.nVersion		= 1;
	pfd.dwFlags			= PFD_DRAW_TO_WINDOW |
		PFD_SUPPORT_OPENGL |
		PFD_DOUBLEBUFFER;
	pfd.iPixelType		= PFD_TYPE_RGBA;
	pfd.cColorBits		= 24;
	pfd.cRedBits		=
		pfd.cRedShift	=
		pfd.cGreenBits	=
		pfd.cGreenShift	=
		pfd.cBlueBits	=
		pfd.cBlueShift	= 0;
	pfd.cAlphaBits		=
		pfd.cAlphaShift	= 0;								// Нет альфа буфера
	pfd.cAccumBits		= 0;								// Нет Accumulation Buffer'а
	pfd.cAccumRedBits	=
		pfd.cAccumGreenBits =
		pfd.cAccumBlueBits	= 
		pfd.cAccumAlphaBits	= 0;							// Accumulation биты игнорируем

	pfd.cDepthBits		= 0;
	pfd.cStencilBits	= 0;
	pfd.cAuxBuffers		= 0;
	pfd.iLayerType		= 0;								// Главный слой для отрисовки (больше не используется)
	pfd.bReserved		= 0;
	pfd.dwLayerMask		= 0;								// Игнорируем маску слоёв(больше не используется)
	pfd.dwVisibleMask	= 0;
	pfd.dwDamageMask	= 0;								// (больше не используется)

	int pixelFormat = ChoosePixelFormat (hDC, &pfd);	// Ищем совместимый Pixel Format
	if (pixelFormat == 0) {
		return FALSE;
	}

	if (SetPixelFormat (hDC, pixelFormat, NULL) == FALSE) {
		return FALSE;
	}

	HGLRC hRC = wglCreateContext (hDC);
	if (hRC == 0) {
		return FALSE;
	}

	if (wglMakeCurrent (hDC, hRC) == FALSE) {
		wglDeleteContext (hRC);
		return FALSE;
	}

	Reshape();
	return TRUE;
}

map<int, vector<int> > confs;
map<int, int> sv;
vector<int> *links, *inv_links;
int count;

struct vert {
	float x, y;
} *verteces;

unsigned int *indices;


void Render() {
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glLoadIdentity();


	glTranslatef(0.5, 0.5, 0);
	glScalef(zoom, zoom, 1);

	glTranslatef(cenx, ceny, 0);
	glTranslatef(-0.5, -0.5, 0);


	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_FLOAT, sizeof(vert), (GLvoid *)verteces);///
	glDrawElements(GL_LINES, count*2, GL_UNSIGNED_INT, (GLvoid *) indices);
	glDisableClientState(GL_VERTEX_ARRAY);

	SwapBuffers(hDC);
	Sleep(20);
}

struct nd {
	float x;
	int ind;
} *b_line;

int cmp_nd(const void *a, const void *b) {
	float r = ((nd *) a)->x - ((nd *) b)->x;
	if (r < 0)
		return -1;
	if (r > 0)
		return 1;
	return 0;
}

int mins;
void sort_graph() {
	int i, j;
	map<int, vector<int> >::iterator it;

	for (it = confs.begin(); it != confs.end(); ++it) {
		for (i = 0; i < it->second.size(); ++i) {
			if (it->first != mins) {
				verteces[it->second[i]].x = 0;
				for (j = 0; j < links[it->second[i]].size(); ++j) {
					verteces[it->second[i]].x += verteces[links[it->second[i]][j]].x;
				}
				for (j = 0; j < inv_links[it->second[i]].size(); ++j) {
					verteces[it->second[i]].x += verteces[inv_links[it->second[i]][j]].x;
				}
				verteces[it->second[i]].x /= (links[it->second[i]].size() + inv_links[it->second[i]].size());
			}
		}
	}

	for (i = 0; i < confs[mins].size(); ++i) {
		b_line[i].x = 0;
		for (j = 0; j < links[b_line[i].ind].size(); ++j) {
			b_line[i].x += verteces[links[b_line[i].ind][j]].x;
		}
		for (j = 0; j < inv_links[b_line[i].ind].size(); ++j) {
			b_line[i].x += verteces[inv_links[b_line[i].ind][j]].x;
		}
		b_line[i].x /= (links[b_line[i].ind].size() + inv_links[b_line[i].ind].size());
	}

	qsort((void *) b_line, confs[mins].size(), sizeof(nd), cmp_nd);
	float len = b_line[confs[mins].size()-1].x - b_line[0].x;
	for (i = 0; i < confs[mins].size(); ++i) {
		verteces[b_line[i].ind].x = (b_line[i].x - b_line[0].x) / len;
		//verteces[b_line[i].ind].x = (float) i / (float) confs[mins].size();
	}
}

int main() {
	FILE *f;
	f = fopen("7s.txt", "r");
	if (!f)
		return 0;

	int i, s;

	mins = 1000;
	while(fscanf(f, "%d %d", &i, &s) > 0) {
 		sv[i-1] = s;
		confs[s].push_back(i-1);
		if (mins > s)
			mins = s;
	}

	fclose(f);
	

	f = fopen("7l.txt", "r");
	if (!f)
		return 0;

	links = new vector<int>[sv.size()];
	inv_links = new vector<int>[sv.size()];
	int i1, i2;
	count = 0;
	while(fscanf(f, "%d %d", &i1, &i2) > 0) {
		links[i1-1].push_back(i2-1);
		inv_links[i2-1].push_back(i1-1);
		++count;
	}

	fclose(f);

	indices = new unsigned int[2*count];
	for (i = 0, i1 = 0; i1 < sv.size(); ++i1) {
		for (i2 = 0; i2 < links[i1].size(); ++i2) {
			indices[i++] = i1;
			indices[i++] = links[i1][i2];
		}
	}

	verteces = new vert[sv.size()];

	map<int, vector<int> >::iterator it;
	for (it = confs.begin(); it != confs.end(); ++it) {
		for (i = 0; i < it->second.size(); ++i) {
			verteces[it->second[i]].x = (float) i / (float) it->second.size();
			verteces[it->second[i]].y = (float) it->first / (float) confs.size() / 2.0;
		}
	}

	b_line = new nd[confs[mins].size()];
	for (i = 0; i < confs[mins].size(); ++i) {
		b_line[i].ind = confs[mins][i];
		b_line[i].x = verteces[confs[mins][i]].x;
	}


	if ( !RegisterWindowClass() ) {
		MessageBox (HWND_DESKTOP, TEXT("RegisterClassEx Failed!"), TEXT("Error"), MB_OK | MB_ICONEXCLAMATION);
		return 0;
	}

	// Создание Windows окна
	DWORD	windowStyle			= WS_POPUP | WS_OVERLAPPEDWINDOW;		// Стиль окна
	DWORD	windowExtendedStyle = WS_EX_APPWINDOW;			// 

	// Cоздаем окно
	HWND hWnd = CreateWindowEx (
		windowExtendedStyle,				// Extended Style
		TEXT("gl_test"),	// Class Name
		TEXT("gl test"),					// Window Title
		windowStyle,						// Window Style
		0, 0,			// Window x,y Position
		width = 800,					// Window Width
		height = 600,					// Window Height
		HWND_DESKTOP,						// Desktop Is Window's Parent
		0,									// No Menu
		GetModuleHandle(NULL),				// Pass The Window Instance
		NULL);

	if ( !hWnd ) {
		MessageBox (HWND_DESKTOP, TEXT("Create window failed!"), TEXT("Error"), MB_OK | MB_ICONEXCLAMATION);
		return 0;
	}

	ShowWindow(hWnd, TRUE);

	hDC = GetDC(hWnd);
	if (!BaseInitialize(hWnd)) {
		MessageBox (HWND_DESKTOP, TEXT("GL initialization failed!"), TEXT("Error"), MB_OK | MB_ICONEXCLAMATION);
		return 0;
	}

	printf("%s\n", glGetString(GL_VERSION));

	MSG msg;
	while (1) {
		if (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE) != 0) {
			TranslateMessage(&msg);						// Переводим сообщение
			DispatchMessage (&msg);						// обработаем сообщение
		} else {
			Render();
			sort_graph();
		}
	}
	return 0;
}