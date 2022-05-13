# gr-flowgraph

gr-flowgraph is used to parse and run a [GNU Radio](https://wiki.gnuradio.org) `flowgraph.grc` file which uses blocks from [gr-digitizer](https://github.com/fair-acc/gr-digitizers) during runtime in C++.

gr-flowgraph as well supports many native [GNU Radio](https://wiki.gnuradio.org).

Why not using native python code generation via `gnuradio-companion` to create a C++ application ?
Because for some use-cases, that is not sufiicient:

- If the host application shall not be restartet on re-loading the flowgraph
- If the host application has callbacks inside some of the blocks which should be served
- If python is not available on the runtime environment

# Building

```bash
# Point to installation directory of a specific gr-digitizer version
export DIGITIZERS_DIR=/path/to/gr-digitizer/<Version>

# Create build folder
mkdir gr-flowgraph/build
cd gr-flowgraph/build

# configure, add optional flags and optionally define installation directory for gr-flowgraph
# For debug output as well add -DDEBUG_ENABLED=1
cmake .. -DENABLE_GR_LOG=1 -DENABLE_STATIC_LIBS=ON -DCMAKE_INSTALL_PREFIX=/path/to/gr-digitizer/<Version>

# Compile and link
make -j8

# Optionally run unit-tests and install the result
./lib/test_flowgraph_test.sh
make install
```
