#!/bin/bash
echo stopping norns
if [ -f ~/dust/data/system.state ]; then
    cp ~/dust/data/system.state /tmp/system.state && sed 's/norns.state.clean_shutdown = false/norns.state.clean_shutdown = true/' < /tmp/system.state > ~/dust/data/system.state
fi
sudo systemctl stop norns-main.service norns-matron.service norns-crone.service norns-sclang.service norns-maiden.service norns-jack.service
sleep 1

