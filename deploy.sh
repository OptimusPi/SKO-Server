#!/bin/bash
# SKO Deploy script
#

#Start Deploy Process
success='\033[1;32m'
echo -e "${success}Deploying SKO to PROD .."
color='\033[1;36m'
color2='\033[1;34m'
done='\033[1;0m'
sleep 1

# Map Data Files
echo -e -n "${color}Copying map files .."
cp *.map ..
echo -e "${color}. Done!"

# Map Configuration Files
echo -e -n "${color}Copying map config files .."
cp map*.ini ..
echo -e "${color}. Done!"
sleep 1

# SKO Server Binaries
echo -e -n "${color2}Copying SKO Server code .."
cp skoserver ..
echo -e "${color2}. Done!"
sleep 1

#All done!
echo -e "${success}SKO Deploy to PROD was successful."
echo -e "${done}"
