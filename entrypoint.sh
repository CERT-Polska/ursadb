#!/bin/bash

cd /var/lib/ursadb

if [ ! -f "$1" ]
then
    /usr/bin/ursadb_new "$1"
fi

/usr/bin/dumb-init -- /usr/bin/ursadb $@

