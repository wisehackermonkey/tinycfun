// #include <stdio.h>
// #include <windows.h>
// // #include <msctf.h>
// // #include <Rpcsal.h>
// #include "globals.h"
// #include "case.h"
// #include "editsess.h"


//   int main() {
//     printf("tinycfun Works!\n");
//     printf("%i", WINVER);
//     ITfRange::SetText();
//     return 0;
//   }


//   // tcc -run ./src/main.c
//   //tcc -run ./src/main.c -o tinycfun
// tcc -run ./main.c -o tinycfun
//   //tcc -o tinycfun ./src/main.c 
//   //<ntddk.h> <wdf.h> windows drver kit
// // i386-win32-tcc  -run ./src/main.c
// // tcc -impdef msctf.def  -run ./src/main.c
// //tcc -impdef msctf.dll -o msctf.def
// //tcc ./main.c -lmsctf -lole32 -o funcc.exe


//   // keyboard processors, look at text service framework,
//   //    look at TSF text service framework for keyboards
//   // yeah ithink im on the right track this shows example of spell checking
//   // idea use keys that are supported on every opperating system but dont have a physical repsentation
//   // so my app can pluginto that system.
//   // this is very windows centric cry 



//   /// making s text service example
//   //i386-win32-tcc  -shared .\tsf_hello.c .\tsf_hello.def -o tsf_hello.dll -lole32 -luser32 -ladvapi32 regsvr32 tsf_hello.dll 
//tcc  -shared .\tsf_hello.c .\tsf_hello.def -o tsf_hello.dll -lole32 -luser32 -ladvapi32 regsvr32 tsf_hello.dll 
//i386-win32-tcc -shared .\tsf_hello.c .\tsf_hello.def -o tsf_hello.dll -lole32 -luser32 -ladvapi32

//+---------------------------------------------------------------------------
//
//  HELLO_WIN.C - Windows GUI 'Hello World!' Example
//
//+---------------------------------------------------------------------------

#include <windows.h>

#define APPNAME "HELLO_WIN"

char szAppName[] = APPNAME; // The name of this application
char szTitle[]   = APPNAME; // The title bar text
const char *pWindowText;

void CenterWindow(HWND hWnd);

//+---------------------------------------------------------------------------
//
//  Function:   WndProc
//
//  Synopsis:   very unusual type of function - gets called by system to
//              process windows messages.
//
//  Arguments:  same as always.
//----------------------------------------------------------------------------

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {

        // ----------------------- first and last
        case WM_CREATE:
            CenterWindow(hwnd);
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        // ----------------------- get out of it...
        case WM_RBUTTONUP:
            DestroyWindow(hwnd);
            break;

        case WM_KEYDOWN:
            if (VK_ESCAPE == wParam)
                DestroyWindow(hwnd);
            break;

        // ----------------------- display our minimal info
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC         hdc;
            RECT        rc;
            hdc = BeginPaint(hwnd, &ps);

            GetClientRect(hwnd, &rc);
            SetTextColor(hdc, RGB(240,240,96));
            SetBkMode(hdc, TRANSPARENT);
            DrawText(hdc, pWindowText, -1, &rc, DT_CENTER|DT_SINGLELINE|DT_VCENTER);

            EndPaint(hwnd, &ps);
            break;
        }

        // ----------------------- let windows do all other stuff
        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}

//+---------------------------------------------------------------------------
//
//  Function:   WinMain
//
//  Synopsis:   standard entrypoint for GUI Win32 apps
//
//----------------------------------------------------------------------------
int APIENTRY WinMain(
        HINSTANCE hInstance,
        HINSTANCE hPrevInstance,
        LPSTR lpCmdLine,
        int nCmdShow
        )
{
    MSG msg;
    WNDCLASS wc;
    HWND hwnd;

    pWindowText = lpCmdLine[0] ? lpCmdLine : "Hello Windows!";

    // Fill in window class structure with parameters that describe
    // the main window.

    ZeroMemory(&wc, sizeof wc);
    wc.hInstance     = hInstance;
    wc.lpszClassName = szAppName;
    wc.lpfnWndProc   = (WNDPROC)WndProc;
    wc.style         = CS_DBLCLKS|CS_VREDRAW|CS_HREDRAW;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);

    if (FALSE == RegisterClass(&wc))
        return 0;

    // create the browser
    hwnd = CreateWindow(
        szAppName,
        szTitle,
        WS_OVERLAPPEDWINDOW|WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        360,//CW_USEDEFAULT,
        240,//CW_USEDEFAULT,
        0,
        0,
        hInstance,
        0);

    if (NULL == hwnd)
        return 0;

    // Main message loop:
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return msg.wParam;
}

//+---------------------------------------------------------------------------

//+---------------------------------------------------------------------------

void CenterWindow(HWND hwnd_self)
{
    HWND hwnd_parent;
    RECT rw_self, rc_parent, rw_parent;
    int xpos, ypos;

    hwnd_parent = GetParent(hwnd_self);
    if (NULL == hwnd_parent)
        hwnd_parent = GetDesktopWindow();

    GetWindowRect(hwnd_parent, &rw_parent);
    GetClientRect(hwnd_parent, &rc_parent);
    GetWindowRect(hwnd_self, &rw_self);

    xpos = rw_parent.left + (rc_parent.right + rw_self.left - rw_self.right) / 2;
    ypos = rw_parent.top + (rc_parent.bottom + rw_self.top - rw_self.bottom) / 2;

    SetWindowPos(
        hwnd_self, NULL,
        xpos, ypos, 0, 0,
        SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE
        );
}

//+---------------------------------------------------------------------------
