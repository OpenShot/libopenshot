#!/bin/sh

ARGS=$(getopt --long 'package,project,soname,all' -n "$0" == "$@")

if [ $? -ne 0 ]; then
        echo 'Terminating...' >&2
        exit 1
fi

eval set -- "$ARGS"
unset ARGS

while true; do
    case "$1" in
        '--p'*)
            output_project_version=1
            shift
            continue
        ;;
        '--s'*)
            output_soname=1
            shift
            continue
        ;;
        '--a'*)
            output_project_version=1
            output_soname=1
            shift
            continue
        ;;
        '--')
            shift
            break
        ;;
        *)
            echo 'Error parsing command line!' >&2
            exit 1
        ;;
    esac
done

if [ "x${output_project_version}" = "x1" ]; then
    project_version=$(\
        grep 'set.*(.*PROJECT_VERSION_FULL' CMakeLists.txt\
        |sed -e 's#set(PROJECT_VERSION_FULL\s*\"*\([^\")]*\)\"*\s*)#\1#;q'\
        )
    echo "PROJECT_VERSION=\"${project_version}\""
fi
if [ "x${output_soname}" = "x1" ]; then
    project_soname=$(\
        grep 'set.*(.*PROJECT_SO_VERSION' CMakeLists.txt\
        |sed -e 's#set(PROJECT_SO_VERSION\s*\"*\([^\")]*\)\"*\s*)#\1#;q'\
        )
    echo "PROJECT_SO=\"${project_soname}\""
fi

