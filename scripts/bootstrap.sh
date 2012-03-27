#!/bin/bash

IN_BOOTSTRAP=1
DEPS_DIR=deps
mkdir -p $DEPS_DIR

# source scripts/cmakecheck.sh
# echo "CMAKE=$cmakecmd" >> configure.deps


source scripts/boostcheck.sh
# we add a boost root only if it is the deps directory
if [ ! -z $BOOST_ROOT ] && [ $BOOST_ROOT == $PWD/$DEPS_DIR ]; then
  echo "BOOST_ROOT=$BOOST_ROOT" >> configure.deps
fi

