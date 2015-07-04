# tmpsnd-daw

VST plugin to author tmpsnd tracks in a DAW, written in C++ using Juce and `libwebsockets`

# General order of operation

The VST, when loaded, creates a WebSocket server on port 7681. It waits for
configuration data, sent by the web browser. Then it creates a bunch of sliders,
that are VST parameters, that can be automated. When it receives incoming MIDI
events or parameter change events, it sends that to the WebSocket, using the
following text protocol:

- Parameter change: `time,index,value`:
  -`time` is the time where the event happened in seconds (floating point)
  -`index` is the index of the parameter (integer)
  -`value` is the new value of the parameter (float).

- Note On: `time,index,note,velocity`:
  -`time` is the time where the event happened in seconds (floating point)
  -`index` is the index of the instrument, which is equal to the midi channel
  where the note was sent from (integer)
  - `note` the MIDI note for this event
  - `velocity` the MIDI velocity for this event
- Note Off: `time,index`
  -`time` is the time where the event happened in seconds (floating point)
  -`index` is the index of the instrument, which is equal to the midi channel
  where the note was sent from (integer)

# How to build?

Get a copy of the VST SDK 2.4 from steinberg. Put it in `~/SDKs/vstsdk2.4`. I
can't redistribute it here. You might or might not have to create an account at
Steinberg.

## Windows

lol, I'll get to it eventually, it's a pain.

## Linux

Install libwebsockets. For example, on Debian/Ubuntu, run `sudo apt-get install
libwebsockets`. Then, `cd Builds/Linux/`, and `make`. That gives you a `.so`
file that you can load in a DAW in `Builds/Linux/build`.

## OSX

Install libwebsockets. The version in `brew` is fine:
`brew install libwebsockets`.

Open the XCode project that is at `Builds/MacOSX/tmpsnd-daw.xcodeproj`, and
build that. That gives you a thing that is called `tmpsnd-daw.component` in
`Builds/MacOSX/build/Debug/`. You can load that in your DAW.

# How to debug?

When loaded in a DAW, you can simply attach a debugger, and debug from there. I
recommend using the `Audio Plugin Host` from the Juce library, that is very
lightweight, open source, and runnable in gdb/lldb/whatever easily.

# License
The code in `Source/` is BSD.
