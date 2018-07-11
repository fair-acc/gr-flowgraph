#!/bin/sh

FOLDER_TO_TAR=usr
INSTALL_DIR_LIB=${FOLDER_TO_TAR}/lib64
INSTALL_DIR_BIN=${FOLDER_TO_TAR}/bin

SCRIPT=$(readlink -f "$0")
SCRIPTPATH=$(dirname "$SCRIPT")     # path where this script is located in

TARBALL_NAME=Flowgraph.tar

mkdir -p ${INSTALL_DIR_LIB}
mkdir -p ${INSTALL_DIR_BIN}

cp ${SCRIPTPATH}/build/lib/libgnuradio-flowgraph-*.so.0.0.0 ${INSTALL_DIR_LIB}

cp ${SCRIPTPATH}/build/lib/test-flowgraph ${INSTALL_DIR_BIN}
cp ${SCRIPTPATH}/build/lib/test_flowgraph_test.sh ${INSTALL_DIR_BIN}

tar cfv ${TARBALL_NAME} ${FOLDER_TO_TAR}
rm -rf ${FOLDER_TO_TAR}
gzip ${TARBALL_NAME}

cp ${TARBALL_NAME}.gz /common/export/fesa/arch/x86_64
