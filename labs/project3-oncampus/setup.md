# Setup 
4th March 2020

This document describes how to install dependencies for Project 3. You will be able to use the system you set up for Project 4 as well.

We will be primarily relying on `vcpkg` for all our dependencies.

## Dependencies
1. [`cmake`](https://cmake.org/download)                                                - For building C/C++ applications 3.10+
2. [`vcpkg`](https://github.com/microsoft/vcpkg)                                        - Package Manager for C/C++ libraries
3. [`protobuf`](https://github.com/protocolbuffers/protobuf/blob/master/src/README.md)  - Google Protocol Buffers
4. [`gRPC`](https://github.com/grpc/grpc/blob/master/src/cpp/README.md)                 - Google's RPC framework

## Steps

The steps below are for an Ubuntu 18.04 LTS system. 
You may need to modify these steps if you plan on using a different Linux distribution.

### Install/Clone Software Pacakges

First install/clone the software packages you will need to complete the setup:

`sudo apt-get install -y unzip build-essential`

`git clone https://github.com/microsoft/vcpkg`

`cd vcpkg/`

### Setup vcpkg and Install gRPC

Next issue the following commands which will install cmake, ninja build system, and configure the vcpkg binary. If you intend to use a version of cmake which is already installed on your system, you will also need to specify the `-useSystemBinaries` option.

`./bootstrap-vcpkg.sh -disableMetrics` 

`./vcpkg search grpc` -> To search for the grpc package

`./vcpkg install grpc` -> To install gRPC via vcpkg (This may take some time depending on your system. You should plan for at least 20-30 minutes of installation time on a system with moderate resources).

### Add cmake to PATH

Next issue the following command to add the cmake utility installed by vcpkg to your PATH. (Note: Skip this step if you are using a different version of cmake that was installed via some other method):

`export PATH=$PATH:$HOME/vcpkg/downloads/tools/cmake-3.14.0-linux/cmake-3.14.0-Linux-x86_64/bin`

### Build Project Files

    $ cd project3/
    $ rm -rf build; mkdir build
    $ cd build
    $ cmake -DVCPKG_TARGET_TRIPLET=x64-linux \
      -DCMAKE_TOOLCHAIN_FILE="$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake" ..
    $ make

This will generate all the relevant binaries, namely `store`, `run_vendors` and `run_tests`
