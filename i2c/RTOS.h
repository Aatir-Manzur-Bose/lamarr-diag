////////////////////////////////////////////////////////////////////////////////
/// @file RTOS.h
///
/// Copyright 2002 Bose Corporation, Framingham, MA
////////////////////////////////////////////////////////////////////////////////

#ifndef RTOS_H
#define RTOS_H

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include "TimeConvert.h"

#undef TRUE
#undef FALSE
#define TRUE 1
#define FALSE 0

#define ENOERROR 0

typedef int64_t INT64;
typedef int64_t SINT64;
typedef uint64_t UINT64;
typedef int INT32;
typedef signed long SINT32;
typedef unsigned long UINT32;
typedef short INT16;
typedef signed short SINT16;
typedef unsigned short UINT16;
typedef char INT8;
typedef char CHAR;
typedef signed char SINT8;
typedef unsigned char UINT8;

typedef void ( *DeviceCallback_t )( void* );
typedef SINT32 RemoteUIMessageId_t;

#include <sys/syscall.h>
//#define gettid() (syscall(SYS_gettid))

#endif //RTOS_H
