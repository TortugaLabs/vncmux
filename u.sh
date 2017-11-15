#!/bin/sh

echo none | vncpasswd -f > basic
exec Xvnc \
  -geometry 480x320 \
  -depth 16 \
  -pixelformat RGB565 \
  -desktop "$(uname -n) Test" \
  -SecurityTypes TLSNone \
  -Protocol3.3 \
  -localhost \
  -Log '*:stderr:100' \
  :73

