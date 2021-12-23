#!/bin/sh
# /usr/bin/rsync -avz --progress * sdob@flittermouse.local:/home/sdob/Development/sdob-host/

/usr/bin/rsync -avz --progress * sdob@$1:~/Development/sdob-host
