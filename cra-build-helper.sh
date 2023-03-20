#!/bin/bash

# This script exists to emulate some of the arguments to webpack (e.g: --env) so that yarn bundle works the same for
# CRA-based projects as for webpack-based ones.

CMD="$1"
shift

while [ "$#" != "0" ] ; do
    ARG="$1"
    shift

    case "${ARG}" in
        --env)
            export "REACT_APP_$1" # CRA uses REACT_APP_ to identify variables from the environment to import.
            shift
        ;;
        *)
            echo "Unknown argument "${ARG} 1>&2
            exit 1
        ;;
    esac
done

yarn react-app-rewired "${CMD}"
