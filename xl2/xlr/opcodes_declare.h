// ****************************************************************************
//  opcodes_declare.h               (C) 1992-2009 Christophe de Dinechin (ddd) 
//                                                                 XL2 project 
// ****************************************************************************
// 
//   File Description:
// 
//     Macros used to declare built-ins.
// 
//     Usage:
//     #include "opcodes_declare.h"
//     #include "builtins.tbl"
// 
//     #include "opcodes_define.h"
//     #include "builtins.tbl"
//
// 
// ****************************************************************************
// This document is released under the GNU General Public License.
// See http://www.gnu.org/copyleft/gpl.html and Matthew 25:22 for details
// ****************************************************************************
// * File       : $RCSFile$
// * Revision   : $Revision$
// * Date       : $Date$
// ****************************************************************************

#undef INFIX
#undef PREFIX
#undef POSTFIX
#undef NAME
#undef TYPE
#undef PARM
#undef MPARM

#define INFIX(t1, symbol, t2, name, code)       \
    Tree *xl_##name(Tree *l, Tree *r) { code; }

#define MPARM(symbol, type)     type##_t symbol,
#define PARM(symbol, type)      type##_t symbol

#define PREFIX(symbol, parms, name, code)       \
    Tree *xl_##name(parms) { code; }

#define POSTFIX(parms, symbol, name, code)       \
    Tree *xl_##name(parms) { code; }

#define NAME(symbol)    \
    Name *xl_##symbol;

#define TYPE(symbol)                            \
    Name *xl_##symbol##_name;                   \
    extern Tree *xl_##symbol(Tree *value);