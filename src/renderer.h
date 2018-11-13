#ifndef RENDERER_H
#define RENDERER_H
// ****************************************************************************
//  renderer.h                      (C) 1992-2009 Christophe de Dinechin (ddd)
//                                                               ELFE project
// ****************************************************************************
//
//   File Description:
//
//     Rendering of ELFE trees
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


#include "base.h"
#include "tree.h"
#include <ostream>

ELFE_BEGIN

struct Syntax;
typedef std::map<text,Tree_p>                      formats_table;
typedef std::map<Tree_p,text>                      highlights_table;
typedef std::pair<std::streampos, std::streampos>  stream_range;
typedef std::vector<stream_range>                  stream_ranges;
typedef std::map<text, stream_ranges>              highlight_result;

struct Renderer
// ----------------------------------------------------------------------------
//   Render a tree to some ostream
// ----------------------------------------------------------------------------
{
    // Construction
    Renderer(std::ostream &out, text styleFile, Syntax &stx);
    Renderer(std::ostream &out, Renderer *from = renderer);

    // Selecting the style sheet file
    void                SelectStyleSheet(text styleFile, text syntaxFile = "");

    // Rendering proper
    void                RenderFile (Tree *what);
    void                Render (Tree *what);
    void                RenderBody(Tree *what);
    void                RenderSeparators(char c);
    void                RenderText(text format);
    void                RenderIndents();
    void                RenderFormat(Tree *format);
    void                RenderFormat(text self, text format);
    void                RenderFormat(text self, text format, text generic);
    void                RenderFormat(text self, text f, text g1, text g2);
    Tree *              ImplicitBlock(Tree *t);
    bool                IsAmbiguousPrefix(Tree *test, bool testL, bool testR);
    bool                IsSubFunctionInfix(Tree *t);
    int                 InfixPriority(Tree *test);

protected:
    std::ostream &      output;
    Syntax &            syntax;
    formats_table       formats;
    highlights_table    highlights;
    highlight_result    highlighted;
    uint                indent;
    text                self;
    Tree_p              left;
    Tree_p              right;
    text                current_quote;
    int                 priority;
    bool                had_space;
    bool                had_newline;
    bool                had_punctuation;
    bool                need_separator;
    bool                need_newline;
    bool                no_indents;

public:
    static Renderer *   renderer;
};

std::ostream& operator<< (std::ostream&out, ELFE::Tree *t);
std::ostream& operator<< (std::ostream&out, ELFE::TreeList &list);

ELFE_END

// For use in a debugger
extern "C" const char *debug(ELFE::Tree *);
extern "C" const char *debugp(ELFE::Tree *);

#endif // RENDERER_H
