#!/bin/sh

PEDANTIC="-Wall -Wextra -Werror -std=gnu99 -pedantic"
GOPTZ="-Og"
OPTIMIZ="-g -D_DEBUG $GOPTZ" # Debug build
CFLAGS="$PEDANTIC $GOPTZ $OPTIMIZ"

gcc $CFLAGS pamauth.c -lpam -o pamauth
sudo chown root:root pamauth
sudo chmod 4755 pamauth
