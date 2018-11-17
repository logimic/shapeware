set project=shapeware

rem //expected build dir structure
set buildexp=build\\VS15_2017

set currentdir=%cd%
set builddir=.\\%buildexp%

mkdir %builddir%

rem //get path to to Shape libs
set shape=..\\shape\\%buildexp%
pushd %shape%
set shape=%cd%
popd

set vcpkg=c:\\devel\\vcpkg\\scripts\\buildsystems\\vcpkg.cmake

rem //launch cmake to generate build environment
pushd %builddir%
cmake -G "Visual Studio 15 2017" -DBUILD_TESTING:BOOL=true -DCMAKE_TOOLCHAIN_FILE=%vcpkg% -DLWS_WITH_SSL:BOOL=false -Dshape_DIR:PATH=%shape% %currentdir%
popd

rem //build from generated build environment
cmake --build %builddir% --config Debug --target install
cmake --build %builddir% --config Release --target install

