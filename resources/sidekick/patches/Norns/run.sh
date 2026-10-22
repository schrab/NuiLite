#!/bin/bash
echo starting norns
sudo systemctl stop norns.target 2>/dev/null
sleep 1
sudo systemctl restart norns-jack.service
sleep 2
sudo systemctl reset-failed norns-main.service 2>/dev/null
sudo systemctl start norns.target


