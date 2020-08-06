#!/bin/bash
# Script for building shape on Linux machine

project=shapeware

#expected build dir structure
buildexp=build/Unix_Makefiles

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
cmake -G "Unix Makefiles" -DBUILD_TESTING:BOOL=false -DCMAKE_BUILD_TYPE=Release -Dshape_DIR:PATH=${shape} ${currentdir} 
popd

#build from generated build environment
cmake --build ${builddir} --config Release --target install

