/*
   Copyright (c) 2001, Loki software, inc.
   All rights reserved.

   Redistribution and use in source and binary forms, with or without modification,
   are permitted provided that the following conditions are met:

   Redistributions of source code must retain the above copyright notice, this list
   of conditions and the following disclaimer.

   Redistributions in binary form must reproduce the above copyright notice, this
   list of conditions and the following disclaimer in the documentation and/or
   other materials provided with the distribution.

   Neither the name of Loki software nor the names of its contributors may be used
   to endorse or promote products derived from this software without specific prior
   written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
   DIRECT,INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
   ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _VFS_H_
#define _VFS_H_

#include "globaldefs.h"

// to get PATH_MAX
#include <stdio.h>

#if GDEF_OS_WINDOWS
#include <wtypes.h>
#include <io.h>

#ifndef R_OK
#define R_OK 04
#endif

#define S_ISDIR( mode ) ( mode & _S_IFDIR )
#else // !GDEF_OS_WINDOWS
#include <dirent.h>
#include <unistd.h>
#endif // !GDEF_OS_WINDOWS

#ifndef PATH_MAX
#define PATH_MAX 260
#endif // PATH_MAX

// PATH_MAX
#if defined( __FreeBSD__ )
#include <sys/syslimits.h>
#endif

// Multiple pakpaths with many pk3dirs can lead
// to high list of VFS directories
#define VFS_MAXDIRS 256

void vfsInitDirectory( const char *path );
void vfsShutdown();
int vfsGetFileCount( const char *filename );
int vfsLoadFile( const char *filename, void **buffer, int index );
void vfsListShaderFiles( char* list, int *num );
qboolean vfsPackFile( const char *filename, const char *packname, const int compLevel );

extern char g_strForbiddenDirs[VFS_MAXDIRS][PATH_MAX + 1];
extern int g_numForbiddenDirs;

#endif // _VFS_H_
