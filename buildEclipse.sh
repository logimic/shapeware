#!/bin/bash
# Script for building Shape on Linux machine

project=shape

#expected build dir structure
buildexp=build/Eclipse_CDT4-Unix_Makefiles

currentdir=$PWD
builddir=./${buildexp}

mkdir -p ${builddir}

#get path to Shape libs
shape=../shape/${buildexp}
pushd ${shape}
shape=$PWD
popd

#launch cmake to generate build environment
pushd ${builddir}
cmake -G "Eclipse CDT4 - Unix Makefiles" -DBUILD_TESTING:BOOL=true -DCMAKE_BUILD_TYPE=Debug -DLWS_STATIC_PIC:BOOL=true -Dshape_DIR:PATH=${shape} ${currentdir} 
popd

#build from generated build environment
cmake --build ${builddir}
