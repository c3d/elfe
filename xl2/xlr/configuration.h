#ifndef CONFIGURATION_H
#define CONFIGURATION_H
/* ************************************************************************* */
/*   configuration.h            (C) 1992-2000 Christophe de Dinechin (ddd)   */
/*                                                          XL2 project      */
/* ************************************************************************* */
/*                                                                           */
/*   File Description:                                                       */
/*                                                                           */
/*     Configuration file for various systems                                */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/* ************************************************************************* */
/* This document is distributed under the GNU General Public License         */
/* See the enclosed COPYING file or http://www.gnu.org for information       */
/* ****************************************************************************
   * File       : $RCSFile$
   * Revision   : $Revision$
   * Date       : $Date$
   ************************************************************************* */


// ============================================================================
//
//    Mozart internal configuration parameters
//
// ============================================================================

// This parameter is set to use static initialization for Mozart runtime
// components (Tones and Performers) in the code generated by tone_preprocessor
// This is because those are C++ classes, which would require constructor
// initialization, which in turn require run-time initialization of global
// objects, which is both slow, inefficient, and a bit dangerous, since
// the order of these initializations between translation units is not
// guaranteed.
// Using this option violates the C++ ODR (One Definition Rule), so
// this may break on some compiler, which is why it is an option. If you
// do not get unsatisfied symbols, then it is probably safe to use, and will
// be significantly faster and more efficient.
#define CONFIG_USE_STATIC_INIT  1


#ifdef CONFIG_LINUX
/* ========================================================================= */
/*                                                                           */
/*   Configuration for Linux (tested on i386 and PPC)                        */
/*                                                                           */
/* ========================================================================= */

#define CONFIG_HAS_UCHAR                1
#define CONFIG_HAS_UINT                 0
#define CONFIG_HAS_ULONG                0
#define CONFIG_HAS_USHORT               0
#define CONFIG_HAS_LONGLONG             1
#define CONFIG_LEADING_UNDERSCORE_IN_NM 0
#define CONFIG_DIRENT_HAS_TYPE          1
#define CONFIG_USE_SBRK                 1
#define CONFIG_C_COMPILER               "cc -g"
#define CONFIG_OBJ_EXT                  ".o"
#define CONFIG_EXE_EXT                  ""
#define CONFIG_DEFAULT_EXE              "a.out"

#endif /* LINUX */



#ifdef CONFIG_MACOSX
/* ========================================================================= */
/*                                                                           */
/*   The following is for MacOS X                                            */
/*                                                                           */
/* ========================================================================= */

#define CONFIG_HAS_UCHAR                1
#define CONFIG_HAS_UINT                 0
#define CONFIG_HAS_ULONG                1
#define CONFIG_HAS_USHORT               0
#define CONFIG_HAS_LONGLONG             1
#define CONFIG_HAS_INT64                0
#define CONFIG_LEADING_UNDERSCORE_IN_NM 1
#define CONFIG_DIRENT_HAS_TYPE          1
#define CONFIG_USE_SBRK                 1
#define CONFIG_C_COMPILER               "cc -g"
#define CONFIG_OBJ_EXT                  ".o"
#define CONFIG_EXE_EXT                  ""
#define CONFIG_DEFAULT_EXE              "a.out"

#endif /* MACOSX */


#ifdef CONFIG_HPUX
/* ========================================================================= */
/*                                                                           */
/*   Configuration for HPUX                                                  */
/*                                                                           */
/* ========================================================================= */

#define CONFIG_HAS_UCHAR                1
#define CONFIG_HAS_UINT                 0
#define CONFIG_HAS_ULONG                1
#define CONFIG_HAS_USHORT               0
#define CONFIG_HAS_LONGLONG             1
#define CONFIG_HAS_INT64                0
#define CONFIG_LEADING_UNDERSCORE_IN_NM 0
#define CONFIG_NM_DASH_P                1
#define CONFIG_DIRENT_HAS_TYPE          1
#define CONFIG_USE_SBRK                 1
#define CONFIG_C_COMPILER               "cc -g"
#define CONFIG_OBJ_EXT                  ".o"
#define CONFIG_EXE_EXT                  ""
#define CONFIG_DEFAULT_EXE              "a.out"

#endif /* HPUX */


#ifdef CONFIG_SUN
/* ========================================================================= */
/*                                                                           */
/*   Configuration for Solaris 8                                             */
/*                                                                           */
/* ========================================================================= */

#define CONFIG_HAS_UCHAR                1
#define CONFIG_HAS_UINT                 0
#define CONFIG_HAS_ULONG                1
#define CONFIG_HAS_USHORT               0
#define CONFIG_HAS_LONGLONG             1
#define CONFIG_HAS_INT64                0
#define CONFIG_LEADING_UNDERSCORE_IN_NM 0
#define CONFIG_NM_DASH_P                1
#define CONFIG_DIRENT_HAS_TYPE          0
#define CONFIG_USE_SBRK                 1
#define CONFIG_C_COMPILER               "gcc -g"
#define CONFIG_OBJ_EXT                  ".o"
#define CONFIG_EXE_EXT                  ""
#define CONFIG_DEFAULT_EXE              "a.out"

#endif /* HPUX */


#ifdef CONFIG_BEOS
/* ========================================================================= */
/*                                                                           */
/*   Configuration for BeOS, contributed by Attila Lendvai                   */
/*                                                                           */
/* ========================================================================= */

#define CONFIG_HAS_UCHAR                1
#define CONFIG_HAS_UINT                 0
#define CONFIG_HAS_ULONG                1
#define CONFIG_HAS_USHORT               0
#define CONFIG_HAS_LONGLONG             1
#define CONFIG_HAS_INT64                0
#define CONFIG_LEADING_UNDERSCORE_IN_NM 0
#define CONFIG_DIRENT_HAS_TYPE          1
#define CONFIG_USE_SBRK                 1
#define CONFIG_C_COMPILER               "cc -g"
#define CONFIG_OBJ_EXT                  ".o"
#define CONFIG_EXE_EXT                  ""
#define CONFIG_DEFAULT_EXE              "a.out"

#endif /* BEOS */


#ifdef CONFIG_CYGWIN
/* ========================================================================= */
/*                                                                           */
/*   Cygwin configuration (Bernard Hurley, Ingo Bruss)                       */
/*                                                                           */
/* ========================================================================= */

#define CONFIG_HAS_UCHAR                1
#define CONFIG_HAS_UINT                 0
#define CONFIG_HAS_ULONG                1
#define CONFIG_HAS_USHORT               0
#define CONFIG_HAS_LONGLONG             1
#define CONFIG_HAS_INT64                0
#define CONFIG_LEADING_UNDERSCORE_IN_NM 1
#define CONFIG_DIRENT_HAS_TYPE          0
#define CONFIG_USE_SBRK                 1
#define CONFIG_C_COMPILER               "gcc"
#define CONFIG_OBJ_EXT                  ".o"
#define CONFIG_EXE_EXT                  ".exe"
#define CONFIG_DEFAULT_EXE              "a.exe"

#endif /* CYGWIN */


#ifdef CONFIG_MSVC
/* ========================================================================= */
/*                                                                           */
/*   MS Visual C++ configuration (Juan Carlos Arevalo-Baeza)                 */
/*                                                                           */
/* ========================================================================= */

#define CONFIG_HAS_UCHAR                1
#define CONFIG_HAS_UINT                 1
#define CONFIG_HAS_ULONG                1
#define CONFIG_HAS_USHORT               1
#define CONFIG_HAS_LONGLONG             0
#define CONFIG_HAS_INT64                1
#define CONFIG_LEADING_UNDERSCORE_IN_NM 1
#define CONFIG_DIRENT_HAS_TYPE          0
#define CONFIG_USE_SBRK                 0
#define CONFIG_C_COMPILER               "cl /nologo"
#define CONFIG_OBJ_EXT                  ".obj"
#define CONFIG_EXE_EXT                  ".exe"
#define CONFIG_DEFAULT_EXE              "a.exe"

#endif /* MSVC */


#define CONFIG_HAS_NAMESPACE            1

#if CONFIG_HAS_LONGLONG
#define CONFIG_HUGE_FORMAT              "%lld"
#define CONFIG_UHUGE_FORMAT             "%llu"
#define CONFIG_XHUGE_FORMAT             "%lxl"
#define CONFIG_OHUGE_FORMAT             "%llo"
#define CONFIG_IHUGE_FORMAT             "%lli"
#define CONFIG_NAME_LONGLONG            "long long"
#define CONFIG_NAME_ULONGLONG           "unsigned long long"
#elif CONFIG_HAS_INT64
#define CONFIG_HUGE_FORMAT              "%I64d"
#define CONFIG_UHUGE_FORMAT             "%I64u"
#define CONFIG_XHUGE_FORMAT             "%I64x"
#define CONFIG_OHUGE_FORMAT             "%I64o"
#define CONFIG_IHUGE_FORMAT             "%I64i"
#define CONFIG_NAME_LONGLONG            "__int64"
#define CONFIG_NAME_ULONGLONG           "unsigned __int64"
#else
#define CONFIG_HUGE_FORMAT              "%ld"
#define CONFIG_UHUGE_FORMAT             "%lu"
#define CONFIG_XHUGE_FORMAT             "%xl"
#define CONFIG_OHUGE_FORMAT             "%lo"
#define CONFIG_IHUGE_FORMAT             "%li"
#define CONFIG_NAME_LONGLONG            "long"
#define CONFIG_NAME_ULONGLONG           "unsigned long"
#endif

#if CONFIG_DIRENT_HAS_TYPE
#define CONFIG_DIRENT_ISFILE(entry)     ((entry)->d_type == DT_REG || \
                                         (entry)->d_type == DT_LNK)
#define CONFIG_DIRENT_ISDIR(entry)      ((entry)->d_type == DT_DIR)
#else
#define CONFIG_DIRENT_ISFILE(entry)     true
#define CONFIG_DIRENT_ISDIR(entry)      true
#endif
#endif /* CONFIGURATION_H */
