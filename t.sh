#!/bin/sh
# Use -f 7 for more logging
exec nc -l 5999 -c 'vnc-reflector/reflector -f 3 -l 0 ./chooser.sh'
