#!/bin/bash
echo starting norns
sudo systemctl stop norns.target 2>/dev/null
sleep 1
sudo systemctl restart norns-jack.service
sleep 2
sudo systemctl restart norns-sclang.service 2>/dev/null || true

if [ -f /etc/systemd/system/norns-main.service ] || [ -f /lib/systemd/system/norns-main.service ]; then
    sudo systemctl reset-failed norns-main.service 2>/dev/null || true
    sudo systemctl restart norns-main.service 2>/dev/null || true
else
    sudo systemctl reset-failed norns-matron.service norns-crone.service 2>/dev/null || true
    sudo systemctl restart norns-crone.service 2>/dev/null || true
    sudo systemctl restart norns-matron.service 2>/dev/null || true
fi

sudo systemctl start norns.target
