#!/bin/sh

# A simple script to extract metadata from CMakeLists.txt for use in
# other contexts
#
# Author: FeRD (Frank Dana) <ferdnyc@gmail.com>
#
# See git commit history for change timeline

print_help() {
    cat <<__EOM__
Usage: $0 [--export] [OPTION ...]
Extract the requested metadata strings from the CMakeLists.txt project
definition and display them in Bash shell 'eval'-able form.

Options:
--package    Display the project version as a PROJECT_VERSION=
--project    variable definition (default)
--version

--name       Display the project name as a PROJECT_NAME=
             variable definition

--soname     Display the library SONAME value as a PROJECT_SO=
             variable definition

--all        Display definitions for all available variables,
             one per line.

--export     Include an 'export' command after each variable.
__EOM__
}

# Utility function to create the final script output
do_output() {
    outvar=$1
    shift
    invar=$1
    shift
    echo -n "${outvar}=\"${invar}\";"
    if [ "x${export_variables}" = "x1" ]; then
       echo " export ${outvar};"
    else
       echo
    fi
}

PATH_TO_CMAKELISTS="$(dirname $(realpath \"$0\"))/CMakeLists.txt"

ARGS=$(getopt --long 'name,p,pa,pr,package,project,version,soname,all,export,help' -n "$0" == "$@")

if [ $? -ne 0 ]; then
        echo 'Terminating...' >&2
        exit 1
fi

eval set -- "$ARGS"
unset ARGS

while true; do
    case "$1" in
        '--n'*)
            output_project_name=1
            shift
            continue
        ;;
        '--p'*|'--v'*)
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
            output_project_name=1
            output_soname=1
            shift
            continue
        ;;
        '--e'*)
            export_variables=1
            shift
            continue
        ;;
        '--h'*)
            print_help
            exit 0
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

# Default mode
if [ "x${output_project_version}" != "x1"\
     -a "x${output_soname}" != "x1"\
     -a "x${output_project_name}" != "x1" ]; then
    output_project_version=1
fi

if [ "x${output_project_version}" = "x1" ]; then
    project_version=$(\
        grep 'set.*(.*PROJECT_VERSION_FULL' "${PATH_TO_CMAKELISTS}"\
        |sed -e 's#set(PROJECT_VERSION_FULL\s*\"*\([^\")]*\)\"*\s*)#\1#;q'\
        )
    do_output "PROJECT_VERSION" "${project_version}"
fi

if [ "x${output_project_name}" = "x1" ]; then
    project_name=$(\
        grep '^\s*project\s*(' "${PATH_TO_CMAKELISTS}"\
        |sed -e 's#project(\(\S*\)[^)]*)#\1#;q'\
        )
    do_output "PROJECT_NAME" "${project_name}"
fi

if [ "x${output_soname}" = "x1" ]; then
    project_soname=$(\
        grep 'set.*(.*PROJECT_SO_VERSION' "${PATH_TO_CMAKELISTS}"\
        |sed -e 's#set(PROJECT_SO_VERSION\s*\"*\([^\")]*\)\"*\s*)#\1#;q'\
        )
    do_output "PROJECT_SO" "${project_soname}"
fi

