// =================================================================================================================
// xs_types.h - copyright (c) 2006 2014 Sree Kotay - All rights reserved
// =================================================================================================================
#ifndef _xs_TYPES_H_
#define _xs_TYPES_H_

#include <limits.h>
#include <stdarg.h>

#if UINT_MAX==0xffffu           //int is 16-bits
typedef char                    xsint8;
typedef unsigned char           xsuint8;
typedef int                     xsint16;
typedef unsigned int            xsuint16;
typedef long                    xsint32;
typedef unsigned long           xsuint32;
typedef long long               xsint64;
typedef unsigned long long      xsuint64;

#elif UINT_MAX==0xffffffffu     //int is 32-bits
typedef char                    xsint8;
typedef unsigned char           xsuint8;
typedef short                   xsint16;
typedef unsigned short          xsuint16;
typedef int                     xsint32;
typedef unsigned int            xsuint32;
typedef long long               xsint64;
typedef unsigned long long      xsuint64;
#endif

#define xs_sizet_neg(a)         ((((a)&(((size_t)1)<<((sizeof(size_t)<<3)-1)))!=0))
#define xs_sizet_negzero(a)     (xs_sizet_neg(a) || (a)==0)

#endif //header
