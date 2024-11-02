## Setup instructions

### Install gRPC

Instructions to install gRPC on MacOS are given below:

For installation instructions on Linux or Windows, please follow the steps here: https://grpc.io/docs/languages/cpp/quickstart/



1. Choose a directory to hold locally installed packages
```
$ export MY_INSTALL_DIR=$HOME/.local
```

2. Ensure that the directory exists
```
$ mkdir -p $MY_INSTALL_DIR
```

3. Add the local `bin` folder to path variable
```
$ export PATH="$MY_INSTALL_DIR/bin:$PATH"
```

4. Install cmake
```
$ brew install cmake
```

5. Install other required tools
```
$ brew install autoconf automake libtool pkg-config
```

6. Clone the gRPC repo
```
$ git clone --recurse-submodules -b v1.66.0 --depth 1 --shallow-submodules https://github.com/grpc/grpc
```

7. Build and install gRPC and Protocol Buffers
```
$ cd grpc
$ mkdir -p cmake/build
$ pushd cmake/build
$ cmake -DgRPC_INSTALL=ON \
      -DgRPC_BUILD_TESTS=OFF \
      -DCMAKE_INSTALL_PREFIX=$MY_INSTALL_DIR \
      ../..
$ make -j 4
$ make install
$ popd
```

### Clone the project repository

1. Clone the project repository which has the source code for running paxos.
```
$ git clone git@github.com:F24-CSE535/apaxos-avjotsingh.git
```


### Build the project

1. Build the project. This should not take too long
```
$ cd apaxos-avjotsingh
$ mkdir build
$ cd ./build
$ cmake -DCMAKE_PREFIX_PATH=$MY_INSTALL_DIR ..
$ cmake --build .
```