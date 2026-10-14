/* winapi_table.c -- Windows API surface for the port, stage 1.
 *
 * Each name below is referenced by the ported game code.  Stage 1 provides
 * signatures and honest no-op / failure defaults so the link completes and
 * the CRT paths run; behaviour-critical APIs (registry, files, DirectDraw)
 * get real implementations per subsystem.  Every stub that deviates from
 * Windows semantics carries a STUB-DIVERGENCE note.
 */
#include "../winshim.h"

#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/mman.h>
#ifndef MAP_32BIT
#define MAP_32BIT 0x40
#endif
#include <ctype.h>

/* ---- process / CRT ---- */
void __amsg_exit(int term) { (void)term; exit(255); }
long __fload_withFB(void) { return 0; }        /* STUB-DIVERGENCE */
void ExitProcess(unsigned code) { exit((int)code); }
void RaiseException(unsigned a, unsigned b, unsigned c, const uintptr_t *d)
{ (void)a; (void)b; (void)c; (void)d; }                /* STUB-DIVERGENCE */
BOOL TerminateProcess(HANDLE h, UINT code) { (void)h; exit((int)code); }
void Sleep(unsigned ms) { struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L }; nanosleep(&ts, 0); }

HANDLE GetCurrentProcess(void) { return (HANDLE)0xffffffffffffffffull; }
HANDLE GetCurrentThread(void) { return (HANDLE)0xfffffffffffffffeull; }
void GetSystemInfo(SYSTEM_INFO *si) { memset(si, 0, 36); si->dwNumberOfProcessors = 1; si->dwPageSize = 4096; }

/* ---- environment / module ---- */
uint32_t GetLastError(void) { return 0; }      /* STUB-DIVERGENCE */
void SetLastError(uint32_t e) { (void)e; }
char * GetCommandLineA(void) { return (char *)""; }
LPVOID GetEnvironmentStrings(void) { return (LPVOID)"\0"; }
LPVOID FreeEnvironmentStringsA(LPVOID p) { (void)p; return 0; }
LPVOID GetEnvironmentStringsW(void) { return (LPVOID)L"\0"; }
LPVOID FreeEnvironmentStringsW(LPVOID p) { (void)p; return 0; }
uint32_t GetACP(void) { return 1252; }
uint32_t GetOEMCP(void) { return 850; }
BOOL GetCPInfo(UINT cp, CPINFO *ci) { (void)cp; memset(ci, 0, sizeof(CPINFO)); ci->MaxCharSize = 1; return 1; }
BOOL GetStringTypeA(DWORD l, DWORD t, LPCSTR s, int c, LPWORD o)
{ (void)l; (void)t; (void)c; (void)s; (void)o; return 0; }  /* STUB-DIVERGENCE */
INT LCMapStringA(DWORD l, DWORD f, LPCSTR s, int n, LPSTR d, int dn)
{ (void)l; if (!(f & 0x400) || !s || !d) return 0; if (n < 0) n = (int)strlen(s) + 1; if (dn < n) return 0; for (int i = 0; i < n; i++) d[i] = (char)((f & 0x100) ? tolower((unsigned char)s[i]) : s[i]); return (INT)n; }
LANGID GetSystemDefaultLangID(void) { return 0x0409; }
DWORD GetModuleFileNameA(HMODULE h, LPSTR out, DWORD n)
{ (void)h; ssize_t r = readlink("/proc/self/exe", out, n - 1); return r > 0 ? (DWORD)strlen(out) : 0; }
HMODULE GetModuleHandleA(LPCSTR name) { (void)name; return (HMODULE)0x10000; }
void GetStartupInfoA(LPSTARTUPINFOA si) { memset(si, 0, sizeof(*si)); si->cb = sizeof(*si); }

/* ---- console / stdio ---- */
static FILE * stdh(int fd) { switch (fd) { case 0: return stdin; case 1: return stdout; default: return stderr; } }
BOOL SetStdHandle(int which, HANDLE h) { (void)which; (void)h; return 1; }
HANDLE GetStdHandle(int which) { return (HANDLE)stdh(which == -10 ? 0 : which == -11 ? 1 : 2); }
DWORD GetFileType(HANDLE h)
{
  if (!h) return 0;
  if (h == (HANDLE)0 || h == (HANDLE)1 || h == (HANDLE)2) return 2;  /* FILE_TYPE_CHAR */
  struct stat st;
  return fstat(fileno((FILE *)h), &st) == 0 && S_ISCHR(st.st_mode) ? 2 : 3; /* CHAR/DISK */
}
BOOL GetConsoleMode(HANDLE h, LPDWORD m) { (void)h; *m = 3; return 1; }
BOOL SetConsoleMode(HANDLE h, DWORD m) { (void)h; (void)m; return 1; }
BOOL WriteConsoleA(HANDLE h, LPCVOID b, DWORD n, LPDWORD w, void *u)
{ (void)h; (void)u; size_t r = fwrite(b, 1, n, stdout); if (w) *w = (DWORD)r; return 1; }
BOOL ReadConsoleInputA(HANDLE h, PINPUT_RECORD rec, DWORD n, LPDWORD read)
{ (void)h; (void)rec; (void)n; *read = 0; return 1; }  /* STUB-DIVERGENCE */
void SetHandleCount(UINT n) { (void)n; }
uint32_t GetHandleCount(void) { return 0; }

/* ---- files ---- */
/* File opens come through the CRT _sopen wrapper in the image; disp is one
 * of the CREATE or OPEN constants. Map to fopen modes. */
HANDLE CreateFileA(LPCSTR name, DWORD access, DWORD share,
                              LPSECURITY_ATTRIBUTES sa, DWORD disp, DWORD flags, HANDLE tmpl)
{ (void)access; (void)share; (void)sa; (void)flags; (void)tmpl;
  if (!name || (uintptr_t)name < 0x10000 || (uintptr_t)name >= 0x7ffffffdfffcUL
      || ((uintptr_t)name >> 47) != ((uintptr_t)name >> 63))
    return INVALID_HANDLE_VALUE;
  fprintf(stderr, "[cf] %s disp=%u\n", name, (unsigned)disp);
  const char *mode;
  switch (disp) {
  case CREATE_NEW:      mode = "wb";  break;   /* fail if exists: close enough for the game's use */
  case CREATE_ALWAYS:   mode = "wb+"; break;
  case OPEN_EXISTING:   mode = "rb";  break;
  case OPEN_ALWAYS:     mode = "ab+"; break;   /* create if missing */
  case TRUNCATE_EXISTING: mode = "wb"; break;
  default:              mode = "rb";  break;
  }
  FILE *f = fopen(name, mode);
  return f ? (HANDLE)f : INVALID_HANDLE_VALUE; }
BOOL ReadFile(HANDLE h, LPVOID buf, DWORD n, LPDWORD read, void *over)
{ (void)over; if (!h || h == INVALID_HANDLE_VALUE) return 0;
  size_t r = fread(buf, 1, n, (FILE *)h); if (read) *read = (DWORD)r; return r == n; }
BOOL WriteFile(HANDLE h, LPCVOID buf, DWORD n, LPDWORD w, void *over)
{ (void)over; if (!h || h == INVALID_HANDLE_VALUE) return 0;
  size_t r = fwrite(buf, 1, n, (FILE *)h); if (w) *w = (DWORD)r; return r == n; }
BOOL SetEndOfFile(HANDLE h) { return h ? (fflush((FILE *)h), 1) : 0; }
DWORD SetFilePointer(HANDLE h, LONG dist, LPLONG hi, DWORD how)
{ (void)hi; if (!h) return (DWORD)-1; int w = how == 0 ? SEEK_SET : how == 1 ? SEEK_CUR : SEEK_END;
  long r = fseek((FILE *)h, dist, w); return r ? (DWORD)-1 : (DWORD)ftell((FILE *)h); }
BOOL CloseHandle(HANDLE h)
{ if (!h || h == INVALID_HANDLE_VALUE) return 0;
  if (h == (HANDLE)0xffffffffffffffffull || h == (HANDLE)0xfffffffffffffffeull) return 1;
  fclose((FILE *)h); return 1; }
DWORD GetCurrentDirectoryA(DWORD n, LPSTR buf) { return getcwd(buf, n) ? (DWORD)strlen(buf) : 0; }
BOOL SetCurrentDirectoryA(LPCSTR p) { return chdir(p) == 0; }
BOOL SetEnvironmentVariableA(LPCSTR k, LPCSTR v) { return setenv(k, v ? v : "", 1) == 0; }

/* ---- memory ---- */
static struct { void *base; size_t len; } g_heaps[8];
HANDLE HeapCreate(DWORD flags, SIZE_T init, SIZE_T max)
{ (void)flags; (void)init; for (unsigned i = 0; i < 8; i++)
    if (!g_heaps[i].base) { g_heaps[i].base = (void *)(uintptr_t)(0x1000 + i); return g_heaps[i].base ? g_heaps[i].base : 0; }
  return 0; }
BOOL HeapDestroy(HANDLE h) { (void)h; return 1; }
/* GetVersion (kernel32): the CRT startup reads the Windows version word.
 * Report Win95 (4.0, platform 1) to match the GetVersionExA stub. */
DWORD GetVersion(void) { return 0x0A040001u; }
/* VirtualAlloc reserve registry: fixed commits are only honoured inside a
 * live reserve (see VirtualAlloc). */
static struct { void *base; size_t len; } bblit_reserves[16];
static int bblit_in_reserve(void *addr, size_t len)
{
    for (unsigned i = 0; i < sizeof bblit_reserves / sizeof bblit_reserves[0]; i++) {
        uintptr_t b = (uintptr_t)bblit_reserves[i].base, l = bblit_reserves[i].len;
        if (b && (uintptr_t)addr >= b && (uintptr_t)addr + len <= b + l) return 1;
    }
    return 0;
}
LPVOID HeapAlloc(HANDLE h, DWORD flags, SIZE_T n)
{ (void)h; void *p = malloc(n); if (p && (flags & 8)) memset(p, 0, n); return p; }
BOOL HeapFree(HANDLE h, DWORD flags, LPVOID p)
{ (void)h; (void)flags; if (!p) return 0;
  /* arena metadata blocks live in VirtualAlloc reserves, not the heap:
   * freeing those with free() would abort, so let them leak instead. */
  if (bblit_in_reserve(p, 1)) return 1;
  free(p); return 1; }
void * VirtualAlloc(void *addr, size_t n, uint32_t type, uint32_t prot)
{
  /* MEM_COMMIT|MEM_RESERVE (0x2000|0x1000) with a NULL address backs the
   * VC6 sub-allocator arenas: 4 MiB + 64 KiB per arena here, so mmap the
   * zeroed pages for real instead of calloc (the 64 KiB commit lands at a
   * different address than the arena base and the old calloc could not
   * provide that).  MAP_32BIT keeps arenas below 2 GiB so the allocator's
   * 32-bit index math ((int)/(uint) casts in FUN_00452380/4523e0/452680)
   * stays exact, exactly as on x86.  A fixed commit is only honoured
   * inside a live reserve; anything else fails like Windows (committing
   * unreserved pages is an error there).  That matters because the growth
   * path in FUN_00452440 occasionally derives a scratch-table address that
   * must fail so the caller falls back to HeapAlloc (a blind MAP_FIXED
   * would clobber live mappings instead). */
  (void)type; (void)prot;
  size_t pagesz = (size_t)sysconf(_SC_PAGESIZE);
  size_t len = (n + pagesz - 1) & ~(pagesz - 1);
  if (addr) {
    if (!bblit_in_reserve(addr, len)) return 0;
    void *p = mmap(addr, len, PROT_READ | PROT_WRITE,
                   MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (p == MAP_FAILED) ? 0 : p;
  }
  void *p = mmap(0, len, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
  if (p == MAP_FAILED)
    p = mmap(0, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (p == MAP_FAILED) return 0;
  for (unsigned i = 0; i < sizeof bblit_reserves / sizeof bblit_reserves[0]; i++)
    if (!bblit_reserves[i].base) { bblit_reserves[i].base = p; bblit_reserves[i].len = len; break; }
  return p;
}
BOOL VirtualFree(LPVOID addr, SIZE_T n, DWORD type)
{
  /* MEM_RELEASE (0x8000): drop the pages when the size is known, and
   * always drop the reserve record so later commits there fail closed. */
  size_t pagesz = (size_t)sysconf(_SC_PAGESIZE);
  if (!addr) return 0;
  for (unsigned i = 0; i < sizeof bblit_reserves / sizeof bblit_reserves[0]; i++)
    if (bblit_reserves[i].base == addr) bblit_reserves[i].base = 0;
  if (type == 0x8000 && n) {
    size_t len = (n + pagesz - 1) & ~(pagesz - 1);
    return munmap(addr, len) == 0;
  }
  return 1;
}
void GlobalMemoryStatus(LPMEMORYSTATUS ms) { memset(ms, 0, sizeof(MEMORYSTATUS)); ms->dwLength = sizeof(MEMORYSTATUS); ms->dwTotalPhys = 256u << 20; ms->dwAvailPhys = 128u << 20; }

/* ---- registry (installation_path lookup is the game's startup gate) ---- */
/* Install path comes from $BBLIT_INSTALL so the game can be pointed at a
 * directory holding DATAS/ without touching the shim; default is cwd. */
const char *bblit_install_path(void);
const char *bblit_install_path(void)
{ const char *e = getenv("BBLIT_INSTALL"); return (e && *e) ? e : "."; }
static const char *g_install_path_marker = 0;
#define g_install_path bblit_install_path()
LONG RegOpenKeyExA(HKEY h, LPCSTR sub, DWORD res, REGSAM access, PHKEY out)
{ (void)h; (void)res; (void)access;
  if (sub && strcmp(sub, "Software\\Infogrames\\Bugs Bunny Lost In Time") == 0) { *out = (HKEY)0x20000; return 0u; }
  return 2u; }                  /* STUB-DIVERGENCE: static key */
LONG RegQueryValueExA(HKEY k, LPCSTR name, LPDWORD res, LPDWORD type,
                                  LPBYTE buf, LPDWORD len)
{ (void)k; (void)res; (void)type;
  if (!buf || !len) return 1;
  /* winmain passes the destination as an outgoing frame slot whose high half
   * the 4-byte slot model never wrote (observed: buf = 4 GiB-short pointer
   * faulting the memcpy).  A wild pointer is dropped, not written through:
   * the caller keeps scanning. */
  int ok = (uintptr_t)buf > 0x10000 && (uintptr_t)buf < 0x7ffffffdfffcUL
           && ((uintptr_t)buf >> 47) == ((uintptr_t)buf >> 63);
  if (!ok) return 1;
  size_t n = strlen(g_install_path) + 1;
  if (*len < n) { *len = (DWORD)n; return 234; }
  memcpy(buf, g_install_path, n); *len = (DWORD)n; return 0u; }
LONG RegCloseKey(HKEY k) { (void)k; return 0u; }

/* ---- drives / volumes ---- */
/* Disc gate: the scan walks 'C'..'Z' looking for DRIVE_CDROM (5) + volume
 * label BBLIT.  We fake exactly one CD-ROM at the drive letter given by
 * $BBLIT_CD (default 'Q') whose volume label reads back "BBLIT", so the
 * scan finds it and stores DAT_004b1928 = that letter.  File wrappers then
 * fall back to <install>/<path> for CD paths. */
char bblit_cd_drive(void);
char bblit_cd_drive(void)
{ const char *e = getenv("BBLIT_CD"); return (e && *e) ? e[0] : 'Q'; }

UINT GetDriveTypeA(LPCSTR root)
{
  if (root && (root[0] & ~0x20) == (bblit_cd_drive() & ~0x20) && root[1] == ':')
    return 5;                                  /* DRIVE_CDROM */
  return 3;                                    /* DRIVE_FIXED */
}
BOOL GetVolumeInformationA(LPCSTR root, LPSTR vol, DWORD vlen,
                                       LPDWORD serial, LPDWORD maxlen, LPDWORD flags, LPSTR fs, DWORD flen)
{ (void)serial; (void)maxlen; (void)flags;
  /* frame-slot pointer args: the caller's 4-byte frame slots widened to 8
   * leave the high half uninitialised (observed: vol=0x7ffd00007ffd, i.e.
   * (1<<32)|0x7ffd), so any wild value must be dropped rather than written
   * through.  Canonical user-space only, and NOT 0x7ffffffdfffc: that is the
   * mmap_min_addr-shaped value the uninitialised slot keeps producing. */
  int cd = root && (root[0] & ~0x20) == (bblit_cd_drive() & ~0x20) && root[1] == ':';
  int ok_vol = vol && (uintptr_t)vol > 0x10000 && (uintptr_t)vol < 0x7ffffffdfffcUL
                   && ((uintptr_t)vol >> 47) == ((uintptr_t)vol >> 63);
  int ok_fs  = fs  && (uintptr_t)fs  > 0x10000 && (uintptr_t)fs  < 0x7ffffffdfffcUL
                   && ((uintptr_t)fs  >> 47) == ((uintptr_t)fs  >> 63);
  fprintf(stderr, "[gvi] root=%s vol=%p vlen=%u ok_vol=%d ok_fs=%d cd=%d\n",
          root ? root : "(nil)", (void *)vol, (unsigned)vlen, ok_vol, ok_fs, cd);
  if (cd) {
    if (ok_vol) { DWORD n = (vlen && vlen <= 0x200) ? vlen : 0x104;
                  /* the game compares the volume label against its own
                   * constant at 0x45f360 ("BBLIT", 5 chars + NUL), so mirror
                   * that exact string instead of a guessed label (observed:
                   * writing "BLIT" here made the CD check fail and opened
                   * the retry dialog) */
                  const char *lbl = (const char *)0x45f360ul;
                  strncpy(vol, lbl, n - 1); vol[n - 1] = 0; }
    if (ok_fs)  { DWORD n = (flen && flen <= 0x200) ? flen : 0x104;
                  strncpy(fs, "ISO9660", n - 1); fs[n - 1] = 0; }
  } else {
    if (ok_vol) *vol = 0;
    if (ok_fs)  *fs = 0;
  }
  return cd;
}  /* STUB-DIVERGENCE: wild/uninit buffer pointers are dropped instead of faulting */

/* ---- user32: windows / messages ---- */
static const char *g_class = "BBLIT_Game";
ATOM RegisterClassA(const WNDCLASSA *wc) { (void)wc; return 1; }   /* STUB-DIVERGENCE */
HWND CreateWindowExA(DWORD ex, LPCSTR cls, LPCSTR name, DWORD style,
                                  int x, int y, int w, int h, HWND parent, HMENU menu,
                                  HINSTANCE inst, LPVOID param)
{ (void)ex; (void)cls; (void)name; (void)style; (void)x; (void)y; (void)w; (void)h;
  (void)parent; (void)menu; (void)inst; (void)param; return (HWND)g_class; }  /* STUB-DIVERGENCE */
BOOL DestroyWindow(HWND h) { (void)h; return 1; }
BOOL ShowWindow(HWND h, int cmd) { (void)h; (void)cmd; return 1; }
BOOL UpdateWindow(HWND h) { (void)h; return 1; }
BOOL InvalidateRect(HWND h, const RECT *r, BOOL erase) { (void)h; (void)r; (void)erase; return 1; }
BOOL SetWindowPos(HWND h, HWND after, int x, int y, int w, int hh, UINT f)
{ (void)h; (void)after; (void)x; (void)y; (void)w; (void)hh; (void)f; return 1; }
BOOL BringWindowToTop(HWND h) { (void)h; return 1; }
BOOL IsIconic(HWND h) { (void)h; return 0; }
HWND GetForegroundWindow(void) { return 0; }
HWND SetFocus(HWND h) { (void)h; return h; }
HWND FindWindowA(LPCSTR cls, LPCSTR title) { (void)cls; (void)title; return 0; }
int MessageBoxA(HWND h, LPCSTR text, LPCSTR cap, UINT type)
{ (void)h; (void)type; fprintf(stderr, "%s: %s\n", cap ? cap : "", text ? text : ""); return 1; }
void PostQuitMessage(int code) { exit(code); }
BOOL PeekMessageA(LPMSG msg, HWND h, UINT lo, UINT hi, UINT remove)
{ (void)msg; (void)h; (void)lo; (void)hi; (void)remove; return 0; }  /* STUB-DIVERGENCE */
BOOL TranslateMessage(const MSG *msg) { (void)msg; return 0; }
LRESULT DispatchMessageA(const MSG *msg) { (void)msg; return 0; }
HICON LoadIconA(HINSTANCE h, LPCSTR id) { (void)h; (void)id; return 0; }
HCURSOR LoadCursorA(HINSTANCE h, LPCSTR id) { (void)h; (void)id; return 0; }
int LoadStringA(HINSTANCE h, UINT id, LPSTR buf, int n)
{ (void)h; (void)id; (void)n; if (buf) *buf = 0; return 0; }
HDC GetDC(HWND h) { (void)h; return (HDC)0x30000; }   /* STUB-DIVERGENCE */
int ReleaseDC(HWND h, HDC dc) { (void)h; (void)dc; return 1; }
BOOL GetClientRect(HWND h, RECT *r) { (void)h; r->left = r->top = 0; r->right = 640; r->bottom = 480; return 1; }
int32_t GetSystemMetrics(int i) { return i == 0 ? 640 : i == 1 ? 480 : 0; }  /* STUB-DIVERGENCE */
short GetKeyboardState(LPBYTE ks) { memset(ks, 0, 256); return 1; }

/* ---- gdi / opengl probe support ---- */
int ChoosePixelFormat(HDC dc, const PIXELFORMATDESCRIPTOR *pfd) { (void)dc; (void)pfd; fprintf(stderr, "[cpf]\n"); return 1; }
BOOL SetPixelFormat(HDC dc, int f, const PIXELFORMATDESCRIPTOR *pp) { (void)dc; (void)f; (void)pp; return 1; }

/* ---- kernel timing ---- */
DWORD GetTickCount(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (DWORD)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000); }
BOOL QueryPerformanceCounter(LARGE_INTEGER *c) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); c->QuadPart = (uint64_t)ts.tv_sec * 1000000ull + ts.tv_nsec / 1000; return 1; }
BOOL QueryPerformanceFrequency(LARGE_INTEGER *f) { f->QuadPart = 1000000; return 1; }
BOOL SetThreadPriority(HANDLE t, int p) { (void)t; (void)p; return 1; }
int GetThreadPriority(HANDLE t) { (void)t; return 0; }
UINT SetErrorMode(UINT m) { (void)m; return 0; }

/* ---- COM / DirectX: real COM is out of scope for stage 1 ---- */
HRESULT CoInitialize(LPVOID p) { (void)p; return 0; }         /* S_FALSE->0 ok */
void CoUninitialize(void) {}
HRESULT CoCreateInstance(const void *cls, void *outer, DWORD ctx, const void *iid, void **out)
{ (void)cls; (void)outer; (void)ctx; (void)iid; *out = 0; return 0x80004005u; }  /* E_NOINTERFACE */
HRESULT DirectDrawEnumerateA(void *cb, LPVOID ctx)
{ (void)cb; (void)ctx; return 1; }   /* DDERR_GENERIC: no ddraw devices */
HRESULT DirectDrawCreate(void *guid, void **dd, void *outer)
{ (void)guid; (void)outer; *dd = 0; return 1; }   /* DDERR_GENERIC */
HRESULT DirectSoundCreate(void *dev, void **ds, void *outer)
{ (void)dev; (void)outer; *ds = 0; return 1; }    /* DSERR_NODRIVER */

/* ---- loader / string conversion ---- */
#include <dlfcn.h>
HMODULE LoadLibraryA(LPCSTR name)
{
    if (!name) return 0;
    if (getenv("BBLIT_DEBUG")) { write(2, "[loadlib] ", 10); write(2, name, strlen(name)); write(2, "\n", 1); }
    if (strcmp(name, "opengl32.dll") == 0 || strcmp(name, "OPENGL32.dll") == 0
        || strcasecmp(name, "opengl32.dll") == 0)
        return (HMODULE)dlopen("libGL.so.1", RTLD_NOW | RTLD_GLOBAL);
    /* user32.dll delay-load from the CRT: nothing to load, stubs answer */
    if (strcasecmp(name, "user32.dll") == 0)
        return (HMODULE)0x20000;
    return (HMODULE)dlopen(name, RTLD_NOW);   /* STUB-DIVERGENCE: .dll names */
}
BOOL FreeLibrary(HMODULE m)
{
    /* ogl_load_dll opens opengl32 twice (probe, then real init) and closes the
     * old handle between the two; glibc keeps a second dlopen of the same lib
     * alive, but never unref it: GL context state would die with it. */
    (void)m;
    return 1;
}
static int wgl_noop(void)
{
    return 0;
}

FARPROC GetProcAddress(HMODULE m, LPCSTR name)
{
    if (m == (HMODULE)0x20000) return 0;      /* user32 stubs answered elsewhere */
    if (!m) return 0;
    if (!name) return 0;
    /* The game resolves every GL/wgl entry it stores in its 0x468750 import
     * table through here (360 names, gl* plus wgl*).  libGL on Linux has the
     * gl* names but not wgl*; glX* covers the context/swap ones, and the font
     * helpers have no GLX equivalent, so those resolve to a no-op stub. */
    if (getenv("BBLIT_DEBUG")) { write(2, "[gpaddr] ", 9); write(2, name, name ? strlen(name) : 5); write(2, "\n", 1); }
    void *s = dlsym((void *)m, name);
    if (s) return (FARPROC)s;
    static char wglbuf[64];
    if (strncmp(name, "wgl", 3) == 0) {
        const char *map = 0;
        if (!strcmp(name, "wglCreateContext")) map = "glXCreateContext";
        else if (!strcmp(name, "wglDeleteContext")) map = "glXDestroyContext";
        else if (!strcmp(name, "wglMakeCurrent")) map = "glXMakeCurrent";
        else if (!strcmp(name, "wglGetCurrentContext")) map = "glXGetCurrentContext";
        else if (!strcmp(name, "wglGetCurrentDC")) map = "glXGetCurrentDisplay";  /* near enough for probes */
        else if (!strcmp(name, "wglSwapBuffers")) map = "glXSwapBuffers";
        else if (!strcmp(name, "wglSwapLayerBuffers")) map = "glXSwapBuffers";
        else if (!strcmp(name, "wglGetProcAddress")) map = "glXGetProcAddress";
        else if (!strcmp(name, "wglChoosePixelFormat")) map = 0;  /* called via gdi shim instead */
        else if (!strcmp(name, "wglSetPixelFormat")) map = 0;
        else if (!strcmp(name, "wglDescribePixelFormat")) map = 0;
        else if (!strcmp(name, "wglGetPixelFormat")) map = 0;
        else if (!strcmp(name, "wglShareLists")) map = "glXCreateContext";
        if (map) { s = dlsym(RTLD_DEFAULT, map); if (s) return (FARPROC)s; }
        return (FARPROC)wgl_noop;   /* STUB-DIVERGENCE: font/layer helpers */
    }
    return (FARPROC)dlsym((void *)m, name);
}
int MultiByteToWideChar(UINT cp, DWORD flags, LPCSTR src, int slen, wchar_t *dst, int dlen)
{
    (void)cp; (void)flags;
    if (!src) return 0;
    size_t n = slen < 0 ? strlen(src) + 1 : (size_t)slen;
    if (!dst) return (int)n;
    if (dlen < (int)n) return 0;
    for (size_t i = 0; i < n; i++) dst[i] = (wchar_t)(unsigned char)src[i];
    return (int)n;
}   /* STUB-DIVERGENCE: latin-1 widening, no CP932/UTF-8 */
int WideCharToMultiByte(UINT cp, DWORD flags, const wchar_t *src, int slen, LPSTR dst,
                        int dlen, LPCSTR defch, BOOL *useddef)
{
    (void)cp; (void)flags; (void)defch;
    if (!src) return 0;
    size_t n = slen < 0 ? wcslen(src) + 1 : (size_t)slen;
    if (!dst) return (int)n;
    if (dlen < (int)n) return 0;
    for (size_t i = 0; i < n; i++) {
        dst[i] = (char)(src[i] & 0xff);
        if (useddef && src[i] > 0xff) *useddef = 1;
    }
    return (int)n;
}   /* STUB-DIVERGENCE: latin-1 narrowing */
