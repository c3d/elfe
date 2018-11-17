// ****************************************************************************
//  winglob.cpp                                                   ELFE project
// ****************************************************************************
//
//   File Description:
//
//
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

#include "config.h"

#ifndef HAVE_GLOB

#include "base.h"
#include "winglob.h"
#include <dirent.h>
#include <string>
#include <windows.h>


static void globinternal(text dir, text pattern, globpaths &paths)
// ----------------------------------------------------------------------------
//   Internal variant of glob, where dir is a directory to look into
// ----------------------------------------------------------------------------
{
    // Check if we are scanning a directory
    text subpat = "";
    bool dirmode = false;

    // Transform the regexp into a file-style regexp
    size_t pos = pattern.find_first_of("/\\");
    if (pos != pattern.npos)
    {
        subpat = pattern.substr(pos + 1, pattern.npos);
        pattern = pattern.substr(0, pos-1);
        dirmode = true;
    }

    // Open directory
    WIN32_FIND_DATAA fdata;
    HANDLE fhandle = FindFirstFileA(dir.c_str(), &fdata);
    bool more = fhandle != INVALID_HANDLE_VALUE;
    if (more)
    {
        do
        {
            std::string entry(fdata.cFileName);
            if (dirmode)
                globinternal(dir + "\\" + entry, subpat, paths);
            else
                paths.push_back(entry);
            more = FindNextFileA(fhandle, &fdata);
        } while (more);
        FindClose(fhandle);
    }
}


int glob(const char *pattern,
         int flags,
         int (*errfunc)(const char *epath, int errno),
         glob_t *pglob)
// ----------------------------------------------------------------------------
//   Simulation of the glob() function on Windows
// ----------------------------------------------------------------------------
{
    (void)flags;
    (void)errfunc;
    globinternal("", pattern, pglob->gl_pathv);
    pglob->gl_pathc = pglob->gl_pathv.size();
    return 0;
}


void globfree(glob_t *pglob)
// ----------------------------------------------------------------------------
//   Simulation of the globfree() function on Windows
// ----------------------------------------------------------------------------
{
    pglob->gl_pathv.clear();
}

#endif // !HAVE_GLOB
