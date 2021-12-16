#!/bin/sh
# /usr/bin/rsync -avz --progress * sdob@flittermouse.local:/home/sdob/Development/sdob-host/

/usr/bin/rsync -avz --progress * $1:~/sdobox/sdob-host
