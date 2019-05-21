#!/bin/bash

#originally copied from https://github.com/surge-synthesizer/surge-rack/blob/master/scripts/buildutil.sh

set -e

help_message()
{
    cat <<EOF
What, this isn't build-osx.sh! Come on.
But I read the directory RACK_INSTALL_DIR and use a default if that's not set
    --get-rack      Downloads rack stuff to a well named spot
    --build         Builds
    --run           Builds and runs
    --debug         Builds and debugs
If RACK_INSTALL_DIR isn't set, I default to /Users/jeremy/Projects/VCVRack-v1-SDK
A typical session could be
  ./scripts/buildutil.sh --get-rack
  ./scripts/buildutil.sh --run
EOF
}

un=`uname`
sdk="https://vcvrack.com/downloads/Rack-SDK-1.dev.ddf06a9.zip"
if [ $un = "Darwin" ]; then
    runtime="https://vcvrack.com/downloads/Rack-1.dev.ddf06a9-mac.zip"
    plugin=plugin.dylib
    mac=true
fi
if [ $un = "Linux" ]; then
    runtime="https://vcvrack.com/downloads/Rack-1.dev.ddf06a9-lin.zip"
    plugin=plugin.so
fi
if [ -z "$runtime" ]; then
    runtime="https://vcvrack.com/downloads/Rack-1.dev.ddf06a9-win.zip"
    plugin=plugin.dll
fi

if [ -z "$RACK_INSTALL_DIR" ]; then
    RACK_INSTALL_DIR="/Users/jeremy/Projects/VCVRack-v1-SDK"
    
    mkdir -p $RACK_INSTALL_DIR
fi    

get_rack()
{
    echo "Getting rack into $RACK_INSTALL_DIR"
    cd $RACK_INSTALL_DIR
    curl -o Rack_Runtime.zip $runtime
    curl -o Rack_SDK.zip $sdk

    unzip Rack_Runtime.zip
    unzip Rack_SDK.zip
}

run_rack()
{
    if [ -z "$mac" ]; then
        cd $RACK_INSTALL_DIR/Rack
        ./Rack -u "$RACK_INSTALL_DIR/User" 
    else
        cd $RACK_INSTALL_DIR
        "$RACK_INSTALL_DIR/Rack.app/Contents/MacOS/Rack" -u "$RACK_INSTALL_DIR/User" 
    fi
}

clean()
{
    RACK_DIR=$RACK_INSTALL_DIR/Rack-SDK make clean
}

build_jwmodules()
{
    RACK_DIR=$RACK_INSTALL_DIR/Rack-SDK make -j 4 all
}

install_jwmodules()
{
    RACK_DIR=$RACK_INSTALL_DIR/Rack-SDK make dist
    mkdir -p $RACK_INSTALL_DIR/User/plugins
    mv dist/*zip $RACK_INSTALL_DIR/User/plugins
    cd $RACK_INSTALL_DIR/User/plugins
    unzip *zip
}

command="$1"

case $command in
    --help)
        help_message
        ;;
    --get-rack)
        get_rack
        ;;
    --build)
        build_surge
        ;;
    --install)
        install_surge
        ;;
    --run)
        run_rack
        ;;
    --bir)
        build_jwmodules
        install_jwmodules
        run_rack
        ;;
    --clean)
	    clean
	    ;;
    *)
        help_message
        ;;
esac