#!/bin/bash
echo starting orac
sudo systemctl stop norns-main.service norns-sclang.service norns-jack.service 2>/dev/null || true
sudo systemctl start orac.service
# let user release b1
sleep 1
sudo systemctl start mec.service


