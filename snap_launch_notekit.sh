#!/bin/bash

set -x

cd "$SNAP/usr/share/notekit"

export XDG_DATA_DIRS="$SNAP/usr/share:$XDG_DATA_DIRS"
export XDG_CONFIG_HOME="$SNAP_USER_COMMON"
export XDG_DATA_HOME="$SNAP_USER_COMMON"

if [ ! -f $SNAP_USER_COMMON/.config/notekit/config.json ]
then
    mkdir -p $SNAP_USER_COMMON/.config
    mkdir -p $SNAP_USER_COMMON/.config/notekit
    cp data/default_config.json $SNAP_USER_COMMON/.config/notekit/config.json
fi

./notekit

set +x
