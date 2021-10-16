#!/bin/sh

# © OpenShot Studios, LLC
#
# SPDX-License-Identifier: LGPL-3.0-or-later

grep 'set.*(.*PROJECT_VERSION_FULL' CMakeLists.txt\
 |sed -e 's#set(PROJECT_VERSION_FULL.*"\(.*\)\")#\1#;q'

