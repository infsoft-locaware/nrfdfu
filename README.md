== Compile on Ubuntu ==

1.) Install dependencies:

    sudo apt install libjson-c-dev zlib1g-dev

2.) Install libzip from https://libzip.org/

3.) Build nrfdfu:

    mkdir build
    cd build
    cmake ..
    make


== Run ==

Example:

    ./build/nrfdfu -p /dev/ttyPL2303 ~/ble-radar-0.0-35-gc6cff41-DFU.zip
