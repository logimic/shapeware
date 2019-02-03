# shapeware
shape plug-ins

## Prerequisites

- Git
- Cmake
- shape https://github.com/logimic/shape

### Windows

Install vcpkg to e.g: **c:\devel\vcpkg** https://github.com/Microsoft/vcpkg

Install via vcpkg:
```
C:\devel\vcpkg>vcpkg install cpprestsdk:x86-windows
C:\devel\vcpkg>vcpkg install curl:x86-windows
...
C:\devel\vcpkg>vcpkg install cpprestsdk:x64-windows
C:\devel\vcpkg>vcpkg install curl:x64-windows
...
```
It installs all other dependecies like BOOST, SSL, ZLIB, ... After succesfull instalation you would have:
```
C:\devel\vcpkg>vcpkg list cpprestsdk
cpprestsdk:x64-windows                             2.10.2           C++11 JSON,
REST, and OAuth library The C++ REST...
cpprestsdk:x86-windows                             2.10.2           C++11 JSON,
REST, and OAuth library The C++ REST...
```
### Linux

Install libcurl:

$ sudo apt-get install libcurl4-openssl-dev

## Build

```bash
$ git clone --recursive https://github.com/logimic/shapeware
$ cd shapeware
```
then for your platform
```bash
$ python3 build.py -G "Unix Makefiles"  #for Raspberry Pi
$ build32.bat                           #for Win x86
$ build64.bat                           #for Win x64
$ ./buildMake.sh                        #for Linux
$ ./buildEclipse.sh                     #for Linux Eclipse IDE
```
