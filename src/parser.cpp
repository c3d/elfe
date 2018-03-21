// ****************************************************************************
//  parser.cpp                      (C) 1992-2003 Christophe de Dinechin (ddd)
//                                                               XL project
// ****************************************************************************
//
//   File Description:
//
//     XL language parser
//
//      See detailed description in parser.h
//
//
//
//
//
//
//
// ****************************************************************************
// This program is released under the GNU General Public License.
// See http://www.gnu.org/copyleft/gpl.html for details
//  (C) 1992-2010 Christophe de Dinechin <christophe@taodyne.com>
//  (C) 2010 Taodyne SAS
// ****************************************************************************

#include <vector>
#include <stdio.h>
#include <iostream>
#include "scanner.h"
#include "errors.h"
#include "tree.h"
#include "parser.h"
#include "options.h"



XL_BEGIN
// ============================================================================
//
//    Parser class itself
//
// ============================================================================

struct Pending
// ----------------------------------------------------------------------------
//   Pending expression while parsing
// ----------------------------------------------------------------------------
{
    Pending(text o, Tree *a, int p, ulong pos):
        opcode(o), argument(a), priority(p), position(pos) {}
    text   opcode;
    Tree_p argument;
    int    priority;
    ulong  position;
};


token_t Parser::NextToken()
// ----------------------------------------------------------------------------
//    Return the next token, skipping comments and gathering long text
// ----------------------------------------------------------------------------
{
    text opening, closing;
    while (true)
    {
        token_t pend = pending;
        if (pend != tokNONE && pend != tokNEWLINE)
        {
            pending = tokNONE;
            beginningLine = false;
            return pend;
        }

        // Here, there's nothing pending or only a newline
        token_t result = scanner.NextToken();
        hadSpaceBefore = scanner.HadSpaceBefore();
        hadSpaceAfter = scanner.HadSpaceAfter();
        record(parser, "Token %u %+s space before and %+s space after",
               result,
               hadSpaceBefore ? "with" : "without",
               hadSpaceAfter ? "with" : "without");

        switch(result)
        {
        case tokNAME:
        case tokSYMBOL:
            opening = scanner.NameValue();
            if (opening == "syntax")
            {
                record(parser, "Reading special syntax");
                syntax.ReadSyntaxFile(scanner, 0);
                record(parser, "End of special syntax");
                continue;
            }
            else if (syntax.IsComment(opening, closing))
            {
                // Skip comments, keep looking to get the right indentation
                record(parser, "Reading comment between '%s' and '%s'",
                       opening.c_str(), closing.c_str());
                text comment = opening + scanner.Comment(closing);
                AddComment(comment);
                record(parser, "Comment is %s", comment.c_str());
                if (closing == "\n" && pend == tokNONE)
                {
                    // If we had comments after a token, add them to that token
                    if (!beginningLine && comments.size() && commented)
                    {
                        AddComments(commented, false);
                        commented = NULL;
                    }

                    pending = tokNEWLINE;
                    beginningLine = true;
                }
                else
                {
                    // Don't change beginningLine in: /* ... */ /* ... */
                }
                continue;
            }
            else if (syntax.IsTextDelimiter(opening, closing))
            {
                record(parser, "Reading long text between '%s' and '%s'",
                       opening.c_str(), closing.c_str());
                text longText = scanner.Comment(closing, false);
                ulong cLen = closing.length();
                longText.erase(longText.length() - cLen, cLen);
                scanner.SetTextValue(longText);
                record(parser, "Long text was %s", longText.c_str());
                openquote = opening;
                closequote = closing;
                if (pend == tokNEWLINE)
                {
                    pending = tokLONGSTRING;
                    return tokNEWLINE;
                }
                if (closing == "\n" && pend == tokNONE)
                {
                    pending = tokNEWLINE;
                    beginningLine = true;
                }
                else
                {
                    beginningLine = false;
                }
                return tokLONGSTRING;
            }

            // If the next token has a substatement infix priority,
            // this takes over any pending newline. Example: else
            if (pend == tokNEWLINE)
            {
                int prefixPrio = syntax.PrefixPriority(opening);
                if (prefixPrio == syntax.default_priority)
                {
                    int infixPrio = syntax.InfixPriority(opening);
                    if (infixPrio < syntax.statement_priority)
                        pending = pend = tokNONE;
                }
            }

            // All comments after this will be following the token
            beginningLine = false;
            break;
        case tokNEWLINE:
            // Record actual new-lines and preceding comment
            opening = scanner.TextValue();
            if (!opening.empty())
            {
                AddComment(opening);

                // If we had comments after a token, add them to that token
                if (!beginningLine && comments.size() && commented)
                {
                    AddComments(commented, false);
                    commented = NULL;
                }
            }

            // Combine newline with any previous pending indent
            pending = tokNEWLINE;
            beginningLine = true;
            continue;
        case tokUNINDENT:
            opening = scanner.TextValue();
            if (!opening.empty())
            {
                AddComment(opening);

                // If we had comments after a token, add them to that token
                if (!beginningLine && comments.size() && commented)
                {
                    AddComments(commented, false);
                    commented = NULL;
                }
            }

            // Add newline if what comes next isn't an infix like 'else'
            pending = tokNEWLINE;
            beginningLine = true;
            return result;
        case tokINDENT:
            // If we had a new-line followed by indent, ignore the new line
            pending = tokNONE;
            beginningLine = true;
            return result;
        default:
            beginningLine = false;
            break;
        } // switch (result)

        // If we have another token here and a pending newline, push
        // the other token back.
        if (pend != tokNONE)
        {
            pending = result;
            beginningLine = true;
            return pend;
        }

        return result;
    } // While loop
}


void Parser::AddComments(Tree *what, bool before)
// ----------------------------------------------------------------------------
//   Add the pending comments to the given tree
// ----------------------------------------------------------------------------
{
    CommentsInfo *cinfo = what->GetInfo<CommentsInfo>();
    if (!cinfo)
    {
        cinfo =  new CommentsInfo();
        what->SetInfo<CommentsInfo> (cinfo);
    }
    if (before)
        cinfo->before = comments;
    else
        cinfo->after = comments;
    comments.clear();
}


static inline Tree *CreatePrefix(Tree *left, Tree *right, TreePosition pos)
// ----------------------------------------------------------------------------
//   Create a prefix, special-case unary minus with constants (feature #1580)
// ----------------------------------------------------------------------------
{
    if (Options::options && Options::options->signedConstants)
    {
        if (Name *name = left->AsName())
        {
            if (name->value == "-")
            {
                if (Integer *iv = right->AsInteger())
                {
                    iv->value = -iv->value;
                    return iv;
                }
                if (Real *rv = right->AsReal())
                {
                    rv->value = -rv->value;
                    return rv;
                }
            }
        }
    }
    return new Prefix(left, right, pos);
}


Tree *Parser::Parse(text closing)
// ----------------------------------------------------------------------------
//   Parse input
// ----------------------------------------------------------------------------
/* XL parsing is not very difficult, but a bit unusual, because it is based
   solely on dynamic information and not, for instance, on keywords.
   Consider the following cases, where p is "prefix-op" and i is "infix-op"
     Write A
       Parses as p(Write,A).
     A and B
       Parses as i(and,A,B) if 'and' has a priority,
              as p(A,p(and,B)) otherwise
     Write -A,B
       This parses as (Write-A),B since "-" has a priority.
       I wish I could fix that one...
       The correct XL syntax is: Write (-A),B
       We hope that semantic will catch such a case later and let us know...
 */
{
    Tree *               result             = NULL;
    Tree *               left               = NULL;
    Tree *               right              = NULL;
    text                 prefix             = "";
    bool                 done               = false;
    int                  default_priority   = syntax.default_priority;
    int                  function_priority  = syntax.function_priority;
    int                  statement_priority = syntax.statement_priority;
    int                  result_priority    = default_priority;
    int                  prefix_priority    = 0;
    int                  prefix_vs_infix    = 0;
    int                  postfix_priority   = 0;
    int                  infix_priority     = 0;
    int                  paren_priority     = syntax.InfixPriority(closing);
    bool                 is_expression      = false;
    bool                 new_statement      = true;
    ulong                pos                = 0;
    uint                 old_indent         = 0;
    text                 infix, name;
    text                 comment_end;
    token_t              tok;
    char                 separator;
    text                 blk_opening, blk_closing;
    std::vector<Pending> stack;
    CommentsList         pendingComments;

    // When inside a () block, we are in 'expression' mode right away
    if (closing != "" && paren_priority > statement_priority)
    {
        new_statement = false;
        is_expression = true;
    }

    while (!done)
    {
        bool wasBeginningLine = beginningLine;

        // Scan next token
        right = NULL;
        prefix_priority = infix_priority = default_priority;
        tok = NextToken();

        // If we had comments after a token, add them to that token
        if (!wasBeginningLine && comments.size() && commented)
            AddComments(commented, false);

        // Check token result
        pos = scanner.Position();
        record(parser, "Next token is %u at position %lu", tok, pos);
        switch(tok)
        {
        case tokEOF:
        case tokERROR:
            done = true;
            if (closing != "" && closing != Block::unindent)
                errors.Log(Error("Unexpected end of text, expected $1",
                                 scanner.Position()).Arg(closing));
            break;
        case tokINTEGER:
            right = new Integer(scanner.IntegerValue(), pos);
            prefix_priority = function_priority;
            break;
        case tokREAL:
            right = new Real(scanner.RealValue(), pos);
            prefix_priority = function_priority;
            break;
        case tokLONGSTRING:
            right = new Text(scanner.TextValue(), openquote, closequote, pos);
            if (!result && new_statement)
                is_expression = false;
            prefix_priority = function_priority;
            break;
        case tokSTRING:
        case tokQUOTE:
            separator = scanner.TokenText()[0];
            name = text(1, separator);
            right = new Text(scanner.TextValue(), name, name, pos);
            if (!result && new_statement)
                is_expression = false;
            prefix_priority = function_priority;
            break;
        case tokNAME:
        case tokSYMBOL:
            name = scanner.NameValue();
            if (name == closing)
            {
                done = true;
            }
            else if (Syntax *cs = syntax.HasSpecialSyntax(name,blk_closing))
            {
                // Read the input with the special syntax
                ulong pos = scanner.Position();
                record(scanner, "Special syntax %s to %s at position %lu",
                       name.c_str(), blk_closing.c_str(), pos);
                Parser childParser(scanner, cs);
                right = childParser.Parse(blk_closing);
                right = new Prefix(new Name(name), right, pos);
                pos = childParser.scanner.Position();
                scanner.SetPosition(pos);
                record(scanner, "Special syntax result %t new position %lu",
                       right, pos);
            }
            else if (!result)
            {
                prefix_priority = syntax.PrefixPriority(name);
                right = new Name(name, pos);
                if (prefix_priority == default_priority)
                    prefix_priority = function_priority;
                if (new_statement && tok == tokNAME)
                    is_expression = false;
            }
            else if (left)
            {
                // This is the right of an infix operator
                // If we have "A and not B", where "not" has
                // higher priority than "and", we want to
                // parse this as "A and (not B)" rather than as
                // "(A and not) B"
                prefix_priority = syntax.PrefixPriority(name);
                right = new Name(name, pos);
                if (prefix_priority == default_priority)
                    prefix_priority = function_priority;
            }
            else
            {
                // Complicated case: need to discriminate infix and prefix
                infix_priority = syntax.InfixPriority(name);
                prefix_vs_infix = syntax.PrefixPriority(name);
                if (infix_priority != default_priority &&
                    (prefix_vs_infix == default_priority ||
                     !hadSpaceBefore || hadSpaceAfter))
                {
                    // We got an infix
                    left = result;
                    infix = name;
                }
                else
                {
                    postfix_priority = syntax.PostfixPriority(name);
                    if (postfix_priority != default_priority)
                    {
                        // We have a postfix operator
                        right = new Name(name, pos);

                        // Flush higher priority items on stack
                        // This is the case for X:integer!
                        while (stack.size())
                        {
                            Pending &prev = stack.back();
                            if (!done &&
                                prev.priority != default_priority &&
                                postfix_priority > (prev.priority & ~1))
                                break;
                            if (prev.opcode == prefix)
                                result = CreatePrefix(prev.argument, result,
                                                      prev.position);
                            else
                                result = new Infix(prev.opcode, prev.argument,
                                                   result, prev.position);
                            stack.pop_back();
                        }
                        right = new Postfix(result, right, pos);
                        prefix_priority = postfix_priority;
                        result = NULL;
                    }
                    else
                    {
                        // No priority: take this as a prefix by default
                        right = new Name(name, pos);
                        prefix_priority = prefix_vs_infix;
                        if (prefix_priority == default_priority)
                        {
                            prefix_priority = function_priority;
                            if (new_statement && tok == tokNAME)
                                is_expression = false;
                        }
                    }
                }
            }
            break;
        case tokNEWLINE:
            // Consider new-line as an infix operator
            infix = "\n";
            name = infix;
            infix_priority = syntax.InfixPriority(infix);
            left = result;
            break;
        case tokPARCLOSE:
            // Check for mismatched parenthese here
            if (scanner.NameValue() != closing)
                errors.Log(Error("Mismatched parentheses: "
                                 "got $1, expected $2",
                                 pos).Arg(scanner.NameValue()).Arg(closing));
            done = true;
            break;
        case tokUNINDENT:
            // Check for mismatched blocks here
            if (closing != Block::unindent)
                errors.Log(Error("Mismatched identation, expected $1",
                                 pos).Arg(closing));
            done = true;
            break;
        case tokINDENT:
            scanner.SetTokenText(Block::indent);
            // Fall-through
        case tokPAROPEN:
            blk_opening = scanner.TokenText();
            if (!syntax.IsBlock(blk_opening, blk_closing))
                errors.Log(Error("Unknown parenthese type: $1 (internal)",
                                 pos).Arg(blk_opening));
            if (tok == tokPAROPEN)
                old_indent = scanner.OpenParen();
            name = blk_opening;
            paren_priority = syntax.InfixPriority(name);

            // Just like for names, parse the contents of the parentheses
            prefix_priority = paren_priority;
            infix_priority = default_priority;
            pendingComments = comments;
            comments.clear();
            right = Parse(blk_closing);
            if (tok == tokPAROPEN)
                scanner.CloseParen(old_indent);
            if (!right)
                right = new Name("", pos); // Case where we have ()
            right = new Block(right, blk_opening, blk_closing, pos);
            comments.insert(comments.end(),
                            pendingComments.begin(), pendingComments.end());
            break;
        default:
            if (true)
            {
                char buffer[20];
                sprintf(buffer, "%u", tok);
                errors.Log(Error("Internal error: unknown token $1 ($2)",
                                 pos).Arg(scanner.NameValue()).Arg(buffer));
            }
            break;
        } // switch(tok)

        record(parser, "Result %t priority %d", result, result_priority);

        // Attach any comments we may have had and return the result
        if (right)
        {
            commented = right;
            if (comments.size())
                AddComments(commented, true);
        }
        else if (left && (pending == tokNONE || pending == tokNEWLINE))
        {
            // We just got 'then', but 'then' will be an infix
            // so we can't really attach comments to it.
            // Instead, we defer the comment to the next 'right'
            commented = NULL;
        }


        // Check what is the current result
        if (!result)
        {
            // First thing we parse
            result = right;
            result_priority = prefix_priority;

            // We are now in the middle of an expression
            if (result && result_priority >= statement_priority)
                new_statement= false;
        }
        else if (left)
        {
            // Check if we had a statement separator
            if (infix_priority < statement_priority)
            {
                new_statement = true;
                is_expression = false;
            }

            // We got left and infix-op, we are now looking for right
            // If we have 'A and not B', where 'not' has higher priority
            // than 'and', we want to finish parsing 'not B' first, rather
            // than seeing this as '(A and not) B'.
            if (prefix_priority != default_priority)
            {
                // Push "A and" in the above example
                ulong st_pos = new_statement ? left->Position() : pos;
                stack.push_back(Pending(infix, left, infix_priority, st_pos));
                left = NULL;

                // Start over with "not"
                result = right;
                result_priority = prefix_priority;
            }
            else
            {
                while (stack.size())
                {
                    Pending &prev = stack.back();

                    // Check priorities compared to stack
                    // A + B * C, we got '*': keep "A+..." on stack
                    // Odd priorities are made right-associative by
                    // turning the low-bit off in the comparison below
                    if (!done &&
                        prev.priority != default_priority &&
                        infix_priority > (prev.priority & ~1))
                        break;
                    if (prev.opcode == prefix)
                        left = CreatePrefix(prev.argument, left, prev.position);
                    else
                        left = new Infix(prev.opcode, prev.argument, left,
                                         prev.position);
                    stack.pop_back();
                }

                // Now, we want to restart with the rightmost operand
                if (done)
                {
                    // End of text: the result is what we just got
                    result = left;
                }
                else
                {
                    // Something like A+B+C, just got second +
                    ulong st_pos = new_statement ? left->Position() : pos;
                    stack.push_back(Pending(infix,left,infix_priority,st_pos));
                    result = NULL;
                }
                left = NULL;
            }
        }
        else if (right)
        {
            // Check if we had a low-priority prefix (e.g. pragmas)
            if (prefix_priority < statement_priority)
            {
                new_statement = true;
                is_expression = false;
            }

            // Check priorities for something like "A.B x,y" -> "(A.B) (x,y)"
            // Odd priorities are made right associative by turning the
            // low bit off for the previous priority
            if (prefix_priority <= result_priority)
            {
                while (stack.size())
                {
                    Pending &prev = stack.back();
                    if (!done &&
                        prev.priority != default_priority &&
                        result_priority > (prev.priority & ~1))
                        break;
                    if (prev.opcode == prefix)
                        result = CreatePrefix(prev.argument, result,
                                              prev.position);
                    else
                        result = new Infix(prev.opcode, prev.argument,
                                           result, prev.position);
                    stack.pop_back();
                }
            }

            // Check if new statement
            if (!is_expression)
                if (result_priority > statement_priority)
                    if (stack.size() == 0 ||
                        stack.back().priority < statement_priority)
                        result_priority = statement_priority;

            // Push a recognized prefix op
            stack.push_back(Pending(prefix,result,result_priority,pos));
            result = right;
            result_priority = prefix_priority;
        }
    } // While(!done)

    if (stack.size())
    {
        if (!result)
        {
            Pending &last = stack.back();
            if (last.opcode != text("\n"))
                result = new Postfix(last.argument,
                                     new Name(last.opcode, last.position));
            else
                result = last.argument;
            stack.pop_back();
        }

        // Check if some stuff remains on stack
        while (stack.size())
        {
            Pending &prev = stack.back();
            if (prev.opcode == prefix)
                result = CreatePrefix(prev.argument, result, prev.position);
            else
                result = new Infix(prev.opcode, prev.argument,
                                   result, prev.position);
            stack.pop_back();
        }
    }

    return result;
}

XL_END

RECORDER(parser, 64, "Parser");
