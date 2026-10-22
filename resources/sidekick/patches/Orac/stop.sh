#!/bin/bash
echo stopping orac
sudo systemctl stop mec.service
sudo systemctl stop orac.service
killall -9 mec-app pd 2>/dev/null || true
sleep 1


