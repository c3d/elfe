#ifndef ELFE_OPTIONS_H
#define ELFE_OPTIONS_H
// ****************************************************************************
//  Christophe de Dinechin                                       ELFE PROJECT
//  options.h
// ****************************************************************************
//
//   File Description:
//
//     Processing of ELFE compiler options
//
//
//
//
//
//
//
//
// ****************************************************************************
// This document is distributed under the GNU General Public License
// See the enclosed COPYING file or http://www.gnu.org for information
//  (C) 1992-2010 Christophe de Dinechin <christophe@taodyne.com>
//  (C) 2010 Taodyne SAS
// ****************************************************************************

#include <string>
#include <vector>
#include "base.h"

ELFE_BEGIN

struct Errors;


struct Options
/*---------------------------------------------------------------------------*/
/*  Class holding options for the compiler                                   */
/*---------------------------------------------------------------------------*/
{
  public:
    Options(int argc, char **argv);
    void Process();
    text LibPath(text name, text extension, text base)
    {
        if (name.rfind(extension) != name.length() - extension.length())
            name += extension;
        if (name.find("/") == name.npos)
            name = base + name;
        return name;
    }
    text LibRemap(text name, text oldpath, text newpath)
    {
        if (name.find(oldpath) == 0)
            name = name.replace(0, oldpath.length(), newpath);
        return name;
    }

  public:
    // Read options from the options.tbl file
#define OPTVAR(name, type, value)       type name;
#define OPTION(name, descr, code)
#include "options.tbl"
#undef OPTVAR
#undef OPTION

    uint                arg;
    std::vector<text>   args;
    std::vector<text>   files;

    static Options *    options;
};

ELFE_END

#endif /* ELFE_OPTIONS_H */
