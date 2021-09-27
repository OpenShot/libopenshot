#!/bin/sh
# Run otool to display some useful data about the structure of our
# compiled macOS binary components

cd "${INSTALL_PATH}"
for lib in libopenshot-audio.dylib libopenshot.dylib libopenshot_protobuf.dylib; do
    echo
    echo "== ${lib} =============================="
    otool -D "lib/${lib}"
    otool -L "lib/${lib}"
    echo
    echo "${lib} load commands:"
    otool -l "lib/${lib}"
done
echo
echo "== openshot-player ===================================="
otool -L "bin/openshot-player"
echo
echo "openshot-player load commands:"
otool -l "bin/openshot-player"

exit 0
