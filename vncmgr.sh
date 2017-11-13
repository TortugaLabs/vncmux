#!/bin/sh
set -euf -o pipefail

session_file=$HOME/.vncsession
exec 3>&1
exec 1>&2

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

count=3
while [ $count -gt 0 ]
do
  count=$(expr $count - 1)

  if [ -f "$session_file" ] ; then
    disp=$(cat "$session_file")
    port=$(expr $disp + 5900)
    if nc localhost $port </dev/null >/dev/null 2>&1 ; then
      # Switch to persistent display...
      echo "localhost:$disp" 1>&3
      exit
    fi
  fi

  disp=$(allocport 6000 5900)
  [ -z $disp ] && exit 1

  echo "Starting new session on :$disp" 1>&2
  echo "$disp" > "$session_file"
  (
    exec 3>&-
    #exec </dev/null >/dev/null 2>&1
    setsid Xvnc \
      -geometry 1024x768 \
      -depth 16 \
      -pixelformat RGB565 \
      -desktop "${USER}@$(uname -n)" \
      -Protocol3.3 \
      -SecurityTypes None \
      :$disp &
    sleep 3
    if [ -x $HOME/.vncinit ] ; then
      (
	export DISPLAY=":$disp"
	. $HOME/.vncinit
      ) &
    #elif type startxfce4 ; then
      #DISPLAY=":$disp" startxfce4 & 
    elif type xterm ; then
      DISPLAY=":$disp" xterm &
    fi
  ) &
  sleep 3
done

echo "Failed to start session"
exit 3
