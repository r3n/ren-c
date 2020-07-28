#!/bin/bash

TOP_DIR=$(pwd)
cd build

# Tests ./r3 with $RUNNER
# Loaded into .travis.yml ("script" step, 2nd phase)

# Run once but don't pipe output, in case it prints out useful crash msg
# that we want to see in the Travis log (especially helpful for failures
# only happening in the Travis builds that aren't reproducing locally)
# Save the exit code ($?) so we can return it to Travis as last step
#
# !!! This is a very minimal sanity check to ensure the built R3 does
# *something*, and it can't be used on cross-compilations (e.g. a Windows
# executable won't run on Linux).  Running the full test suite would be a
# bit much, and developers are expected to have already done that.  But
# doing an HTTPS read exercises a fair amount of code.
#

if [[ $OS_ID = 0.4.40 || $OS_ID = 0.4.4 || $OS_ID = 0.2.40 ]]; then
    $RUNNER ./r3 --do "print {Testing...} quit either find to-text read https://example.com {<h1>Example Domain</h1>} [0] [1]"
    R3_EXIT_STATUS=$?
else
    R3_EXIT_STATUS=0
fi
echo "$R3_EXIT_STATUS"

# Run basic testing on FFI if it is included and this container can run it

if [[ -z $WITH_FFI || $WITH_FFI != no ]]; then
  if [[ $OS_ID = 0.4.40 || $OS_ID = 0.4.4 ]]; then
    $RUNNER ./r3 ../tests/misc/qsort_r.r
    R3_EXIT_STATUS=$?
  else
    R3_EXIT_STATUS=0
  fi
else
    R3_EXIT_STATUS=0
fi
echo "$R3_EXIT_STATUS"

# If the build is TCC-based and the host can run the produced executable,
# do a simple test that does a TCC fibonnaci calculation.

# The TCC extension currently looks at the environment variable
# LIBREBOL_INCLUDE_DIR to find "rebol.h".  Ultimately, this should be
# shipped with the TCC extension.  (It may be desirable to embed it,
# but it also may be desirable to have a copy that a file can find
# via `#include "rebol.h"`)
#
# (export is so that the child process can see the environment variable)
#
echo "Note: CONFIG_TCCDIR is ${CONFIG_TCCDIR}"
export LIBREBOL_INCLUDE_DIR="${TOP_DIR}/build/prep/include"
if [[ (! -z $TCC) && ($TCC != no) ]]; then
  if [[ $OS_ID = 0.4.40 ]]; then
    $RUNNER ./r3 ../extensions/tcc/tests/fib.r
    R3_EXIT_STATUS=$?
  elif [[ $OS_ID = 0.4.4 ]]; then
    #
    # Most 32-bit builds are intending to run on 32-bit systems, hence
    # the running TCC should link to libs in `/usr/lib`.  But this
    # container is 64-bit (Travis does not support 32-bit containers,
    # nor do they seem to intend to).  The tcc extension heeds this
    # environment variable and uses hardcoded output of `gcc -v m32`
    # for library directories to override `/usr/lib` with.
    #
    export REBOL_TCC_EXTENSION_32BIT_ON_64BIT=1
    $RUNNER ./r3 ../extensions/tcc/tests/fib.r
    R3_EXIT_STATUS=$?
  fi
else
    R3_EXIT_STATUS=0
fi
echo $R3_EXIT_STATUS

if [[ $R3_EXIT_STATUS != 0 ]]; then
  exit $R3_EXIT_STATUS
fi

# vim: et sw=2:
