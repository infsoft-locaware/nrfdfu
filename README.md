# nrfdfu - Nordic Serial DFU #

This is a C Implementation of Nordics Serial DFU protocol, targeted for embedded systems where running Python-based `nrfutil` is too heavy-weight.

## Dependecies ##

  * JSON-C: https://github.com/json-c/json-c
  * ZLib
  * LibZIP: https://libzip.org/
  
## Compile on Ubuntu ##

1.) Install dependencies:

    sudo apt install libjson-c-dev zlib1g-dev

2.) Install libzip from https://libzip.org/

3.) Build nrfdfu:

    mkdir build
    cd build
    cmake ..
    make


## Usage ##

Example:

    ./build/nrfdfu -p /dev/ttyPL2303 ~/ble-radar-0.0-35-gc6cff41-DFU.zip
