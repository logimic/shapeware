set project=shapeware

rem //expected build dir structure
set buildexp=build\\VS14_2015_x64

set currentdir=%cd%
set builddir=.\\%buildexp%
set libsdir=..\\

mkdir %builddir%

rem //get path to to Shape libs
set shape=..\\shape\\%buildexp%
pushd %shape%
set shape=%cd%
popd

set vcpkg=c:\\devel\\vcpkg\\scripts\\buildsystems\\vcpkg.cmake

rem //launch cmake to generate build environment
pushd %builddir%
cmake -G "Visual Studio 14 2015 Win64" -DBUILD_EXAMPLES:BOOL=true -DBUILD_TESTING:BOOL=true -DCMAKE_TOOLCHAIN_FILE=%vcpkg% -DLWS_WITH_SSL:BOOL=false -Dshape_DIR:PATH=%shape% %currentdir%
popd

rem //build from generated build environment
cmake --build %builddir%
