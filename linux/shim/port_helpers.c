/* port_helpers.c -- small helpers the decompile-to-C transform emits. */
#include "winshim.h"
#include <string.h>
#include <math.h>

void data_wr3(void *base, long off, unsigned long v) {
    unsigned char *p = (unsigned char *)base + off;
    p[0] = (unsigned char)(v);
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
}
unsigned long data_rd3(void *base, long off) {
    const unsigned char *p = (const unsigned char *)base + off;
    return (unsigned long)p[0] | ((unsigned long)p[1] << 8) | ((unsigned long)p[2] << 16);
}
/* Ghidra renders cross-function label references as &LAB_x; the port resolves
 * them through this table (filled by linux/fixups.c). */
void *lab_addr(const char *name);
void *lab_addr(const char *name) { (void)name; return 0; }

/* Ghidra occasionally names an unmapped register as data (register0x...);
 * the single site in this binary uses it as a scratch pointer. */
void *reg_scratch;

/* Ghidra renders a bit-cast of a float expression to a pointer on functions with
 * broken return paths; keep the bit pattern.  f-variant takes a float, d-variant
 * a double, so the returned bits are the original variable's bits. */
unsigned long flt_bitsf(float v);
unsigned long flt_bitsf(float v) {
    unsigned int u;
    __builtin_memcpy(&u, &v, sizeof u);
    return u;
}
unsigned long flt_bitsd(double v);
unsigned long flt_bitsd(double v) {
    unsigned long u;
    __builtin_memcpy(&u, &v, sizeof u);
    return u;
}

/* ---- bit reinterpretation helpers (x86 slots are untyped dwords) ---- */
float u32_as_f(unsigned long bits) {
    float f; __builtin_memcpy(&f, &bits, sizeof f); return f;
}
double u32_as_d(unsigned long bits) {
    /* original x87 loaded a 32-bit float and widened */
    return (double)u32_as_f(bits);
}
long double u32_as_ld(unsigned long bits) {
    return (long double)u32_as_f(bits);
}

/* ---- x86/x87 idioms Ghidra names explicitly ---- */
long CARRY4(unsigned long a, unsigned long b) {
    return (long)(((unsigned long long)(unsigned int)a + (unsigned int)b) >> 32);
}
unsigned long long rdtsc(void) {
    return __builtin_ia32_rdtsc();
}
/* 8254 PIT channel-2 countdown, as sysinfo_cpu_detect programs it: mode 3
 * (square wave, out(0x43,0xb8)) with an initial count of 0, then the game
 * reads low/high bytes via in(0x42) pairs around a delay loop and derives
 * the CPU MHz.  A flat 0 makes the divisor 0 (SIGFPE in 0x1fffe/uVar16).
 * Report a steady counter so local_c lands at 200 ticks -> ~780 MHz,
 * comfortably above the 300 MHz software-renderer gate.  Other ports
 * have no emulation and keep reading 0. */
static unsigned short pit_count = 0xFF38;
static int pit_want_low = 1;
unsigned char in(unsigned short port) {
    if (port == 0x42) {
        unsigned char r;
        if (pit_want_low) {
            r = (unsigned char)(pit_count & 0xff);
        } else {
            r = (unsigned char)(pit_count >> 8);
            if (pit_count > 0xFE00) pit_count -= 3;
        }
        pit_want_low = !pit_want_low;
        return r;
    }
    (void)port; return 0;
}
void out(unsigned short port, unsigned char val) { (void)port; (void)val; }

long double f2xm1(long double x) { return exp2l((double)x) - 1.0L; }
long double fpatan(long double y, long double x) { return atan2l((double)y, (double)x); }
long double fscale(long double v, long double s) { return ldexpl((double)v, (int)floorl((double)s)); }
long double fcos(long double x) { return cosl((double)x); }

int *cpuid_basic_info(int leaf);
int *cpuid_Version_info(int leaf);
static int cpuid_out[4];
int *cpuid_basic_info(int leaf) {
    /* leaf 0: max leaf, vendor "GenuineIntel" */
    if (leaf == 0) {
        cpuid_out[0] = 1;
        __builtin_memcpy(&cpuid_out[1], "Genu", 4);
        __builtin_memcpy(&cpuid_out[2], "ineI", 4);
        __builtin_memcpy(&cpuid_out[3], "ntel", 4);
    } else {
        cpuid_out[0] = cpuid_out[1] = cpuid_out[2] = cpuid_out[3] = 0;
    }
    return cpuid_out;
}
int *cpuid_Version_info(int leaf) {
    /* leaf 1: family 6 model 3 stepping 3, no features advertised */
    (void)leaf;
    cpuid_out[0] = 0x633; cpuid_out[1] = 0; cpuid_out[2] = 0; cpuid_out[3] = 0;
    return cpuid_out;
}
