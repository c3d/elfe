#ifndef TYPES_H
#define TYPES_H
// ****************************************************************************
//  types.h                                                       XL project
// ****************************************************************************
//
//   File Description:
//
//     The type system in XL
//
//
//
//
//
//
//
//
// ****************************************************************************
// This document is released under the GNU General Public License, with the
// following clarification and exception.
//
// Linking this library statically or dynamically with other modules is making
// a combined work based on this library. Thus, the terms and conditions of the
// GNU General Public License cover the whole combination.
//
// As a special exception, the copyright holders of this library give you
// permission to link this library with independent modules to produce an
// executable, regardless of the license terms of these independent modules,
// and to copy and distribute the resulting executable under terms of your
// choice, provided that you also meet, for each linked independent module,
// the terms and conditions of the license of that module. An independent
// module is a module which is not derived from or based on this library.
// If you modify this library, you may extend this exception to your version
// of the library, but you are not obliged to do so. If you do not wish to
// do so, delete this exception statement from your version.
//
// See http://www.gnu.org/copyleft/gpl.html and Matthew 25:22 for details
//  (C) 1992-2010 Christophe de Dinechin <christophe@taodyne.com>
//  (C) 2010 Taodyne SAS
// ****************************************************************************
//
//  The type system in XL is somewhat similar to what is found in Haskell,
//  except that it's based on the shape of trees.
//
//  A type form in XL can be:
//   - A type name              integer
//   - A litteral value         0       1.5             "Hello"
//   - A range of values        0..4    1.3..8.9        "A".."Z"
//   - A union of types         0,3,5   integer|real
//   - A block for precedence   (real)
//   - A rewrite specifier      integer => real
//   - The type of a pattern    type (X:integer, Y:integer)
//
//  REVISIT: The form A => B is to distinguish from a rewrite itself.
//  Not sure if this is desirable.

#include "tree.h"
#include "context.h"
#include <map>


XL_BEGIN

// ============================================================================
//
//   Forward classes
//
// ============================================================================

struct RewriteCalls;
typedef GCPtr<RewriteCalls>              RewriteCalls_p;
typedef std::map<Tree_p, RewriteCalls_p> rcall_map;

extern Name_p tree_type;


// ============================================================================
//
//   Class used to infer types in a program (hacked Damas-Hindley-Milner)
//
// ============================================================================

class Types
// ----------------------------------------------------------------------------
//   Record type information
// ----------------------------------------------------------------------------
{
    Context_p   context;        // Context in which we lookup things
    TreeMap     types;          // Map an expression to its type
    TreeMap     unifications;   // Map a type to its reference type
    rcall_map   rcalls;         // Rewrites to call for a given tree
    Tree_p      left, right;    // Current left and right of unification
    bool        prototyping;    // Prototyping a function declaration
    bool        matching;       // Matching a pattern
    static ulong id;            // Id of next type

public:
    Types(Scope *scope);
    Types(Scope *scope, Types *parent);
    ~Types();
    typedef bool value_type;
    enum unify_mode { STANDARD, DECLARATION };

public:
    // Main entry point
    bool        TypeAnalysis(Tree *source);
    Tree *      Type(Tree *expr);
    rcall_map & RewriteCalls();

public:
    // Interface for Tree::Do() to annotate the tree
    bool        DoInteger(Integer *what);
    bool        DoReal(Real *what);
    bool        DoText(Text *what);
    bool        DoName(Name *what);
    bool        DoPrefix(Prefix *what);
    bool        DoPostfix(Postfix *what);
    bool        DoInfix(Infix *what);
    bool        DoBlock(Block *what);

public:
    // Common code for all constants (integer, real, text)
    bool        DoConstant(Tree *what);

    // Annotate expressions with type variables
    bool        AssignType(Tree *expr, Tree *type = NULL);
    bool        Rewrite(Infix *rewrite);
    bool        Data(Tree *form);
    bool        Extern(Tree *form);
    bool        Statements(Tree *expr, Tree *left, Tree *right);

    // Attempt to evaluate an expression and perform required unifications
    bool        Evaluate(Tree *tree);

    // Indicates that two trees must have compatible types
    bool        UnifyExpressionTypes(Tree *expr1, Tree *expr2);
    bool        Unify(Tree *t1, Tree *t2,
                      Tree *x1, Tree *x2, unify_mode mode = STANDARD);
    bool        Unify(Tree *t1, Tree *t2, unify_mode mode = STANDARD);
    bool        Join(Tree *base, Tree *other, bool knownGood = false);
    bool        JoinConstant(Name *tname, Tree *cst);
    bool        UnifyPatterns(Tree *t1, Tree *t2);
    bool        UnifyPatternAndValue(Tree *pat, Tree *val);
    bool        Commit(Types *child);

    // Return the base type associated with a given tree
    Tree *      Base(Tree *type);
    bool        IsGeneric(text name);
    bool        IsGeneric(Tree *type);
    bool        IsTypeName(Tree *type);

    // Type constructors
    Tree *      TypePattern(Tree *type);

    // Generation of type names
    Name *      NewTypeName(TreePosition pos);

    // Lookup a type name in the given context
    Tree *      LookupTypeName(Tree *input);

    // Error messages
    bool        TypeError(Tree *t1, Tree *t2);

public:
    GARBAGE_COLLECT(Types);
};
typedef GCPtr<Types> Types_p;



// ============================================================================
//
//   High-level entry points for type management
//
// ============================================================================

Tree *ValueMatchesType(Context *, Tree *type, Tree *value, bool conversions);
Tree *TypeCoversType(Context *, Tree *type, Tree *test, bool conversions);
Tree *TypeIntersectsType(Context *, Tree *type, Tree *test, bool conversions);
Tree *UnionType(Context *, Tree *t1, Tree *t2);
Tree *CanonicalType(Tree *value);
Tree *StructuredType(Context *, Tree *value);
bool  IsTreeType(Tree *type);



// ============================================================================
//
//    Representation of types
//
// ============================================================================

struct TypeInfo : Info
// ----------------------------------------------------------------------------
//    Information recording the type of a given tree
// ----------------------------------------------------------------------------
{
    TypeInfo(Tree *type): type(type) {}
    typedef Tree_p       data_t;
    operator             data_t()  { return type; }
    Tree_p               type;
};



// ============================================================================
//
//   Inline functions
//
// ============================================================================

inline bool Types::IsGeneric(text name)
// ----------------------------------------------------------------------------
//   Check if a given type is a generated generic type name
// ----------------------------------------------------------------------------
{
    return name.size() && name[0] == '#';
}


inline bool Types::IsGeneric(Tree *type)
// ----------------------------------------------------------------------------
//   Check if a given type is a generated generic type name
// ----------------------------------------------------------------------------
{
    if (Name *name = type->AsName())
        return IsGeneric(name->value);
    return false;
}


inline bool Types::IsTypeName(Tree *type)
// ----------------------------------------------------------------------------
//   Check if a given type is a 'true' type name, i.e. not generated
// ----------------------------------------------------------------------------
{
    if (Name *name = type->AsName())
        return !IsGeneric(name->value);
    return false;
}


inline bool IsTreeType(Tree *type)
// ----------------------------------------------------------------------------
//   Return true for any 'tree' type
// ----------------------------------------------------------------------------
{
    return type == tree_type;
}

XL_END

extern "C" void debugt(XL::Types *ti);
extern "C" void debugu(XL::Types *ti);
extern "C" void debugr(XL::Types *ti);

RECORDER_DECLARE(types);

#endif // TYPES_H
