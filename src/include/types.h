#ifndef _TYPES_H
#define _TYPES_H

typedef unsigned long size_t;
typedef signed long ssize_t;

typedef struct Point {
	unsigned int x;
	unsigned int y;
} Point;

typedef struct Time {
	unsigned short year;
	unsigned char month;
	unsigned char day;
	unsigned char hour;
	unsigned char minute;
	unsigned char second;
} Time;

/* Good enough for now... This is valid for GCC, at least, and *probably* all other common x86 compilers. We rely on GCC extensions anyhow. */
typedef signed   char  sint8;
typedef unsigned char  uint8;
typedef signed   short sint16;
typedef unsigned short uint16;
typedef signed   int   sint32;
typedef unsigned int   uint32;

/* this is usually in stdbool.h */
#define bool    _Bool
#define true    1
#define false   0
#define __bool_true_false_are_defined   1

#define NULL 0

#endif
