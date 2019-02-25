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

```
$ sudo apt-get install libcurl4-openssl-dev
```

## Build

```
$ git clone --recursive https://github.com/logimic/shapeware
$ cd shapeware
```

Then run Python building sript:

```
$ python3 build.py                        #for Linux, Raspberry Pi
$ py build.py                             #for Win
```

Building parameters are specified in **bcfgWin.json** and **bcfgLin.json** files consumed by the building script.
