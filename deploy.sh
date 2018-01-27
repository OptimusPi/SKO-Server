#!/bin/bash
# SKO Deploy script
#

echo "Deploying SKO to PROD .."

# Map Data and Configuration files
echo "Copying map files .."
cp *.map ..
echo "Copying map config files .."
cp map*.ini ..
sleep 1
echo -n ". Done!"

# SKO Server Binaries
echo "Copying Starting server .."
cp skoserver ..
sleep 1
echo -n ". Done!"

#All done!
echo "SKO Deploy to PROD was successful."

sleep 1

