#!/bin/sh
dir=$(cd $(dirname $0) && pwd)
exec tclsh $dir/vncutil.tcl in_sshvnc
