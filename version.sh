#!/bin/sh

# A simple script to extract metadata from CMakeLists.txt for use in
# other contexts
#
# Author: FeRD (Frank Dana) <ferdnyc@gmail.com>
#
# See git commit history for change timeline

print_help() {
    cat <<__EOM__
Usage: $0 [--help] [--export|--powershell] [SCOPE ...]
Extract the requested metadata strings from the CMakeLists.txt project
definition and display them in Bash shell 'eval'-able form.

Scope Options:
--package    Display the project version as a PROJECT_VERSION=
--project    variable definition (default)
--version

--name       Display the project name as a PROJECT_NAME=
             variable definition

--soname     Display the library SONAME value as a PROJECT_SO=
             variable definition

--all        Display definitions for all available variables,
             one per line.

Output customization:
--prefix <arg>  Prefix each variable with '<arg>', instead of 'PROJECT'.
--export        Include an 'export' command for each variable set.
--powershell    Output command to set an environment variable from
                within PowerShell, instead of bash. (See Note, below.)

Other flags:
--help       Display this reference and exit.

PowerShell Note:
The output from this command can be executed with the Invoke-Expression
command, e.g. to set the PROJECT_VERSION variable:

Invoke-Expression (& 'C:\Program Files\git\bin\sh.exe' -c './version.sh --powershell')

This only seems to work for one command at a time, so don't use it
with '--all'.
__EOM__
}

# Utility function to create the final script output
bash_output() {
    outvar=$1; shift
    invar=$1; shift
    echo -n "${outvar}=\"${invar}\";"
    if [ "x${export_variables}" = "x1" ]; then
       echo " export ${outvar};"
    else
       echo
    fi
}

powershell_start() {
  echo "{"
}
powershell_end() {
  echo "}"
}

powershell_output() {
  outvar=$1; shift
  invar=$1; shift;
  echo "Set-Item -Path Env:${outvar} -Value \"${invar}\""
}

PATH_TO_CMAKELISTS=$(dirname $(realpath "$0"))/CMakeLists.txt

ARGS=$(getopt --long 'name,p,pa,pro,package,project,version,soname,all,pre:,prefix:,export,powershell,help' -n "$0" == "$@")

if [ $? -ne 0 ]; then
        echo 'Terminating...' >&2
        exit 1
fi

eval set -- "$ARGS"
unset ARGS

while [[ $# -gt 0 ]]; do
arg="$1"
case $arg in
    '--n'*)
        output_project_name=1
        shift
        continue
    ;;
    '--pa'*|'--pro'*|'--v'*)
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
    '--pre'*)
        custom_prefix="$2"
        shift
        shift
        continue
    ;;
    '--e'*)
        export_variables=1
        shift
        continue
        ;;
    '--po'*)
        powershell_format=1
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
PFX="PROJECT"
if [ "x${custom_prefix}" != "x" ]; then
    PFX="${custom_prefix}"
fi

# Output requested variables
if [ "x${output_project_version}" = "x1" ]; then
  project_version=$(\
    grep 'set.*(.*PROJECT_VERSION_FULL' "${PATH_TO_CMAKELISTS}"\
    |sed -e 's#set(PROJECT_VERSION_FULL\s*\"*\([^\")]*\)\"*\s*)#\1#;q'\
  )
  if [ "x${powershell_format}" = "x1" ]; then
    powershell_output "${PFX}_VERSION" "${project_version}"
  else
    bash_output "${PFX}_VERSION" "${project_version}"
  fi
fi

if [ "x${output_project_name}" = "x1" ]; then
  project_name=$(\
    grep '^\s*project\s*(' "${PATH_TO_CMAKELISTS}"\
    |sed -e 's#project(\(\S*\)[^)]*)#\1#;q'\
  )
  if [ "x${powershell_format}" = "x1" ]; then
    powershell_output "${PFX}_NAME" "${project_name}"
  else
    bash_output "${PFX}_NAME" "${project_name}"
  fi
fi

if [ "x${output_soname}" = "x1" ]; then
  project_soname=$(\
    grep 'set.*(.*PROJECT_SO_VERSION' "${PATH_TO_CMAKELISTS}"\
    |sed -e 's#set(PROJECT_SO_VERSION\s*\"*\([^\")]*\)\"*\s*)#\1#;q'\
  )
  if [ "x${powershell_format}" = "x1" ]; then
    powershell_output "${PFX}_SO" "${project_soname}"
  else
    bash_output "${PFX}_SO" "${project_soname}"
  fi
fi

# Stick a fork in it
exit 0
