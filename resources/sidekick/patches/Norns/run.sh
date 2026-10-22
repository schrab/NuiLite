#!/bin/bash
echo starting norns
if systemctl list-unit-files norns-main.service 2>/dev/null | grep -q norns-main; then
    sudo systemctl start norns.target || sudo systemctl start norns-jack.service norns-sclang.service norns-main.service norns-maiden.service
else
    # Legacy norns 2.x
    sudo systemctl start norns-jack.service
    sudo systemctl start norns-crone.service
    sudo systemctl start norns-sclang.service
    sudo systemctl start norns-matron.service
    sudo systemctl start norns-maiden.service
fi

