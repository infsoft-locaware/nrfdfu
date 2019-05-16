# nrfdfu - Nordic Serial DFU #

This is a C Implementation of Nordics Serial DFU protocol, targeted for embedded systems where running Python-based `nrfutil` is too heavy-weight.


## Dependencies ##

  * [JSON-C](https://github.com/json-c/json-c)
  * [ZLib](https://zlib.net/)
  * [LibZIP](https://libzip.org/)


## Compile on Ubuntu ##

1.) Install dependencies:

    sudo apt install libjson-c-dev zlib1g-dev

2.) Download and build libzip from https://libzip.org/

    mkdir build
    cd build/
    cmake ..
    make
    sudo make install

3.) Build nrfdfu:

    mkdir build
    cd build
    cmake ..
    make


## Usage ##
```
Usage: nrfserdfu [options] DFUPKG.zip
Nordic NRF DFU Upgrade with DFUPKG.zip
Options:
  -h, --help            Show help
  -v, --verbose=<level> Log level 1 or 2 (-vv)
  -p, --port <tty>      Serial port (/dev/ttyUSB0)
  -c, --cmd <text>      Command to enter DFU mode
 ```
 
Example:

    ./build/nrfdfu -p /dev/ttyPL2303 -c dfu ~/dfu-update.zip

This is like typing "dfu" on the serial console (this is optional if the device is already in DFU mode) and then the Nordic DFU Upgrade is started over the same serial port. Use -v or -vv for a more verbose output.
