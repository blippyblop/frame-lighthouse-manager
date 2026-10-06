#pragma once
#include <ctype.h>
/* POSIX character class macros missing from musl, expected by libstdc++. */
#ifndef _A
#define _A 0x0001
#define _B 0x0002
#define _C 0x0004
#define _L 0x0008
#define _N 0x0010
#define _P 0x0020
#define _S 0x0040
#define _U 0x0080
#define _X 0x0100
#endif
