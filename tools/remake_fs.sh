#!/usr/bin/env bash

bash disk
python3 tools/gen_rootfs.py
./build.sh