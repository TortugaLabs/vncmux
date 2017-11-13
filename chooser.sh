#!/bin/sh
set -euf -o pipefail

dispfd="$1"

allocport() {
  local try=0 start=$$ i display
  while [ $try -lt 100 ]
  do
    try=$(expr $try + 1)
    display=$(expr $(expr $try + $start) % 100)
    for i in "$@"
    do
      port=$(expr $i + $display)
      if nc localhost $port </dev/null >/dev/null 2>&1 ; then
        continue 2
      fi
    done
    echo "$display"
    return
  done
  # No display found!
}
disp=$(allocport 6000)
[ -z $disp ] && exit 5

(
  xvncpid=$$
  exec 1>&2 </dev/null
  echo "Using display :$disp"
  echo "XvncPID: $xvncpid"
  echo "MyPID: $(sh -c 'echo $PPID')"
  echo "DISPFD: $dispfd"
  sleep 1
  export DISPLAY=":$disp"
  
  #eval xterm -e ./pamauth $(pwd)/vncmgr.sh $dispfd>/dev/null
  echo "ENTERING MAIN LOOP ===================+"
  newdisp=$(./pamauth $(pwd)/vncmgr.sh) || echo "EXIT: $?"
  echo "NEWDIPS: $newdisp ____________________"
  if [ -z "$newdisp" ] ; then
    wish <<-EOF
	label .l -text "Unable to start VNC session..." -fg red
	pack .l
	EOF
    kill $xvncpid
    exit
  fi
  ( exec wish <<-EOF
	label .l -text "Connecting to $newdisp..."
	pack .l
	EOF
  ) &
  otherpid=$!
  echo "Signaling reflector on $dispfd..."
  eval "echo $newdisp" >&"$dispfd"

  # Connecting...
  sleep 10
  kill $otherpid
  wish <<-EOF
	label .l -text "Unable to resume VNC session" -fg red
	pack .l
	EOF
  kill $xvncpid
  exit
) &

eval "exec $dispfd>&-"
exec Xvnc \
  -geometry 480x320 \
  -depth 16 \
  -pixelformat RGB565 \
  -inetd \
  -desktop "$(uname -n) Login" \
  -Protocol3.3 \
  -SecurityTypes None \
  -once :$disp
