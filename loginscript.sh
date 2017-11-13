#!/bin/sh
set -euf -o pipefail

# ls -l /proc/$$/fd 1>&2

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
  sleep 2
  xterm -display ":$disp" -e ./pamauth xterm

#  disp=$(allocport 5900 6000)
  #echo "$disp" 1>&$dispfd
  #eval "exec $dispfd>&-"

  #Xvnc \
    #-geometry 1024x768 \
    #-depth 16 \
    #-pixelformat RGB565 \
    #-desktop "USER_DESKTOP" \
    #-Protocol3.3 \
    #-SecurityTypes None \
    #:$disp &
  #sleep 3
  #xterm -display ":$disp" &

  
  kill $xvncpid
) &

eval "exec $dispfd>&-"
exec Xvnc \
  -geometry 800x600 \
  -depth 16 \
  -pixelformat RGB565 \
  -inetd \
  -desktop "$(uname -n) Login" \
  -Protocol3.3 \
  -SecurityTypes None \
  -once :$disp
