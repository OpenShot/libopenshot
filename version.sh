#!/bin/sh
grep 'set.*(.*PROJECT_VERSION_FULL' CMakeLists.txt\
 |sed -e 's#set(PROJECT_VERSION_FULL.*"\(.*\)\")#\1#;q'

