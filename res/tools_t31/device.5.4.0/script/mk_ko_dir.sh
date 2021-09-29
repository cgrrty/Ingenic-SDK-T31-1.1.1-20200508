#!/bin/sh

MODULE_DIR=$(uname -r)
mkdir -p /tmp/modules/${MODULE_DIR}
mkdir -p /lib/modules
cd /lib/modules/
ln -s /tmp/modules/*

