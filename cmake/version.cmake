# The version of Vita3K, including the commit it is built from.
# Besides being included when configuring, it is run again at each build by the vita3k_version
# target so that it follows the checked out commit, the files are only written when they change.

set(VITA3K_ORG_NAME "Vita3K")
set(VITA3K_APP_NAME "Vita3K")
set(VITA3K_APP_ID "org.vita3k.vita3k")
set(VITA3K_APP_VER_HI 0)
set(VITA3K_APP_VER_MID 2)
set(VITA3K_APP_VER_LO 1)
set(VITA3K_APP_VERSION "v${VITA3K_APP_VER_HI}.${VITA3K_APP_VER_MID}.${VITA3K_APP_VER_LO}")

get_filename_component(VITA3K_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if(NOT DEFINED VITA3K_BINARY_DIR)
	set(VITA3K_BINARY_DIR "${CMAKE_BINARY_DIR}")
endif()

if(NOT GIT_EXECUTABLE)
	find_package(Git QUIET)
endif()

set(GIT_COMMIT "")
set(GIT_COUNT "")
set(GIT_HASH "")
execute_process(
	COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
	WORKING_DIRECTORY ${VITA3K_SOURCE_DIR}
	OUTPUT_VARIABLE GIT_COMMIT
	OUTPUT_STRIP_TRAILING_WHITESPACE
	ERROR_QUIET)
if(NOT GIT_COMMIT STREQUAL "")
	execute_process(
		COMMAND ${GIT_EXECUTABLE} rev-list ${GIT_COMMIT} --count
		WORKING_DIRECTORY ${VITA3K_SOURCE_DIR}
		OUTPUT_VARIABLE GIT_COUNT
		OUTPUT_STRIP_TRAILING_WHITESPACE)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} rev-parse --short ${GIT_COMMIT}
		WORKING_DIRECTORY ${VITA3K_SOURCE_DIR}
		OUTPUT_VARIABLE GIT_HASH
		OUTPUT_STRIP_TRAILING_WHITESPACE)
	if(NOT GIT_COUNT MATCHES "^[0-9]+$")
		set(GIT_HASH "")
	endif()
endif()

# Value is different when is PR
if("$ENV{GITHUB_EVENT_NAME}" STREQUAL "pull_request")
	set(GIT_BRANCH "$ENV{GITHUB_HEAD_REF}")
	if(WIN32)
		set(CMD powershell "(Get-Content $ENV{GITHUB_EVENT_PATH} | ConvertFrom-Json).pull_request.head.repo.html_url")
	else()
		set(CMD bash -c "jq .pull_request.head.repo.html_url $ENV{GITHUB_EVENT_PATH} | tr -d \\\" ")
	endif()
else()
	execute_process(
		COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
		WORKING_DIRECTORY ${VITA3K_SOURCE_DIR}
		OUTPUT_VARIABLE GIT_BRANCH
		OUTPUT_STRIP_TRAILING_WHITESPACE)
	set(CMD ${GIT_EXECUTABLE} config --get remote.origin.url)
endif()

execute_process(COMMAND ${CMD}
	WORKING_DIRECTORY ${VITA3K_SOURCE_DIR}
	OUTPUT_VARIABLE GIT_REPO
	OUTPUT_STRIP_TRAILING_WHITESPACE)

set(VITA3K_OFFICIAL_UPDATE_BUILD 0)

if (NOT GIT_REPO STREQUAL "")
	string(FIND ${GIT_REPO} "git@github.com:" IS_GIT_SSH)
	if (IS_GIT_SSH EQUAL 0)
		string(REPLACE "git@github.com:" "" GIT_REPO ${GIT_REPO})
	else()
		string(REPLACE "https://github.com/" "" GIT_REPO ${GIT_REPO})
	endif()
	string(REPLACE "/" ";" GIT_REPO ${GIT_REPO})
	list(POP_BACK GIT_REPO)
endif()

if(GIT_HASH STREQUAL "")
	set(VITA3K_GIT_REV "Development version")
	set(GIT_COUNT 0)
elseif((GIT_REPO STREQUAL "Vita3K") AND ((GIT_BRANCH STREQUAL "master") OR (GIT_BRANCH STREQUAL "HEAD")))
	set(VITA3K_GIT_REV "${GIT_HASH}")
	set(VITA3K_OFFICIAL_UPDATE_BUILD 1)
else()
	set(VITA3K_GIT_REV "${GIT_HASH}-${GIT_REPO}/${GIT_BRANCH}")
endif()

configure_file("${VITA3K_SOURCE_DIR}/vita3k/config/src/version.cpp.in" "${VITA3K_BINARY_DIR}/vita3k/config/version.cpp" ESCAPE_QUOTES)
file(CONFIGURE OUTPUT "${VITA3K_BINARY_DIR}/vita3k/config/gitver.h"
	CONTENT "#define APP_VER_HI @VITA3K_APP_VER_HI@\n#define APP_VER_MID @VITA3K_APP_VER_MID@\n#define APP_VER_LO @VITA3K_APP_VER_LO@\n#define APP_NUMBER @GIT_COUNT@\n"
	@ONLY)
