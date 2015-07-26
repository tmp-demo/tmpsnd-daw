import os
import sys

f = open(sys.argv[1])

last_time = 0.
base_time = 0.

def strip_zeros(f):
    return ('%f' % float(f)).rstrip('0').rstrip('.')

for event in f:
    out = []
    event = event[0:-1]
    tuple = event.split(",")
    # delta compression for time
    channel = tuple[1]
    time = float(tuple[0]) - float(last_time)
    if base_time == 0. and time != 0.:
        base_time = time
        time -= base_time

    last_time = tuple[0] 
    out.append(float(time))
    out.append(int(channel))

    # note off: strip it for drums
    if len(tuple) == 2:
        if tuple[1] in [1,2,3,5,6]:
            continue

    # param change
    if len(tuple) > 2:
        out.append(float(tuple[2]))
    # note on
    if len(tuple) > 3:
      out.append(float(tuple[3]))
    # put the drum elements on back their own track: they were all in the kickdrum
    # track so that it's easier to write the music in the DAW
    if len(tuple) > 3 and int(tuple[1]) == 1:
        # snare
        if int(tuple[2]) == 38:
            out[1] = 2
        # clave
        elif int(tuple[2]) == 39:
            out[1] = 5
        # cowbell
        elif int(tuple[2]) == 42:
            out[1] = 6
        # hihat
        elif int(tuple[2]) == 40:
            out[1] = 3

    outstr = ""
    outstr += strip_zeros(out[0]) + ","
    outstr += strip_zeros(out[1])
    if len(out) > 2:
        outstr +=  "," + strip_zeros(out[2])
    if len(out) > 3:
        outstr += "," + strip_zeros(out[3])

    print outstr
