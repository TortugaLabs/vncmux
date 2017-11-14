#!/bin/sh
set -euf -o pipefail

script_dir=$(cd $(dirname "$0") && pwd)

host_info=$($script_dir/vncmgr.sh)
#
# Parse display...
#
display=$(echo $host_info | (read a b ; echo $a))
host_name=$(cut -d: -f1 <<<"$display")
host_disp=$(cut -d: -f2- <<<"$display")
host_port=$(expr $host_disp + 5900)

#cat /etc/issue
echo "Using $host_name:$host_disp ($host_port)"

exec nc $host_name $host_port
