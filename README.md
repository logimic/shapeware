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
C:\devel\vcpkg>vcpkg install mqtt-paho:x64-windows
C:\devel\vcpkg>vcpkg install curl:x64-windows
C:\devel\vcpkg>vcpkg install cppzmq:x64-windows
```
### Linux

Install via apt-get:

```
$ sudo apt-get install libcurl4-openssl-dev
$ sudo apt-get install libzmqpp-dev


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
