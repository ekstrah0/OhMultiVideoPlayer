Oh! Multi Video Player

You can watch multiple live videos simultaneously with Oh! Multi Video Player.

One focused video is played in full resolution, others are played in reduced
resolution due to the memory consumption issue. Except for one focused video,
other videos are displayed as a snapshot still image.

With "-p" option, you can set proxy server uri. Suppose
"rtp://233.18.158.206:5000" is specified in m3u file, and the udpxy is running
at "172.30.1.201" with port number "4022". With "-p http://172.30.1.201:4022"
option, it tries to connect to
"http://172.30.1.201:4022/rtp/233.18.158.206:5000".
With "-a" option, you can change animation duration. If you want to disable
animation completely, run with "-a 0" option.
With "-n" option, you can change the number of videos per row. By default,
it tries to find the optimal number of videos per row based on the total number
of the specified uris.
With "-j" option, you can change the number of scan jobs. If you increase the
number of scan jobs, you can see snapshot still images are updated faster, and
if you increase to equal or more than the number of the specified uris, you can
see scan videos are also played in realtime. However, you need take caution,
because it can cause network congestion.
With "-t" option, you can change the scan video timeout. The default scan video
timeout is 10000ms(10 seconds). It tries to get snapshot still image while
scanning videos. If it can't get the snapshot still image for the timeout, it
gives up and tries the next uri.
With "-i" option, you can change how long the text will be displayed. By
default, the text disappears 3 seconds later if there is no input.
With "-w" and "-h" option, you can change scan video resolution. By default,
the snapshot still image resolution is 480x270. You can increase/decrease the
still image resolution. You need take caution when you increase the resolution
because it can consume a lot of memory.
With "-v" option, you can set the initial volume.
With "-m" option, you can start in mute mode.
With "-o" option, you can start without displaying text.
With "-d" option, you can start with displaying debug text.

Run "./configure ; make" to build omvp. Modify omvp.m3u file as you want and
run "./omvp". For the detailed build information, consult INSTALL,
INSTALL.ubuntu, INSTALL.windows, or INSTALL.macos.

You can use omvp with the following shortcut keys.
Left/Right/Up/Down : Move focus left/right/up/down
+/-                : Zoom in/out
Enter              : Zoom in fully
F11                : Fullscreen/Unfullscreen
PgUp/PgDn          : Volume up/down
Home               : Change audio
End                : Mute/Unmute
Insert             : Change text mode(none/main_only/main_debug)

For more detail, please run omvp --help.

This program is distributed under GPLv3. If you can't conform to GPLv3,
please contact me to discuss about other license options.

Written by Taeho Oh <ohhara@postech.edu>
http://ohhara.sarang.net/omvp
