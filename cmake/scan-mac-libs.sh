#!/bin/sh
# Run otool to display some useful data about the structure of our
# compiled macOS binary components

function indent() {
  w=$1
  ind=$(printf "%${w}s" "")
  sed -e "s/^/${ind}/;"
}

cd "${INSTALL_PATH}"
for lib in lib/libopenshot-audio.dylib lib/libopenshot.dylib lib/libopenshot_protobuf.dylib python/_openshot.so ; do
    libname=`basename ${lib}`
    echo
    echo "== ${libname} =============================="
    echo -n "${libname} name: "
    otool -D "${lib}"
    echo "${libname} libraries:"
    otool -L "${lib}" | indent 2
    echo
    echo "${libname} load commands:"
    otool -l "${lib}" | indent 2
done
echo
echo "== openshot-player ===================================="
echo "openshot-player libraries:"
otool -L "bin/openshot-player" | indent 2
echo
echo "openshot-player load commands:"
otool -l "bin/openshot-player" | indent 2

exit 0
