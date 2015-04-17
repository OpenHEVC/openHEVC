
#--------------------------------------------------------------------------------
# Copyright (c) 2012-2013, Lars Baehren <lbaehren@gmail.com>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:
#
#  * Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#--------------------------------------------------------------------------------

# - Check for the presence of YASM
#
# The following variables are set when YASM is found:
#  YASM_FOUND      = Set to true, if all components of YASM have been found.
#  YASM_INCLUDES   = Include path for the header files of YASM
#  YASM_LIBRARIES  = Link these to use YASM

if (NOT YASM_FOUND)

  if (NOT YASM_ROOT_DIR)
    set (YASM_ROOT_DIR ${CMAKE_INSTALL_PREFIX})
  endif (NOT YASM_ROOT_DIR)

  ##_____________________________________________________________________________
  ## Check for the header files

  find_path (YASM_INCLUDES
    NAMES libyasm.h libyasm/valparam.h
    HINTS ${YASM_ROOT_DIR} ${CMAKE_INSTALL_PREFIX}
    PATH_SUFFIXES include include/libyasm include/yasm
    )

  ##_____________________________________________________________________________
  ## Check for the library

  find_library (YASM_LIBRARIES yasm
    HINTS ${YASM_ROOT_DIR} ${CMAKE_INSTALL_PREFIX}
    PATH_SUFFIXES lib
    )

  ##_____________________________________________________________________________
  ## Check for the executable

  find_program (YASM_EXECUTABLE yasm
    HINTS ${YASM_ROOT_DIR} ${CMAKE_INSTALL_PREFIX}
    PATH_SUFFIXES bin
    )

  ##_____________________________________________________________________________
  ## Actions taken when all components have been found

  INCLUDE(FindPackageHandleStandardArgs)
# FIND_PACKAGE_HANDLE_STANDARD_ARGS(YASM DEFAULT_MSG YASM_LIBRARIES YASM_INCLUDES)
# FIND_PACKAGE_HANDLE_STANDARD_ARGS(YASM DEFAULT_MSG YASM_INCLUDES)
  FIND_PACKAGE_HANDLE_STANDARD_ARGS(YASM DEFAULT_MSG YASM_EXECUTABLE)
  if (YASM_FOUND)
    if (NOT YASM_FIND_QUIETLY)
      message (STATUS "Found components for YASM")
      message (STATUS "YASM_ROOT_DIR  = ${YASM_ROOT_DIR}")
      message (STATUS "YASM_INCLUDES  = ${YASM_INCLUDES}")
      message (STATUS "YASM_LIBRARIES = ${YASM_LIBRARIES}")
    endif (NOT YASM_FIND_QUIETLY)
  else (YASM_FOUND)
    if (YASM_FIND_REQUIRED)
      message (FATAL_ERROR "Could not find YASM!")
    endif (YASM_FIND_REQUIRED)
  endif (YASM_FOUND)

  ##_____________________________________________________________________________
  ## Mark advanced variables

  mark_as_advanced (
    YASM_ROOT_DIR
    YASM_INCLUDES
    YASM_LIBRARIES
    )

endif (NOT YASM_FOUND)