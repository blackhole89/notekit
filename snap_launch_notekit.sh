#!/bin/bash

set -x

cd /snap/notekit/current/usr/share/notekit

if [ ! -f $SNAP_USER_DATA/.config/notekit/config.json ]
then
    mkdir -p $SNAP_USER_DATA/.config
    mkdir -p $SNAP_USER_DATA/.config/notekit
    cp data/default_config.json $SNAP_USER_DATA/.config/notekit/config.json
fi

./notekit

set +x
