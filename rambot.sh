#!/bin/sh
# simple script to run rambot in a loop, so that if the bot disconnects, it will reconnect automatically.
# you can also run this from /etc/rc.local or similar, using eg "screen -d -m /home/rambot/rambot.sh"

# very important that we run rambot from it's own current working directory
cd /home/rambot
while :
do
        /home/rambot/rambotc
        sleep 5
done
