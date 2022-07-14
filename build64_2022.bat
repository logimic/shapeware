set project=shapeware

rem //expected build dir structure
set buildexp=build\\VS17_2022_x64

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
cmake -G "Visual Studio 17 2022" -DBUILD_ZEROMQ:BOOL=true -DBUILD_TESTING:BOOL=true -DWS_WITHOUT_TLS:BOOL=true BUILD_ZEROMQ:BOOL=true -DCMAKE_TOOLCHAIN_FILE=%vcpkg% -Dshape_DIR:PATH=%shape% %currentdir%
popd

rem //build from generated build environment
cmake --build %builddir% --config Debug --target install
cmake --build %builddir% --config Release --target install
