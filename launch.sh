#!/bin/bash
# SKO Launch script
#

echo "Starting Stick-Knights-Online server"

#clear TCP port
echo "Clearing port 1338"
fuser 1338/tcp

#backup the server log
echo -n "backing up server log file..."
if [ -e "server.log" ]
then
	mv server.log "backup/server.log-$(date | cut -f 6 -d" ")-$(date | cut -f 2 -d" ")-$(date | cut -f 3 -d" ")-$(date | cut -d" " -f4 | cut -c1-5)"
else
	echo "no server log file detected..."
fi
echo "...done!"

#run the server
echo -n "Starting server..."
nohup ./skoserver-dev > server.log &

#All done!
echo "...Stick-Knights-Online server is running!"

sleep 1

tail -f server.log
