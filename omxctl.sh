#!/bin/bash

OMXPLAYER_DBUS_ADDR="/tmp/omxplayerdbus.${USER}" #$(whoami | grep -v www-data)"
OMXPLAYER_DBUS_PID="$OMXPLAYER_DBUS_ADDR.pid"
export DBUS_SESSION_BUS_ADDRESS=`cat $OMXPLAYER_DBUS_ADDR`
export DBUS_SESSION_BUS_PID=`cat $OMXPLAYER_DBUS_PID`

_dbus_omx='dbus-send --print-reply=literal --session --reply-timeout=500 --dest=org.mpris.MediaPlayer2.omxplayer /org/mpris/MediaPlayer2'

dbus_props_='org.freedesktop.DBus.Properties'
dbus_method_='org.mpris.MediaPlayer2.Player'

[ -z "$DBUS_SESSION_BUS_ADDRESS" ] && { echo "Must have DBUS_SESSION_BUS_ADDRESS" >&2; exit 1; }

case $1 in
play)
	while pgrep omxplayer > /dev/null
	do
		$0 stop
		sleep .25
	done

	echo "$2" > 'last_play_uri'

	exec nohup omxplayer -o local --vol "$(cat volume)" "$2" \
		< /dev/null 2>&1 > /dev/null 2>&1 > /dev/null &

	exit $?
;;
volu)
	vol_=$(cat volume)
	new_vol_=$vol_
	for ((i = $2; $i; i--))
	do
		if $_dbus_omx $dbus_method_.Action int32:18
		then
			new_vol_=$(($new_vol_ + 300))
		fi
	done
	if [ $new_vol_ -ne $vol_ ]
	then
		echo $new_vol_ > volume
		true
	else
		false
	fi
;;
vold)
	vol_=$(cat volume)
	new_vol_=$vol_
	for ((i = $2; $i; i--))
	do
		if $_dbus_omx $dbus_method_.Action int32:17
		then
			new_vol_=$(($new_vol_ - 300))
		fi
	done
	if [ $new_vol_ -ne $vol_ ]
	then
		echo $new_vol_ > volume
		true
	else
		false
	fi
;;
stop)
	$_dbus_omx $dbus_method_.Action int32:15
;;
status)
	duration=`$_dbus_omx $dbus_props_.Duration`
	[ $? -ne 0 ] && exit 1
	duration="$(awk '{print $2}' <<< "$duration")"

	position=`$_dbus_omx $dbus_props_.Position`
	[ $? -ne 0 ] && exit 1
	position="$(awk '{print $2}' <<< "$position")"

	playstatus=`$_dbus_omx $dbus_props_.PlaybackStatus`
	[ $? -ne 0 ] && exit 1
	playstatus="$(sed 's/^ *//;s/ *$//;' <<< "$playstatus")"

	paused="true"
	[ "$playstatus" == "Playing" ] && paused="false"
	echo "Duration: $duration"
	echo "Position: $position"
	echo "Paused: $paused"
;;
pause)
	$_dbus_omx $dbus_method_.Action int32:16
;;
seek)
	$_dbus_omx $dbus_method_.Seek int64:$2
;;
setposition)
	$_dbus_omx $dbus_method_.SetPosition objpath:/not/used int64:$2
;;
setvideopos)
	$_dbus_omx $dbus_method_.VideoPos objpath:/not/used string:"$2 $3 $4 $5"
;;
hidevideo)
	$_dbus_omx $dbus_method_.Action int32:28
;;
unhidevideo)
	$_dbus_omx $dbus_method_.Action int32:29
;;
togglesubtitles)
	$_dbus_omx $dbus_method_.Action int32:12
;;
hidesubtitles)
	$_dbus_omx $dbus_method_.Action int32:30
;;
showsubtitles)
	$_dbus_omx $dbus_method_.Action int32:31
;;
*)
	echo "usage: $0 status|pause|stop|seek|volumeup|volumedown|setposition [position in microseconds]|hidevideo|unhidevideo|togglesubtitles|hidesubtitles|showsubtitles|setvideopos [x1 y1 x2 y2]" >&2
	exit 1
;;
esac

exit $?
