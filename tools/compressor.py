import os
import sys

f = open(sys.argv[1])

last_time = 0.
base_time = 0.

for event in f:
  out = []
  event = event[0:-1]
  tuple = event.split(",")
  # delta compression for time
  time = float(tuple[0]) - float(last_time)
  if base_time == 0. and time != 0.:
    base_time = time
    time -= base_time
  last_time = tuple[0] 
  out.append(time)

  # param change
  if len(tuple) > 2:
    out.append(tuple[2])
  # note on
  if len(tuple) > 3:
    out.append(tuple[3])
    # put the drum elements on back their own track: they were all in the kickdrum
    # track so that it's easier to write the music in the DAW
    if tuple[1] == 1:
      # snare
      if tuple[2] == 38
        tuple[1] = 2
      # clave
      elif tuple[2] == 39
        tuple[1] = 5
      # cowbell
      elif tuple[2] == 42
        tuple[1] = 6
      # hihat
      elif tuple[2] == 40
        tuple[1] = 3
    

  outstr = ""
  outstr += str(out[0]) + ","
  outstr += str(out[1])
  if len(tuple) > 2:
    outstr +=  "," + str(out[2])
  if len(tuple) > 3:
    outstr += "," + str(out[3])

  print outstr
