#!/bin/bash
# Run from repository root
scripts/build_firmware.sh > build_output.log 2>&1
echo $?
