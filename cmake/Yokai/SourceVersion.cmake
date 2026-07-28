# Daemon BSD Source Code
# Copyright (c) 2024-2026, Daemon Developers
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the of the Daemon developers nor the
#    component_names of its contributors may be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL DAEMON DEVELOPERS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

################################################################################
# Source version and date detection.
################################################################################

include("${CMAKE_CURRENT_LIST_DIR}/GitHelpers.cmake")

option(YOKAI_SOURCE_ALLOW_STRAY "Allow stray repositories not tracked by Git" OFF)
mark_as_advanced(YOKAI_SOURCE_ALLOW_STRAY)

option(YOKAI_SOURCE_ASSUME_TARBALL "Assume the repository is a tarball extraction: extract version and date from dir name and file metadata" OFF)
mark_as_advanced(YOKAI_SOURCE_ASSUME_TARBALL)

set(YOKAI_SOURCE_OVERRIDE_VERSION "" CACHE STRING "Override the version string (empty: detect)")
mark_as_advanced(YOKAI_SOURCE_OVERRIDE_VERSION)

set(YOKAI_SOURCE_OVERRIDE_DATE "" CACHE STRING "Override the date string (empty: detect, format: UTC ISO-8601 or RFC-3339)")
mark_as_advanced(YOKAI_SOURCE_OVERRIDE_DATE)

set(YOKAI_SOURCE_SHORT_REF_LENGTH "7" CACHE STRING "Length of the short Git reference")
mark_as_advanced(YOKAI_SOURCE_SHORT_REF_LENGTH)

if (YOKAI_SOURCE_OVERRIDE_VERSION STREQUAL "")
	set(YOKAI_RUNTIME_GENERIC_VERSION "0")
else()
	set(YOKAI_RUNTIME_GENERIC_VERSION "${YOKAI_SOURCE_OVERRIDE_VERSION}")
endif()

if (YOKAI_SOURCE_OVERRIDE_DATE STREQUAL "")
	string(TIMESTAMP YOKAI_RUNTIME_GENERIC_DATE "@%s" UTC)
else()
	if (YOKAI_SOURCE_OVERRIDE_DATE
	MATCHES "^[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]T[0-9][0-9]:[0-9][0-9]:[0-9][0-9]\+00:00$")
		set(YOKAI_RUNTIME_GENERIC_DATE "${YOKAI_SOURCE_OVERRIDE_DATE}")
	elseif (YOKAI_SOURCE_OVERRIDE_DATE
	MATCHES "^[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]T[0-9][0-9]:[0-9][0-9]:[0-9][0-9]$")
		set(YOKAI_RUNTIME_GENERIC_DATE "${YOKAI_SOURCE_OVERRIDE_DATE}")
	elseif (YOKAI_SOURCE_OVERRIDE_DATE
	MATCHES "^[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9] [0-9][0-9]:[0-9][0-9]:[0-9][0-9]\+00:00$")
		set(YOKAI_RUNTIME_GENERIC_DATE "${YOKAI_SOURCE_OVERRIDE_DATE}")
	elseif (YOKAI_SOURCE_OVERRIDE_DATE
	MATCHES "^[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9] [0-9][0-9]:[0-9][0-9]:[0-9][0-9]$")
		set(YOKAI_RUNTIME_GENERIC_DATE "${YOKAI_SOURCE_OVERRIDE_DATE}")
	elseif (YOKAI_SOURCE_OVERRIDE_DATE
	MATCHES "^[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]$")
		set(YOKAI_RUNTIME_GENERIC_DATE "${YOKAI_SOURCE_OVERRIDE_DATE} 00:00:00")
	else()
		message(FATAL_ERROR "Invalid YOKAI_SOURCE_OVERRIDE_DATE format: ${YOKAI_SOURCE_OVERRIDE_DATE}")
	endif()
endif()

macro(yokai_generate_source_version version date)
	if ("${YOKAI_SOURCE_OVERRIDE_VERSION}" STREQUAL "")
		set(version_string "${version}")
	else()
		set(version_string "${YOKAI_SOURCE_OVERRIDE_VERSION}")
	endif()

	message(STATUS "Detected source version: ${version}")
	message(STATUS "Detected source date: ${date}")

	set(YOKAI_SOURCE_VERSION_STRING
		"${version_string}"
		PARENT_SCOPE
	)

	set(YOKAI_SOURCE_DATE_STRING
		"${date}"
		PARENT_SCOPE
	)
endmacro()

function(yokai_compact_date date OUTPUT_VAR)
	string(REPLACE "+00:00" "" compact_date "${date}")
	string(REPLACE "-" "" compact_date "${compact_date}")
	string(REPLACE ":" "" compact_date "${compact_date}")
	string(REPLACE "T" "-" compact_date "${compact_date}")
	string(REPLACE " " "-" compact_date "${compact_date}")
	set(${OUTPUT_VAR} "${compact_date}" PARENT_SCOPE)
endfunction()

function(yokai_detect_source_version)
	set(options ASSUME_TARBALL ALLOW_STRAY VERBOSE GLOBAL)
	cmake_parse_arguments(ARG "${options}" "" "" ${ARGN})

	if (ASSUME_TARBALL OR YOKAI_SOURCE_ASSUME_TARBALL)
		# This assumes the source directory name is suffixed with the version,
		# and that the date of the files are set to the commit date.
		get_filename_component(source_dir_name "${CMAKE_SOURCE_DIR}" NAME)

		if (source_dir_name MATCHES "([0-9]+(\\.[0-9]+)*)$")
			set(version "${CMAKE_MATCH_1}")
		endif()

		if (version STREQUAL "")
			message(FATAL_ERROR "Failed to extract version from directory name")
		endif()

		file(TIMESTAMP "${CMAKE_SOURCE_DIR}/CMakeLists.txt" date "%Y-%m-%d %H:%M:%S" UTC)

		yokai_generate_source_version("${version}" "${date}")

		return()
	endif()

	set(date "${YOKAI_RUNTIME_GENERIC_DATE}")

	yokai_compact_date("${date}" compact_date)

	# Fallback version and date string for stray repositories (not tracked by git yet).
	set(version_string "0")
	set(date_string "-${compact_date}")
	set(ref_string "")
	set(dirt_string "+stray")

	# Test that Git is available and working.
	yokai_git_run(
		git_version
		ALLOW_FAILURE
		${ARG_UNPARSED_ARGUMENTS}
		-v
	)

	if (NOT result EQUAL 0)
		message(FATAL_ERROR "Git is not working, enable YOKAI_SOURCE_ALLOW_STRAY when not building a git repository")
	endif()

	# Git returns an error when the directory is not a Git repository or it has no
	# commits. It also may return an error when the repo is broken somehow. We fail
	# to distinguish these cases and assume it is not a Git repository.
	yokai_git_run(
		git_last_commit
		ALLOW_FAILURE
		${ARG_UNPARSED_ARGUMENTS}
		rev-parse HEAD --
	)

	if (NOT result EQUAL 0)
		if (ARG_ALLOW_STRAY OR YOKAI_SOURCE_ALLOW_STRAY)
			if ("${YOKAI_SOURCE_OVERRIDE_VERSION}" STREQUAL "")
				message(WARNING "You better set YOKAI_SOURCE_OVERRIDE_VERSION when using YOKAI_SOURCE_ALLOW_STRAY")
			endif()

			if ("${YOKAI_SOURCE_OVERRIDE_DATE}" STREQUAL "")
				message(WARNING "You better set YOKAI_SOURCE_OVERRIDE_DATE when using YOKAI_SOURCE_ALLOW_STRAY")
			endif()

			set(version "${tag_string}${date_string}${dirt_string}")

			yokai_generate_source_version("${version}" "${date}")

			return()
		endif()

		message(FATAL_ERROR "'${CMAKE_SOURCE_DIR}' is not a valid Git repository, enable YOKAI_SOURCE_ALLOW_STRAY when not building a git repository.")
	endif()

	# Now we know we must use a Git-based version string.
	set(dirt_string "")

	# Git prints the current commit reference.
	string(SUBSTRING "${git_last_commit}" 0 ${YOKAI_SOURCE_SHORT_REF_LENGTH} short_hash)
	set(ref_string "-${short_hash}")

	# Git prints the current commit date.
	yokai_git_get_date(date)

	yokai_compact_date("${date}" compact_date)

	set(date_string "-${compact_date}")

	# Git prints the most recent tag, or a commit hash when there is no tag at all.
	yokai_git_get_closest_tag(git_closest_tag)

	# If a tag is found:
	if (git_closest_tag MATCHES "^v")
		string(SUBSTRING "${git_closest_tag}" 1 -1 tag_string)

		# Git prints a version string that is equal to the most recent tag
		# when the most recent tag is on the current commit.
		yokai_git_describe_tag(git_describe_tag)

		string(SUBSTRING "${git_describe_tag}" 1 -1 describe_version)

		if (tag_string STREQUAL describe_version)
			# Do not write current commit reference and date in version
			# string when the tag is on the current commit.
			set(date_string "")
			set(ref_string "")
		endif()
	endif()

	yokai_git_is_dirty(
		dirty
		${ARG_UNPARSED_ARGUMENTS}
	)

	if (dirty)
		# Write the dirty flag in version string when not everything in
		# the Git repository is properly committed.
		set(dirt_string "+dirty")
	endif()

	set(version "${tag_string}${date_string}${ref_string}${dirt_string}")

	yokai_generate_source_version("${version}" "${date}")
endfunction()

yokai_detect_source_version()

if (YOKAI_SOURCE_GENERATOR)
	# Add printable strings to the executable.
	yokai_add_buildinfo("char*" "YOKAI_SOURCE_VERSION_STRING" "\"${YOKAI_SOURCE_VERSION_STRING}\"")
	yokai_add_buildinfo("char*" "YOKAI_SOURCE_DATE_STRING" "\"${YOKAI_SOURCE_DATE_STRING}\"")
endif()
