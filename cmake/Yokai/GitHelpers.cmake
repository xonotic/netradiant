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
#  * Neither the name of the Daemon developers nor the
#    names of its contributors may be used to endorse or promote products
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
# Git helpers.
################################################################################

include_guard(GLOBAL)

option(YOKAI_GIT_USE_GLOBAL "Look for dirt in the whole repository, not only the given directory" OFF)
mark_as_advanced(YOKAI_GIT_USE_GLOBAL)

option(YOKAI_GIT_USE_VERBOSE "Do not silence Git errors" OFF)
mark_as_advanced(YOKAI_GIT_USE_VERBOSE)

# Do not require Git on stray repositories (tarballs).
find_package(Git QUIET)

function(yokai_git_run OUTPUT_VAR)
	set(options ALLOW_FAILURE VERBOSE)
	cmake_parse_arguments(ARG "${options}" "" "" ${ARGN})

	if (NOT GIT_FOUND)
		message(FATAL_ERROR "Git unavailable")
		return()
	endif()

	if (NOT YOKAI_GIT_ALLOW_STRAY AND NOT GIT_FOUND)
		message(FATAL_ERROR "Git is not available, enable YOKAI_GIT_ALLOW_STRAY when not building a git repository")
	endif()

	if (ARG_VERBOSE OR YOKAI_GIT_USE_VERBOSE)
		set(error_args)
	else()
		set(error_args ERROR_QUIET)
	endif()

	execute_process(
		COMMAND "${GIT_EXECUTABLE}" ${ARG_UNPARSED_ARGUMENTS}
		WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
		RESULT_VARIABLE result
		OUTPUT_VARIABLE output
		OUTPUT_STRIP_TRAILING_WHITESPACE
		${error_args}
	)

	if (NOT ARG_ALLOW_FAILURE AND NOT result EQUAL 0)
		message(FATAL_ERROR "Git command failed: ${GIT_EXECUTABLE} -C \"${CMAKE_SOURCE_DIR}\" ${ARG_UNPARSED_ARGUMENTS}")
	endif()

	set(${OUTPUT_VAR} "${output}" PARENT_SCOPE)
	set(result "${result}" PARENT_SCOPE)
endfunction()

function(yokai_git_is_dirty OUTPUT_VAR)
	set(options GLOBAL VERBOSE)
	cmake_parse_arguments(ARG "${options}" "" "" ${ARGN})

	if (ARG_GLOBAL OR YOKAI_GIT_USE_GLOBAL)
		set(lookup)
	else()
		set(lookup "${CMAKE_SOURCE_DIR}")
	endif()

	yokai_git_run(
		"${CMAKE_SOURCE_DIR}"
		status
		${ARG_UNPARSED_ARGUMENTS}
		status --porcelain -- ${lookup}
	)

	if (status STREQUAL "")
		set(${OUTPUT_VAR} FALSE PARENT_SCOPE)
	else()
		set(${OUTPUT_VAR} TRUE PARENT_SCOPE)
	endif()
endfunction()

function(yokai_git_get_date OUTPUT_VAR)
	# Git prints the current commit date.
	yokai_git_run(
		git_date
		${ARG_UNPARSED_ARGUMENTS}
		log --date=format:%Y-%m-%dT%H:%M:%S --format=%cd -1
	)

	string(REPLACE "T" " " git_date "${git_date}")

	set(${OUTPUT_VAR} "${git_date}" PARENT_SCOPE)
endfunction()

function(yokai_git_get_closest_tag OUTPUT_VAR)
	# Git prints the most recent tag, or a commit hash when there is no tag at all.
	yokai_git_run(
		git_closest_tag
		${ARG_UNPARSED_ARGUMENTS}
		describe --always --tags --abbrev=0 --match "v[0-9]*"
	)

	set(${OUTPUT_VAR} "${git_closest_tag}" PARENT_SCOPE)
endfunction()

function(yokai_git_describe_tag OUTPUT_VAR)
	# Git prints a version string that is equal to the most recent tag
	# when the most recent tag is on the current commit.
	yokai_git_run(
		git_describe_tag
		${ARG_UNPARSED_ARGUMENTS}
		describe --tags --match "v[0-9]*"
	)

	set(${OUTPUT_VAR} "${git_describe_tag}" PARENT_SCOPE)
endfunction()

function(yokai_git_get_root OUTPUT_VAR)
	yokai_git_run(
		git_root
		rev-parse --show-toplevel
	)

	set(${OUTPUT_VAR} "${git_root}" PARENT_SCOPE)
endfunction()

function(yokai_git_list_submodules OUTPUT_VAR)
	yokai_git_get_root(git_root)

	yokai_git_run(
		submodule_list
		config
		--file "${git_root}/.gitmodules"
		--get-regexp "^submodule\\..*\\.path$"
		ALLOW_FAILURE
	)

	if (result EQUAL 0)
		string(REGEX REPLACE "submodule\\.[^ ]*\\.path " "" submodule_list "${submodule_list}")
		string(REPLACE "\n" ";" submodule_list "${submodule_list}")
	else()
		set(submodule_list)
	endif()

	set(${OUTPUT_VAR} "${submodule_list}" PARENT_SCOPE)
endfunction()

function(yokai_init_missing_submodules)
	if (NOT GIT_FOUND)
		message(STATUS "Git unavailable: not looking for missing submodules")
		return()
	endif()

	yokai_git_get_root(git_root)
	yokai_git_list_submodules(submodule_list)

	foreach(submodule IN LISTS submodule_list)
		if (submodule IN_LIST YOKAI_GIT_EXCLUDE_SUBMODULES)
			continue()
		endif()

		set(submodule_path "${git_root}/${submodule}")

		if (EXISTS "${submodule_path}")
			message(STATUS "Found Git submodule: ${submodule_path}")
		else()
			message(STATUS "Fetching Git submodule: ${submodule_path}")
			execute_process(
				COMMAND
					"${GIT_EXECUTABLE}" submodule update --init --recursive -- "${submodule}"
				WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
		endif()
	endforeach()
endfunction()
