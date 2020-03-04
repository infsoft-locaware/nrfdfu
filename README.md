# nrfdfu - Nordic Serial DFU #

This is a C Implementation of Nordics DFU protocol over Serial and BLE, targeted for embedded systems where running Python-based `nrfutil` is too heavy-weight.


## Dependencies ##

  * [JSON-C](https://github.com/json-c/json-c)
  * [ZLib](https://zlib.net/)
  * [LibZIP](https://libzip.org/)

For BLE:

  * [blzlib](https://github.com/infsoft-locaware/blzlib)
  * [BlueZ](http://www.bluez.org/) version 5.49 and higher
  * [libsystemd](https://www.freedesktop.org/wiki/Software/systemd/) SD-Bus library from systemd


## Compile on Ubuntu ##

1.) Install dependencies:

    sudo apt install libjson-c-dev zlib1g-dev libsystemd-dev

2.) Download and build libzip from https://libzip.org/

    mkdir build
    cd build/
    cmake ..
    make
    sudo make install

3.) Download and build blzlib from https://github.com/infsoft-locaware/blzlib

    mkdir build
    cd build/
    cmake ..
    make
    sudo make install

4.) Build nrfdfu:

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
  -t, --timeout <num>   Timeout after <num> tries (60)
 ```
 
Example:

    ./build/nrfdfu -p /dev/ttyPL2303 -c dfu ~/dfu-update.zip

This is like typing "dfu" on the serial console (this is optional if the device is already in DFU mode) and then the Nordic DFU Upgrade is started over the same serial port. Use -v or -vv for a more verbose output.


## License ##

This program is licensed under the GPLv3.

Parts of it, the Nordic DFU definitions (nrf_dfu_req_handler.h) and SLIP (slip.c) are covered by the `5-Clause Nordic License`:

```
Copyright (c) 2015 - 2019, Nordic Semiconductor ASA

All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form, except as embedded into a Nordic
   Semiconductor ASA integrated circuit in a product or a software update for
   such product, must reproduce the above copyright notice, this list of
   conditions and the following disclaimer in the documentation and/or other
   materials provided with the distribution.

3. Neither the name of Nordic Semiconductor ASA nor the names of its
   contributors may be used to endorse or promote products derived from this
   software without specific prior written permission.

4. This software, with or without modification, must only be used with a
   Nordic Semiconductor ASA integrated circuit.

5. Any software provided in binary form under this license must not be reverse
   engineered, decompiled, modified and/or disassembled.

THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```
