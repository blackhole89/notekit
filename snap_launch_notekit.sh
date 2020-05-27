#!/bin/bash

set -x

cd /snap/notekit/current/usr/share/notekit

if [ ! -f $SNAP_USER_DATA/.config/notekit/config.json ]
then
    cp data/default_config.json $SNAP_USER_DATA/.config/notekit/config.json
fi

./notekit

set +x
