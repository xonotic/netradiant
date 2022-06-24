/* SPDX-License-Identifier: MIT License

Copyright © 2021 Thomas “illwieckz” Debesse

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the “Software”),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE. */

/* transformpath: transform file path based on keywords.

This is not an environment variable parser, this only supports
a set of keywords using common environment name syntax when it
exists to make the strings easier to read.

Supported substitution keywords,
Windows:

- %HOMEPATH%
- %USERPROFILE%
- %ProgramFiles%
- %ProgramFiles(x86)%
- %ProgramW6432%
- %APPDATA%
- [CSIDL_MYDOCUMENTS]

Supported substitution keywords,
Linux, FreeBSD, macOS:

- ~
- ${HOME}

Supported substitution keywords,
Linux, FreeBSD, other XDG systems:

- ${XDG_CONFIG_HOME}
- ${XDG_DATA_HOME}

Examples,
game engine directories:

- Windows: %ProgramFiles%\Unvanquished
- Linux: ${XDG_DATA_HOME}/unvanquished/base
- macOS: ${HOME}/Games/Unvanquished

Examples,
game home directories:

- Windows: [CSIDL_MYDOCUMENTS]\My Games\Unvanquished
- Linux: ${XDG_DATA_HOME}/unvanquished
- macOS: ${HOME}/Application Data/Unvanquished

*/

#include "globaldefs.h"
#include "stringio.h"

#include <cstring>
#include <string>

#if GDEF_OS_WINDOWS
#include <windows.h>
#include <iostream>
#include <shlobj.h>
#pragma comment(lib, "shell32.lib")
#endif // !GDEF_OS_WINDOWS

#if GDEF_OS_LINUX || GDEF_OS_BSD || GDEF_OS_MACOS
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#endif // !GDEF_OS_LINUX && !GDEF_OS_BSD && !GDEF_OS_MACOS

#if GDEF_OS_WINDOWS
static std::string getUserProfilePath();
#endif // !GDEF_OS_WINDOWS

static std::string getUserName()
{
#if GDEF_OS_WINDOWS
	std::string path( getenv( "USERNAME" ) );

	if ( ! path.empty() )
	{
		return path;
	}

	globalErrorStream() << "%USERNAME% not found.\n";

	return "";
#endif // !GDEF_OS_WINDOWS

#if GDEF_OS_LINUX || GDEF_OS_BSD || GDEF_OS_MACOS
	std::string path( getenv( "USERNAME" ) );

	if ( ! path.empty() )
	{
		return path;
	}

	globalErrorStream() << "${USERNAME} not found, guessing…\n";

	path = std::string( getenv( "LOGNAME" ) );

	if ( ! path.empty() )
	{
		return path;
	}

	globalErrorStream() << "${LOGNAME} not found, guessing…\n";

	path = std::string( getenv( "USER" ) );

	if ( ! path.empty() )
	{
		return path;
	}

	globalErrorStream() << "${USER} not found.\n";

	return "";
#endif // !GDEF_OS_LINUX && !GDEF_OS_BSD && !GDEF_OS_MACOS
}

static std::string getHomePath()
{
#if GDEF_OS_WINDOWS
	std::string path( getenv( "HOMEPATH" ) );

	if ( ! path.empty() )
	{
		return path;
	}

	globalErrorStream() << "%HOMEPATH% not found, guessing…\n";

	std::string path1 = getUserProfilePath();

	if ( ! path1.empty() )
	{
		return path1;
	}

	globalErrorStream() << "%HOMEPATH% not found.\n";

	return "";
#endif // !GDEF_OS_WINDOWS

#if GDEF_OS_LINUX || GDEF_OS_BSD || GDEF_OS_MACOS
	// Get the path environment variable.
	std::string path( getenv( "HOME" ) );

	// Look up path directory in password database.
	if( ! path.empty() )
	{
		return path;
	}

	globalErrorStream() << "${HOME} not found, guessing…\n";

	static char	buf[ 4096 ];
	struct passwd pw, *pwp;

	if ( getpwuid_r( getuid(), &pw, buf, sizeof( buf ), &pwp ) == 0 )
	{
		return std::string( pw.pw_dir );
	}

	globalErrorStream() << "${HOME} not found, guessing…\n";

	std::string path1( "/home/" );

	std::string path2 = getUserName();

	if ( ! path2.empty() )
	{
		return path1 + path2;
	}

	globalErrorStream() << "${HOME} not found…\n";

	return "";
#endif // !GDEF_OS_LINUX && !GDEF_OS_BSD && !GDEF_OS_MACOS
}

#if GDEF_OS_WINDOWS
static std::string getSystemDrive()
{
	std::string path( getenv( "SYSTEMDRIVE" ) );

	if ( ! path.empty() )
	{
		return path;
	}

	globalErrorStream() << "%SYSTEMDRIVE% not found, guessing…\n";

	return "C:";
}

static std::string getUserProfilePath()
{
	std::string path( getenv( "USERPROFILE" ) );

	if ( ! path.empty() )
	{
		return path;
	}

	globalErrorStream() << "%USERPROFILE% not found, guessing…\n";

	std::string path1 = getSystemDrive();
	std::string path2 = getUserName();

	if ( ! path2.empty() )
	{
		return path1 + "\\Users\\" + path2;
	}

	globalErrorStream() << "%USERPROFILE% not found.\n";

	return "";
}

static std::string getProgramFilesPath()
{
	std::string path( getenv( "ProgramFiles" ) );

	if ( ! path.empty() )
	{
		return path;
	}

	globalErrorStream() << "%ProgramFiles% not found, guessing…\n";

	std::string path1 = getSystemDrive();
	return path1 + "\\Program Files";
}

static std::string getProgramFilesX86Path()
{
	std::string path( getenv( "ProgramFiles(x86)" ) );

	if ( ! path.empty() )
	{
		return path;
	}

	globalErrorStream() << "%ProgramFiles(x86)% not found, guessing…\n";

	return getProgramFilesPath();
}

static std::string getProgramW6432Path()
{
	std::string path( getenv( "ProgramW6432" ) );

	if ( ! path.empty() )
	{
		return path;
	}

	globalErrorStream() << "%ProgramW6432% not found, guessing…\n";

	return getProgramFilesPath();
}

static std::string getAppDataPath()
{
	std::string path( getenv( "APPDATA" ) );

	if ( ! path.empty() )
	{
		return path;
	}

	globalErrorStream() << "%APPDATA% not found, guessing…\n";

	std::string path1 = getUserProfilePath();

	if ( ! path1.empty() )
	{
		return path1 + "\\AppData\\Roaming";
	}

	globalErrorStream() << "%APPDATA% not found.\n";

	return std::string( "" );
}

/* TODO: see also qFOLDERID_SavedGames in mainframe.cpp,
HomePaths_Realise and other things like that,
they look to be game paths, not NetRadiant paths. */

static std::string getMyDocumentsPath()
{
	CHAR path[ MAX_PATH ];
	HRESULT result = SHGetFolderPath(NULL, CSIDL_MYDOCUMENTS, NULL, SHGFP_TYPE_CURRENT, path);

	if ( result == S_OK )
	{
		return std::string( path );
	}

	globalErrorStream() << "[CSIDL_MYDOCUMENTS] not found, guessing…\n";

	std::string path1 = getHomePath();

	if ( ! path1.empty() )
	{
		path1 += "\\Documents";
		
		return path1;
	}

	globalErrorStream() << "[CSIDL_MYDOCUMENTS] not found.\n";

	return "";
}
#endif // !GDEF_OS_WINDOWS

#if GDEF_OS_XDG
static std::string getXdgConfigHomePath()
{
	/* FIXME: we may want to rely on g_get_user_config_dir()
	provided by GLib. */
	std::string path ( getenv( "XDG_CONFIG_HOME" ) );

	if ( ! path.empty() )
	{
		return path;
	}

	// This is not an error.
	// globalErrorStream() << "${XDG_CONFIG_HOME} not found, guessing…\n";

	std::string path1 = getHomePath();

	if ( ! path1.empty() )
	{
		return path1 + "/.config";
	}

	globalErrorStream() << "${XDG_CONFIG_HOME} not found.\n";

	return "";
}

static std::string getXdgDataHomePath()
{
	std::string path ( getenv( "XDG_DATA_HOME" ) );

	if ( ! path.empty() )
	{
		return path;
	}

	// This is not an error.
	// globalErrorStream() << "${XDG_DATA_HOME} not found, guessing…\n";

	std::string path1 = getHomePath();

	if ( ! path1.empty() )
	{
		return path1 + "/.local/share";
	}

	globalErrorStream() << "${XDG_DATA_HOME} not found.\n";

	return "";
}
#endif // GDEF_OS_XDG

struct pathTransformer_t
{
	std::string pattern;
	std::string ( *function )();
};

static const pathTransformer_t pathTransformers[] =
{
#if GDEF_OS_WINDOWS
	{ "%HOMEPATH%", getHomePath },
	{ "%USERPROFILE%", getUserProfilePath },
	{ "%ProgramFiles%", getProgramFilesPath },
	{ "%ProgramFiles(x86)%", getProgramFilesX86Path },
	{ "%ProgramW6432%", getProgramW6432Path },
	{ "%APPDATA%", getAppDataPath },
	{ "[CSIDL_MYDOCUMENTS]", getMyDocumentsPath },
#endif // GDEF_OS_WINDOWS

#if GDEF_OS_LINUX || GDEF_OS_BSD || GDEF_OS_MACOS
	{ "~", getHomePath },
	{ "${HOME}", getHomePath },
#endif // GDEF_OS_LINUX || GDEF_OS_BSD || GDEF_OS_MACOS

#if GDEF_OS_XDG
	{ "${XDG_CONFIG_HOME}", getXdgConfigHomePath },
	{ "${XDG_DATA_HOME}", getXdgDataHomePath },
#endif // GDEF_OS_XDG
};

/* If no transformation succeeds, the path will be returned untransformed. */
std::string transformPath( std::string transformedPath )
{
	for ( const pathTransformer_t &pathTransformer : pathTransformers )
	{
		if ( transformedPath.find( pathTransformer.pattern, 0 ) == 0 )
		{
			globalOutputStream() << "Path Transforming: '" << transformedPath.c_str() << "'\n";

			std::string path = pathTransformer.function();

			if ( ! path.empty() )
			{
				transformedPath.replace( 0, pathTransformer.pattern.length(), path );

				globalOutputStream() << "Path Transformed: '" << transformedPath.c_str() << "'\n";

				return transformedPath;
			}

			break;
		}
	}

	globalErrorStream() << "Path not transformed: '" << transformedPath.c_str() << "'\n";

	return transformedPath;
}
