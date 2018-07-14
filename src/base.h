#ifndef BASE_H
#define BASE_H
/* ************************************************************************* */
/*   base.h                     (C) 1992-2000 Christophe de Dinechin (ddd)   */
/*                                                          XL project    */
/* ************************************************************************* */
/*                                                                           */
/*   File Description:                                                       */
/*                                                                           */
/*     Most basic things in the Mozart system:                               */
/*     - Basic types                                                         */
/*     - Debugging macros                                                    */
/*     - Derived configuration information                                   */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/* ************************************************************************* */
/* This document is released under the GNU General Public License, with the  */
/* following clarification and exception.                                    */
/*                                                                           */
/* Linking this library statically or dynamically with other modules is      */
/* a combined work based on this library. Thus, the terms and conditions of  */
/* the GNU General Public License cover the whole combination.               */
/*                                                                           */
/* As a special exception, the copyright holders of this library give you    */
/* permission to link this library with independent modules to produce an    */
/* executable, regardless of the license terms of these independent modules, */
/* and to copy and distribute the resulting executable under terms of your   */
/* choice, provided that you also meet, for each linked independent module,  */
/* the terms and conditions of the license of that module. An independent    */
/* module is a module which is not derived from or based on this library.    */
/* If you modify this library, you may extend this exception to your version */
/* of the library, but you are not obliged to do so. If you do not wish to   */
/* do so, delete this exception statement from your version.                 */
/*                                                                           */
/* See http://www.gnu.org/copyleft/gpl.html and Matthew 25:22 for details    */
/*  (C) 1992-2010 Christophe de Dinechin <christophe@taodyne.com>            */
/*  (C) 2010 Taodyne SAS                                                     */
/* ***************************************************************************/

/* Include the places where conflicting versions of some types may exist */
#include <sys/types.h>
#include <string>
#include <cstddef>
#include <stdint.h>

/* Include the configuration file */
#include "configuration.h"


/* ========================================================================= */
/*                                                                           */
/*    C/C++ differences that matter for include files                        */
/*                                                                           */
/* ========================================================================= */

#ifndef NULL
#  ifdef __cplusplus
#    define NULL        0
#  else
#    define NULL        ((void *) 0)
#  endif
#endif


#ifndef externc
#  ifdef __cplusplus
#    define externc     extern "C"
#  else
#    define externc     extern
#endif
#endif


#ifndef inline
#  ifndef __cplusplus
#    if defined(__GNUC__)
#      define inline      static __inline__
#    else
#      define inline      static
#    endif
#  endif
#endif


#ifndef true
#  ifndef __cplusplus
#    define true        1
#    define false       0
#    define bool        char
#  endif
#endif


#if defined(__GNUC__)
#define XL_MAYBE_UNUSED     __attribute((unused))
#elif __cplusplus > 201103L
#define XL_MAYBE_UNUSED     [[maybe_unused]]
#else
#define XL_MAYBE_UNUSED
#endif

/*===========================================================================*/
/*                                                                           */
/*  Common types                                                             */
/*                                                                           */
/*===========================================================================*/

/* Used for some byte manipulation, where it is more explicit that uchar */
typedef unsigned char   byte;

/* Shortcuts for unsigned numbers - Quite uneasy to figure out in general */
typedef unsigned char   uchar;
typedef unsigned short  ushort;
typedef unsigned int    uint;
typedef unsigned long   ulong;
 /* The largest available integer type */
typedef long long          longlong;
typedef unsigned long long ulonglong;
 /* Sized integers (dependent on the compiler...) - Only when needed */
typedef int8_t          int8;
typedef int16_t         int16;
typedef int32_t         int32;
typedef int64_t         int64;

typedef uint8_t         uint8;
typedef uint16_t        uint16;
typedef uint32_t        uint32;
typedef uint64_t        uint64;

/* A type that can be used to cast a pointer without data loss */
typedef intptr_t        ptrint;


/* Constant and non constant C-style string and void pointer */
typedef char *          cstring;
typedef const char *    kstring;
typedef void *          void_p;
typedef std::string     text;

/* Unicode character */
typedef wchar_t         wchar;
typedef wchar *         wcstring;
typedef const wchar *   wkstring;



/* ========================================================================= */
/*                                                                           */
/*   Debug information                                                       */
/*                                                                           */
/* ========================================================================= */
/*
   XL_ASSERT checks for some condition at runtime.
   XL_CASSERT checks for a condition at compile time
*/


#if !defined(XL_DEBUG) && (defined(DEBUG) || defined(_DEBUG))
#define XL_DEBUG        1
#endif

#ifdef XL_DEBUG
#define XL_ASSERT(x)    XL_ASSERT_(x,"assertion")
#define XL_REQUIRE(x)   XL_ASSERT_(x,"precondition")
#define XL_ENSURE(x)    XL_ASSERT_(x,"postcondition")

#define XL_ASSERT_(x,kind)                                              \
    do                                                                  \
    {                                                                   \
        if (!(x))                                                       \
        {                                                               \
            xl_assert_failed(kind, #x, __FILE__, __LINE__);             \
        }                                                               \
    } while (0)

/* Compile-time assert */
#define XL_CASSERT(x) struct __dummy { char foo[((int) (x))*2-1]; }
externc void xl_assert_failed(kstring kind, kstring msg, kstring file, uint line);
#define XL_DEBUG_CODE(x)        x

#else
#define XL_ASSERT(x)
#define XL_REQUIRE(x)
#define XL_ENSURE(x)
#define XL_CASSERT(x)
#define XL_DEBUG_CODE(x)
#endif


/* ========================================================================= */
/*                                                                           */
/*   Namespace support                                                       */
/*                                                                           */
/* ========================================================================= */

#if CONFIG_HAS_NAMESPACE
#define XL_BEGIN                namespace XL {
#define XL_END                  }
#else   /* !CONFIG_HAS_NAMESPACE */
#define XL_BEGIN
#define XL_END
#endif  /* ?CONFIG_HAS_NAMESPACE */


#endif /* ELEMENTALS_H */
