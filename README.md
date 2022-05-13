# gr-flowgraph

gr-flowgraph is used to parse and run a [GNU Radio](https://wiki.gnuradio.org) `flowgraph.grc` file which uses blocks from [gr-digitizer](https://github.com/fair-acc/gr-digitizers) during runtime in C++.

gr-flowgraph as well supports many native [GNU Radio](https://wiki.gnuradio.org) blocks.

This project was created, because there are various use-cases for which a native python code generation via `gnuradio-companion` (creation of standalone C++ application) is not sufficient:

- There is a host application which runs/controls the flowgraph.
- The host application whould be able to start/stop/reload the flowgraph, without restarting itself.
- The host application has callbacks inside some of the blocks which should be served (E.g. see [gr-digitizer](https://github.com/fair-acc/gr-digitizers)).
- python is not available on the runtime environment.

# Building

```bash
# Installation directory of a specific gr-digitizer version
export DIGITIZERS_DIR=/path/to/gr-digitizer/<Version>

# Creation of build folder
mkdir gr-flowgraph/build
cd gr-flowgraph/build

# Configure, add optional flags and optionally define an installation directory for gr-flowgraph
# For debug output add -DDEBUG_ENABLED=1 (check the documentation of cmake for a complete list)
cmake .. -DENABLE_GR_LOG=1 -DENABLE_STATIC_LIBS=ON -DCMAKE_INSTALL_PREFIX=/path/to/gr-digitizer/<Version>

# Compile and link (-j8 will use 8 CPU cores, put whatever works for you)
make -j8

# Optionally run unit-tests and install the result
./lib/test_flowgraph_test.sh
make install
```
