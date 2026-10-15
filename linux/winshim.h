/* winshim.h -- Win32 subset used by BUGS.EXE, declared so the decompiled game
 * code compiles on Linux.  Implementations live in shim/winapi_shim.c and
 * friends.  Field layouts match the originals where game code reads them.
 */
#ifndef BBLIT_WINSHIM_H
#define BBLIT_WINSHIM_H

#include "ghidra_types.h"
#include <stddef.h>
#include <wchar.h>
/* the ported game code calls these directly (game_sprintf and friends are
 * renamed CRT-family entry points, not libc ones) */
#include <stdio.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- base scalar types ---- */
typedef int BOOL;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int DWORD;
typedef int INT;
typedef unsigned int UINT;
typedef long LONG;
typedef unsigned long ULONG;
typedef long long LONGLONG;
typedef unsigned long long ULONGLONG;
typedef int HRESULT;
typedef char CHAR, *LPSTR, *LPTSTR;
typedef const char *LPCSTR, *LPCTSTR;
typedef unsigned short WCHAR, OLECHAR;

typedef void *LPVOID;
typedef BYTE *LPBYTE;
typedef WORD *LPWORD;
typedef DWORD *LPDWORD;
typedef int *LPLONG;
typedef const void *LPCVOID;
typedef void *HANDLE;
typedef void *HMODULE;
typedef void *HINSTANCE;
typedef void *HWND;
typedef void *HDC;
typedef void *HGLRC;
typedef void *HICON;
typedef void *HCURSOR;
typedef void *HMENU;
typedef void *HBRUSH;
typedef void *HFONT;
typedef void *HGDIOBJ;
typedef void *HGLOBAL;
typedef void *HRSRC;
typedef void *HKEY;
typedef void *HPALETTE;
typedef void *HBITMAP;
typedef void *HHOOK;
typedef long (*FARPROC)();
typedef long (*PROC)();
typedef size_t SIZE_T;

#define TRUE  1
#define FALSE 0
#ifndef NULL
#define NULL  ((void*)0)
#endif

#define MAX_PATH 260
#define WINAPI
#define APIENTRY
#define CALLBACK

typedef long HRESULT_;
#define S_OK      ((HRESULT)0)
#define S_FALSE   ((HRESULT)1)
#define E_FAIL    ((HRESULT)0x80004005L)
#define E_NOTIMPL ((HRESULT)0x80004001L)
#define E_NOINTERFACE ((HRESULT)0x80004002L)
#define FAILED(hr) (((HRESULT)(hr)) < 0)
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)

/* ---- structures whose fields game code touches by name ---- */
typedef struct _RECT { LONG left, top, right, bottom; } RECT, *LPRECT;
typedef struct _POINT { LONG x, y; } POINT, *LPPOINT;
typedef struct _SIZE { LONG cx, cy; } SIZE;

typedef union _LARGE_INTEGER {
    struct { DWORD LowPart; LONG HighPart; } s;
    LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

typedef struct _SECURITY_ATTRIBUTES {
    DWORD nLength; LPVOID lpSecurityDescriptor; BOOL bInheritHandle;
} SECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;

typedef struct _SYSTEM_INFO {
    DWORD dwOemId; DWORD dwPageSize; LPVOID lpMinimumApplicationAddress;
    LPVOID lpMaximumApplicationAddress; DWORD dwActiveProcessorMask;
    DWORD dwNumberOfProcessors; DWORD dwProcessorType; DWORD dwAllocationGranularity;
    WORD dwProcessorLevel; WORD dwProcessorRevision;
} SYSTEM_INFO, *LPSYSTEM_INFO;

typedef struct _MEMORYSTATUS {
    DWORD dwLength; DWORD dwMemoryLoad;
    DWORD dwTotalPhys, dwAvailPhys, dwTotalPageFile, dwAvailPageFile,
          dwTotalVirtual, dwAvailVirtual;
} MEMORYSTATUS, *LPMEMORYSTATUS;

typedef struct _OSVERSIONINFOA {
    DWORD dwOSVersionInfoSize; DWORD dwMajorVersion, dwMinorVersion,
          dwBuildNumber; DWORD dwPlatformId; CHAR szCSDVersion[128];
} OSVERSIONINFOA, *LPOSVERSIONINFOA;

/* Ghidra keeps the struct tag spelling in local declarations */
typedef struct _MEMORYSTATUS _MEMORYSTATUS;
typedef struct _OSVERSIONINFOA _OSVERSIONINFOA;

/* Ghidra's anonymous union placeholder inside SYSTEM_INFO locals */
typedef union _union_530 {
    struct { DWORD s; }; /* first member overlay; real fields follow via array */
    DWORD dwOemId;
    DWORD dwProcessorType;
    char pad_[96];
} _union_530;

typedef struct _STARTUPINFOA {
    DWORD cb; LPSTR lpReserved, lpDesktop, lpTitle;
    DWORD dwX, dwY, dwXSize, dwYSize, dwXCountChars, dwYCountChars,
          dwFillAttribute, dwFlags; WORD wShowWindow, cbReserved2;
    LPBYTE lpReserved2; HANDLE hStdInput, hStdOutput, hStdError;
} STARTUPINFOA, *LPSTARTUPINFOA;

typedef struct _CPINFO {
    UINT MaxCharSize; BYTE DefaultChar[MAX_PATH]; BYTE LeadByte[MAX_PATH];
} CPINFO, *LPCPINFO;

typedef struct _FILETIME { DWORD dwLowDateTime, dwHighDateTime; } FILETIME;

typedef struct _WIN32_FIND_DATAA { /* only if needed */ DWORD dwFileAttributes; } WIN32_FIND_DATAA;

/* message loop */

typedef unsigned int UINT_PTR;
typedef long LRESULT;
typedef unsigned int WPARAM;
typedef long LPARAM;

typedef struct _MSG {
    HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam;
    DWORD time; POINT pt;
} MSG, *LPMSG;

/* Ghidra names the struct tagMSG in local declarations */
typedef struct _MSG tagMSG;

typedef struct _PAINTSTRUCT {
    HDC hdc; BOOL fErase; RECT rcPaint; BOOL fRestore; BOOL fIncUpdate;
    BYTE rgbReserved[32];
} PAINTSTRUCT, *LPPAINTSTRUCT;

typedef LRESULT (*WNDPROC)(HWND, UINT, WPARAM, LPARAM);

typedef struct _WNDCLASSA {
    UINT style;
    WNDPROC lpfnWndProc;
    int cbClsExtra, cbWndExtra;
    HINSTANCE hInstance;
    HICON hIcon;
    HCURSOR hCursor;
    HBRUSH hbrBackground;
    LPCSTR lpszMenuName;
    LPCSTR lpszClassName;
} WNDCLASSA, *LPWNDCLASSA;


typedef struct _PIXELFORMATDESCRIPTOR {
    WORD nSize, nVersion;
    DWORD dwFlags;
    BYTE iPixelType, cColorBits, cRedBits, cRedShift, cGreenBits, cGreenShift,
         cBlueBits, cBlueShift, cAlphaBits, cAlphaShift, cAccumBits, cAccumRedBits,
         cAccumGreenBits, cAccumBlueBits, cAccumAlphaBits, cDepthBits, cStencilBits,
         cAuxBuffers, iLayerType, bReserved, dwLayerMask, dwVisibleMask, dwDamageMask;
} PIXELFORMATDESCRIPTOR, *LPPIXELFORMATDESCRIPTOR;

typedef struct tagLOGPALETTE {
    WORD palVersion, palNumEntries;
} LOGPALETTE, *LPLOGPALETTE;

typedef struct _PALETTEENTRY { BYTE peRed, peGreen, peBlue, peFlags; } PALETTEENTRY;
typedef DWORD COLORREF;

/* OpenGL typedefs the game uses via dynamic loading */
typedef int (WINAPI *PFNWGLCHOOSEPIXELFORMAT_T)(HDC, const PIXELFORMATDESCRIPTOR *);
typedef BOOL (WINAPI *PFNWGL_T)(void);

/* COM / OLE names the decompiler uses verbatim */
typedef struct _GUID { unsigned long Data1; unsigned short Data2, Data3; unsigned char Data4[8]; } IID, CLSID, *LPIID;
typedef void *LPUNKNOWN;
typedef void *LPCLASSUNKNOWN;
typedef DWORD LCID;
typedef BOOL *LPBOOL;
typedef wchar_t *LPWSTR;
typedef const wchar_t *LPCWSTR;
typedef char *LPCH;
typedef unsigned short *LPWCH;
typedef long *PLONG;
typedef unsigned char *PBYTE;
typedef HKEY *PHKEY;
typedef void *LPOVERLAPPED;
typedef struct _MSG *LPMSG;
typedef unsigned long ULONG_PTR, DWORD_PTR;

/* registry */
typedef DWORD REGSAM;
#define HKEY_CURRENT_USER ((HKEY)0x80000001)
#define KEY_READ   0x20019
#define KEY_WRITE  0x20006
#define REG_SZ     1
#define REG_DWORD  4

/* CreateFileA constants */
#define GENERIC_READ  0x80000000L
#define GENERIC_WRITE 0x40000000L
#define CREATE_NEW        1
#define CREATE_ALWAYS     2
#define OPEN_EXISTING     3
#define OPEN_ALWAYS       4
#define TRUNCATE_EXISTING 5
#define FILE_SHARE_READ   1
#define FILE_SHARE_WRITE  2
#define FILE_BEGIN   0
#define FILE_CURRENT 1
#define FILE_END     2
#define INVALID_HANDLE_VALUE ((HANDLE)(long)-1)
#define INVALID_SET_FILE_POINTER 0xFFFFFFFF
#define FILE_ATTRIBUTE_NORMAL 0x80
#define NO_ERROR 0L

/* console */
typedef struct _CHAR_INFO { /* unused placeholder */ WORD a; } CHAR_INFO;
typedef union _INPUT_RECORD_u { BYTE EventTypePad[16]; } INPUT_RECORD_pad;
typedef struct _KEY_EVENT_RECORD {
    BOOL bKeyDown; WORD wRepeatCount; WORD wVirtualKeyCode; WORD wVirtualScanCode;
    union { WCHAR UnicodeChar; CHAR AsciiChar; } uChar;
} KEY_EVENT_RECORD;
typedef struct _INPUT_RECORD {
    WORD EventType;
    union { KEY_EVENT_RECORD KeyEvent; BYTE Event[16]; } Event;
} INPUT_RECORD, *PINPUT_RECORD;
#define KEY_EVENT 0x0001

/* GetSystemMetrics indices the game uses */
#define SM_CXSCREEN 0
#define SM_CYSCREEN 1
#define SM_CXDLGFRAME 7
#define SM_CYDLGFRAME 8
#define SM_CYCAPTION 4

/* ShowWindow / window messages used */
#define SW_SHOWNORMAL 1
#define SW_SHOWDEFAULT 10
#define WM_QUIT 0x0012
#define WM_PAINT 0x000F
#define WM_CLOSE 0x0010
#define WM_DESTROY 0x0002
#define WM_ACTIVATEAPP 0x001C
#define WM_SETCURSOR 0x0020
#define WM_KEYDOWN 0x0100
#define WM_KEYUP  0x0101
#define WM_SYSKEYDOWN 0x0104
#define WM_SYSKEYUP 0x0105
#define WM_CHAR 0x0102
#define PM_NOREMOVE 0
#define PM_REMOVE 1
#define MB_OK 0x0
#define MB_ICONERROR 0x10
#define MB_ICONINFORMATION 0x40
#define MB_RETRYCANCEL 0x5
#define MB_DEFBUTTON1 0x0
#define IDRETRY 4
#define IDCANCEL 2
#define IDYES 6
#define IDNO 7
#define MB_SYSTEMMODAL 0x1000
#define SWP_NOZORDER 0x4
#define SWP_NOMOVE 0x2
#define SWP_NOSIZE 0x1
#define CW_USEDEFAULT ((int)0x80000000)

/* GetDriveTypeA */
#define DRIVE_CDROM 5

/* GetSystemDefaultLangID etc */
typedef WORD LANGID;

/* ---- function declarations (imports) ---- */
/* kernel32 */
DWORD GetOEMCP(void);
DWORD GetACP(void);
void ExitProcess(UINT uExitCode);
BOOL WriteConsoleA(HANDLE, const void *, DWORD, LPDWORD, LPVOID);
UINT SetErrorMode(UINT);
UINT GetDriveTypeA(LPCSTR);
BOOL GetVolumeInformationA(LPCSTR, LPSTR, DWORD, LPDWORD, LPDWORD, LPDWORD, LPSTR, DWORD);
BOOL QueryPerformanceCounter(LARGE_INTEGER *);
DWORD GetTickCount(void);
BOOL QueryPerformanceFrequency(LARGE_INTEGER *);
void Sleep(DWORD);
LPSTR GetCommandLineA(void);
LANGID GetSystemDefaultLangID(void);
BOOL FreeLibrary(HMODULE);
/* LoadLibraryA returns the full 64-bit dlopen handle: the game parks it in a
 * dword slot but feeds it straight back into GetProcAddress, which walks it
 * as a pointer.  A truncated handle segfaults inside ld.so. */
HMODULE LoadLibraryA(LPCSTR);
DWORD GetLastError(void);
FARPROC GetProcAddress(HMODULE, LPCSTR);
HANDLE GetCurrentThread(void);
int GetThreadPriority(HANDLE);
BOOL SetThreadPriority(HANDLE, int);
BOOL CloseHandle(HANDLE);
HANDLE CreateFileA(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
BOOL FlushFileBuffers(HANDLE);
LPVOID FreeEnvironmentStringsA(LPVOID);
LPVOID FreeEnvironmentStringsW(LPVOID);
BOOL GetConsoleMode(HANDLE, LPDWORD);
BOOL GetCPInfo(UINT CodePage, CPINFO *lpCPInfo);
DWORD GetCurrentDirectoryA(DWORD, LPSTR);
HANDLE GetCurrentProcess(void);
LPVOID GetEnvironmentStrings(void);
LPVOID GetEnvironmentStringsW(void);
DWORD GetFileType(HANDLE);
DWORD GetModuleFileNameA(HMODULE, LPSTR, DWORD);
HMODULE GetModuleHandleA(LPCSTR);
UINT GetCPInfo_A_placeholder(void);
void GetStartupInfoA(STARTUPINFOA *);
HANDLE GetStdHandle(int);
BOOL GetStringTypeA(LCID, DWORD, LPCSTR, int, LPWORD);
void GetSystemInfo(SYSTEM_INFO *);
void GlobalMemoryStatus(MEMORYSTATUS *);
/* GetVersion (kernel32); the CRT startup splits this word into the
 * DAT_004b1e20..2c version bytes.  Report Win95 4.0 to match GetVersionExA. */
DWORD GetVersion(void);
LPVOID HeapAlloc(HANDLE, DWORD, SIZE_T);
HANDLE HeapCreate(DWORD, SIZE_T, SIZE_T);
BOOL HeapDestroy(HANDLE);
BOOL HeapFree(HANDLE, DWORD, LPVOID);
int LCMapStringA(DWORD lcid, DWORD dwMapFlags, LPCSTR lpSrcStr, int cchSrc, LPSTR lpDestStr, int cchDest);
int MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr, int cbMultiByte, wchar_t *lpWideCharStr, int cchWideChar);
void RaiseException(DWORD, DWORD, DWORD, const ULONG *);
BOOL ReadConsoleInputA(HANDLE, PINPUT_RECORD, DWORD, LPDWORD);
BOOL ReadFile(HANDLE, LPVOID, DWORD, LPDWORD, void *lpOverlapped);
BOOL SetConsoleMode(HANDLE, DWORD);
BOOL SetCurrentDirectoryA(LPCSTR);
BOOL SetEndOfFile(HANDLE);
BOOL SetEnvironmentVariableA(LPCSTR, LPCSTR);
DWORD SetFilePointer(HANDLE, LONG, LPLONG, DWORD);
void SetHandleCount(UINT);
BOOL SetStdHandle(int, HANDLE);
BOOL TerminateProcess(HANDLE, UINT);
LONG UnhandledExceptionFilter(void *ExceptionInfo);
LPVOID VirtualAlloc(LPVOID, SIZE_T, DWORD, DWORD);
BOOL VirtualFree(LPVOID, SIZE_T, DWORD);
int WideCharToMultiByte(UINT CodePage, DWORD dwFlags, const wchar_t *lpWideCharStr, int cchWideChar, LPSTR lpMultiByteStr, int cbMultiByte, LPCSTR lpDefaultChar, BOOL *lpUsedDefaultChar);
BOOL WriteFile(HANDLE, LPCVOID, DWORD, LPDWORD, void *lpOverlapped);

/* user32 */
BOOL BeginPaint(HWND, LPPAINTSTRUCT);
BOOL BringWindowToTop(HWND);
HWND CreateWindowExA(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
LRESULT DefWindowProcA(HWND, UINT, WPARAM, LPARAM);
BOOL DestroyWindow(HWND);
LRESULT DispatchMessageA(const MSG *);
BOOL EndPaint(HWND, const LPPAINTSTRUCT);
HWND FindWindowA(LPCSTR, LPCSTR);
BOOL GetClientRect(HWND, LPRECT);
HDC GetDC(HWND);
HWND GetForegroundWindow(void);
short GetKeyboardState(LPBYTE);
int GetSystemMetrics(int);
BOOL InvalidateRect(HWND, const RECT *, BOOL);
BOOL IsIconic(HWND);
HCURSOR LoadCursorA(HINSTANCE, LPCSTR);
HICON LoadIconA(HINSTANCE, LPCSTR);
int LoadStringA(HINSTANCE, UINT, LPSTR, int);
int MessageBoxA(HWND, LPCSTR, LPCSTR, UINT);
BOOL PeekMessageA(MSG *, HWND, UINT, UINT, UINT);
void PostQuitMessage(int);
typedef unsigned short ATOM;
ATOM RegisterClassA(const WNDCLASSA *);
int ReleaseDC(HWND, HDC);
HCURSOR SetCursor(HCURSOR);
HWND SetFocus(HWND);
BOOL SetWindowPos(HWND, HWND, int, int, int, int, UINT);
BOOL ShowWindow(HWND, int);
BOOL TranslateMessage(const MSG *);
BOOL UpdateWindow(HWND);

/* gdi32 */
int ChoosePixelFormat(HDC, const PIXELFORMATDESCRIPTOR *);
HPALETTE CreatePalette(const LOGPALETTE *);
BOOL DeleteObject(HGDIOBJ);
UINT RealizePalette(HDC);
HPALETTE SelectPalette(HDC, HPALETTE, BOOL);
int SetPixelFormat(HDC, int, const PIXELFORMATDESCRIPTOR *);
int StretchDIBits(HDC, int, int, int, int, int, int, int, const void *,
                  const void *, UINT, DWORD, DWORD, DWORD, UINT);
BOOL SwapBuffers(HDC);

/* advapi32 */
LONG RegCloseKey(HKEY);
LONG RegOpenKeyExA(HKEY, LPCSTR, DWORD, REGSAM, HKEY *);
LONG RegQueryValueExA(HKEY, LPCSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);

/* ole32 */
HRESULT CoCreateInstance(const void *, void *, DWORD, const void *, void **);
HRESULT CoInitialize(LPVOID);
void CoUninitialize(void);

/* ddraw / dsound entry points (COM creation) */
HRESULT DirectDrawCreate(void *lpGuid, void **lplpDD, void *pUnkOuter);
HRESULT DirectDrawEnumerateA(void *lpCallback, LPVOID lpContext);
HRESULT DirectSoundCreate(void *lpGuid, void **ppDS, void *pUnkOuter);

/* ---- Linux GL backend (shim/gl_wgl.c) ----
 * wgl* wrappers backed by EGL surfaceless + Mesa; the game resolves them
 * through GetProcAddress like on Windows. */
FARPROC wgl_wrapper(const char *name);
void *ddraw_fake_object(void);
int gl_probe_frame(unsigned *checksum);
void gl_table_sync(void);
void cb1a_init(void);
void *gl_trampoline(void *target);
void bblit_fd_register(int fd, FILE *f);

/* ---- COM interface shims ----
 * The game drives DirectDraw/DirectSound through raw vtable offsets
 * (e.g. (**(code**)*obj + 0x0c)(obj, ...)), so the shim exposes real
 * pointer-vtables with matching layout. */
typedef struct ShimUnknown ShimUnknown;
typedef struct { HRESULT (*QueryInterface)(ShimUnknown *, const void *, void **);
                 ULONG (*AddRef)(ShimUnknown *);
                 ULONG (*Release)(ShimUnknown *); } ShimVTableUnknown;
struct ShimUnknown { const ShimVTableUnknown *vtable; };

#ifdef __cplusplus
}
#endif

#endif /* BBLIT_WINSHIM_H */

/* ---- port helpers implemented in shim/port_helpers.c ---- */
unsigned long flt_bitsf(float v);
unsigned long flt_bitsd(double v);
float u32_as_f(unsigned long bits);
double u32_as_d(unsigned long bits);
long double u32_as_ld(unsigned long bits);
long CARRY4(unsigned long a, unsigned long b);
unsigned long long rdtsc(void);
unsigned char in(unsigned short port);
void out(unsigned short port, unsigned char val);
long double f2xm1(long double x);
long double fpatan(long double y, long double x);
long double fscale(long double v, long double s);
long double fcos(long double x);
int *cpuid_basic_info(int leaf);
int *cpuid_Version_info(int leaf);
int32_t vc6_ftol(double v);
void data_wr3(void *base, long off, unsigned long v);
unsigned long data_rd3(void *base, long off);
void *lab_addr(const char *name);
extern void *reg_scratch;

/* VC++6 CRT support (shim/shim_crt.c); unprototyped where the original
 * call sites vary. */
longlong __allmul(longlong a, longlong b);
longlong __alldiv(longlong a, longlong b);
longlong __allrem(longlong a, longlong b);
ulonglong __aulldiv(ulonglong a, ulonglong b);
ulonglong __aullrem(ulonglong a, ulonglong b);
longlong __allshl(longlong a, int shift);
longlong __allshr(longlong a, int shift);
ulonglong __aullshr(ulonglong a, int shift);
char *_strstr(const char *h, const char *n);
char *_strrchr(const char *s, int c);
char *_strncpy(char *d, const char *s, size_t n);
int __strcmpi(const char *a, const char *b);
int _stricmp(const char *a, const char *b);
void __amsg_exit();
long double __math_exit(void);
long double __startOneArgErrorHandling(void);
long __fload_withFB();

/* ---- WinAPI the CRT-adjacent code calls (stubs in shim/winapi_stubs.c) ---- */
typedef DWORD LCID;
BOOL GetVersionExA(LPOSVERSIONINFOA);
int LCMapStringW(LCID lcid, DWORD flags, LPCWSTR src, int srclen, LPWSTR dst, int dstlen);
BOOL GetStringTypeW(DWORD type, LPCWSTR src, int srclen, LPWORD chartype);

/* Ghidra renders tag-only struct spellings as bare tag names */
typedef struct _SECURITY_ATTRIBUTES _SECURITY_ATTRIBUTES;

/* GetLogicalDrive/GetVersion-adjacent CRT struct Ghidra kept as _cpinfo */
typedef struct _cpinfo {
    UINT numc;
} _cpinfo;

/* Ghidra "exref" names reference the IAT slot of the real function */
#define GetLastError_exref ((FARPROC)GetLastError)

/* Ghidra half-part views of a dword slot (CONCAT22 inputs) */
#define DAT_009ca740_2 (*(uint16_t *)((unsigned char *)&DAT_009ca740 + 1))
/* raw byte views Ghidra names ram0xADDR */
#define ram0x004b31c1 (*(unsigned char *)0x004b31c1ul)
