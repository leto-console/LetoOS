#ifdef __WIN__

#include "WinDisplay.hpp"

#ifndef UNICODE
#define UNICODE
#endif

#include <Windows.h>

#include <string>

#include <GraphicsPC/KeyEventPC.hpp>
#include <Input/KeyCodePC.hpp>

static bool closeFlag = false;

static const wchar_t* CLASS_NAME = L"STM32_GAMES";
static int SCALE 	= 1;
static int WIDTH	= 128;
static int HEIGHT 	= 64;

static uint32_t* imageData{};

static void DrawImage(HDC hdc)
{
	if (!imageData) return;

	BITMAPINFOHEADER bmih	= { 0 };
	bmih.biSize				= sizeof(BITMAPINFOHEADER);
	bmih.biWidth			= WIDTH;
	bmih.biHeight			= -HEIGHT;
	bmih.biPlanes			= 1;
	bmih.biBitCount			= 32;
	bmih.biCompression		= BI_RGB;
	bmih.biSizeImage		= 0;
	bmih.biXPelsPerMeter	= 10;
	bmih.biYPelsPerMeter	= 10;

	BITMAPINFO dbmi			= { 0 };
	dbmi.bmiHeader			= bmih;

	// Draw pixels without stretching
  //  SetDIBitsToDevice(hdc, 0, 0, SSD1306_Width, SSD1306_Height,
  //                    0, 0, 0, SSD1306_Height, imageData, &dbmi, 0 );

	StretchDIBits(hdc, 
		0, 0, WIDTH * SCALE, HEIGHT * SCALE,
		0, 0, WIDTH, HEIGHT,
		imageData, &dbmi, DIB_RGB_COLORS, SRCCOPY);
}

static LRESULT WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CREATE:
		closeFlag = false;
		break;
	case WM_MOVING:
		break;
	case WM_CLOSE:
		DestroyWindow(hWnd);
		break;
	case WM_DESTROY:
		// Флаг closeFlag добавлен для возможности перезапустить приложение
		closeFlag = true;
		//PostQuitMessage(0);
		return 0;
	case WM_KEYDOWN:
		KeyEventsPCMap[wParam].push_back({ wParam, KeyEventPC::PRESSED });
		//KeyEventsPCMap[wParam] = { wParam, KeyEventPC::PRESSED };
		//KeyEventPCs.push_back({ wParam, KeyEventPC::PRESSED });
		break;
	case WM_KEYUP:
		KeyEventsPCMap[wParam].push_back({ wParam, KeyEventPC::RELEASED });
		//KeyEventsPCMap[wParam] = { wParam, KeyEventPC::RELEASED };
		//KeyEventPCs.push_back({ wParam, KeyEventPC::RELEASED });
		break;
	case WM_MOUSEWHEEL:
		{
			uint64_t VK_MouseWheel = KeyCodePC::MOUSE_WHEEL;
			int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
			if (zDelta > 0)
			{
				// Действие при прокрутке ВВЕРХ (от пользователя)
				KeyEventsPCMap[VK_MouseWheel].push_back({VK_MouseWheel, KeyEventPC::ROTATE_LEFT});
			}
			else if (zDelta < 0)
			{
				// Действие при прокрутке ВНИЗ (к пользователю)
				KeyEventsPCMap[VK_MouseWheel].push_back({VK_MouseWheel, KeyEventPC::ROTATE_RIGHT});
			}
		}
		break;
	case WM_MBUTTONDOWN:
		KeyEventsPCMap[VK_MBUTTON].push_back({ VK_MBUTTON, KeyEventPC::PRESSED });
		break;
	case WM_MBUTTONUP:
		KeyEventsPCMap[VK_MBUTTON].push_back({ VK_MBUTTON, KeyEventPC::RELEASED });
		break;
	case WM_LBUTTONDOWN:
		KeyEventsPCMap[VK_LBUTTON].push_back({ VK_LBUTTON, KeyEventPC::PRESSED });
		break;
	case WM_LBUTTONUP:
		KeyEventsPCMap[VK_LBUTTON].push_back({ VK_LBUTTON, KeyEventPC::RELEASED });
		break;
	case WM_RBUTTONDOWN:
		KeyEventsPCMap[VK_RBUTTON].push_back({ VK_RBUTTON, KeyEventPC::PRESSED });
		break;
	case WM_RBUTTONUP:
		KeyEventsPCMap[VK_RBUTTON].push_back({ VK_RBUTTON, KeyEventPC::RELEASED });
		break;
	case WM_PAINT:
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		DrawImage(hdc);
		EndPaint(hWnd, &ps);
		break;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

WinDisplay::WinDisplay(int width, int height, int scale) :
	m_instance{ GetModuleHandle(nullptr) },
	m_window{ nullptr }
{
	SCALE = scale;
	WIDTH = width;
	HEIGHT = height;

	imageData = new uint32_t[WIDTH * HEIGHT]{};
}

WinDisplay::~WinDisplay()
{
	DestroyWindow(static_cast<HWND>(m_window));
	UnregisterClass(CLASS_NAME, static_cast<HINSTANCE>(m_instance));
	delete imageData;
}

int WinDisplay::Width() const
{
	return WIDTH;
}

int WinDisplay::Height() const
{
	return HEIGHT;
}

bool WinDisplay::IsOpen()
{
	return !closeFlag;
}

bool WinDisplay::Init()
{
	WNDCLASS wndClass = {};
	wndClass.lpszClassName = CLASS_NAME;
	wndClass.hInstance = static_cast<HINSTANCE>(m_instance);
	wndClass.hIcon = LoadIcon(NULL, IDI_WINLOGO);
	wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndClass.lpfnWndProc = WindowProc;

	RegisterClass(&wndClass);

	DWORD style = WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU;

	RECT rect;
	rect.left = 250;
	rect.top = 250;
	rect.right = rect.left + WIDTH * SCALE;
	rect.bottom = rect.top + HEIGHT * SCALE;

	AdjustWindowRect(&rect, style, false);

	m_window = CreateWindowEx(
		0,
		CLASS_NAME,
		L"STM_INS",
		style,
		rect.left,
		rect.top,
		rect.right - rect.left,
		rect.bottom - rect.top,
		NULL,
		NULL,
		static_cast<HINSTANCE>(m_instance),
		NULL
	);

	ShowWindow(static_cast<HWND>(m_window), SW_SHOW);

	return true;
}

void WinDisplay::Loop()
{
	MSG msg = {};

	while (PeekMessage(&msg, nullptr, 0u, 0u, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
    }
}

#include "BufferScreen.hpp"
#include "WinDisplay.hpp"

bool WinDisplay::Render(IDrawer* drawer)
{
	static BufferScreen screen;
	screen.SetSize(WIDTH, HEIGHT);

	drawer->Draw(screen);

	memcpy(imageData, screen.image, WIDTH * HEIGHT * sizeof(uint32_t));

	InvalidateRect(static_cast<HWND>(m_window), nullptr, FALSE);
	UpdateWindow(static_cast<HWND>(m_window));

	return true;
}

#endif

