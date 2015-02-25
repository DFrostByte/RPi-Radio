#!/bin/bash

# take user argument as search criteria for a radio station to play
# the mandatory first argument will be used as criteria for a case-
# insensitive search of the configured search directory.

_cfg_radio_dir='/srv/ftp/radio'
_cfg_player='./omxctl.sh'
_cfg_player_args=''
_cfg_stations='stations'
_cfg_stations_basename='stations.basename'
_cfg_last_played='station_recent'

if [ "$1" == "-l" ]
then
	# basename list the stations
	cat "$_cfg_stations_basename" #2 > /dev/null
	exit $?
fi

# enclose criteria in '*' to match anywhere in filename

_criteria="$1"

# any other arguments will be passed through to the player

shift 1
_player_args=("$@")

# find a radio pls file based on the given criteria
# '-quit' is to quit after finding first regular matching file

_radio_pls=$(grep --max-count=1 "$_criteria" "$_cfg_stations")
if [ $? -ne 0 ]
then
	echo 'grep failed'
	exit 1
fi

echo "$(basename "$_radio_pls" '.pls')" > "$_cfg_last_played"

# extract one URL from the PLS file to play

_radio_url=$(grep --max-count=1 http "$_radio_pls" | cut -d '=' -f2)
if [ -z "$_radio_url" ]
then
	echo 'failed to extract url from playlist'
	exit 1
fi

#echo "Play: $_radio_pls - $_radio_url"
exec $_cfg_player play "$_radio_url"

