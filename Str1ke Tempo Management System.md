# Str1ke Tempo Management System

## Manual

Written by Ted Pallas
v3.18.18

### INTRODUCTION

Tempo is an important concept for live performers. On the lowest levels most performance scenarios involve sequencing events in time, generally to base pulse that drives an immediate section of the performance. Tempo is how we internalize this pulse, and how we talk about this pulse with others. Tempo is tricky - sometimes it's fixed, sometimes it's changing, sometime's it's relevant because it's much faster or slower than it was a moment ago. Sometimes it's a force that gives some comfort, other times it's a force that needs to be responded to quickly.

My relationship with tempo has almost always been expressed through my relationship with computers. I specialize in playing back media - I program media servers for live performance, I produce media systems for museum installs, I VJ'd to pay the bills early on and I started DJing when I moved to Illinois a few years ago. I play drum machines with bands, I play in bands with drum machines, I've Ableton Linked the world. This captures essentially 100% of my career - there's always a clock somewhere, it's always ticking, and if I don't need to poke it now there's a good chance I'll need to poke it soon.

There's been one tool I've always wanted - a MIDI clock I could nudge, just a little bit, and then put back in place. Ableton came close - except it's Nudge buttons have a feel that is incompatible with my ability to feel time. I could gank something together with Ms Pinky, but that was too heavy on my systems. When I started working in the studio as Str1ke last Spring it was with M. Sylvia, a Chicago DJ transitioning from playing on digital systems to rocking vinyl exclusively whenever possible. Some lab time in December 2017 revealed VCV Rack as a potent partner to a vinyl-based DJ system - the slightly hot signal coming off the needle and mixer chain did nice things to most of the DSP.

What I wanted, more than anything else, was to put a donk on the Rack-DJ sessions - not all the time, but just every once in a while, to stitch grooves together while giving me the ability to do what I had to with gain structure as things played out. A connection with Jeremy Wentworth lead very quickly to the clocking system described in this document. His DNA is in here as much as my own, expressed especially in the world of possibility opened up by the D1v1de module's offset functions.

In a nutshell - working with a DJ made me want to be able to think of tempo the same way a DJ does. There's one tempo, and then there's another tempo, and I'm going to choose one to change and change it so beats line up. The clock Jeremy and I cooked up together gives you a Technics 1200-style pitch fader you can use to reach out and touch anything via the Ableton Live API (for super tight MIDI clocks) or OSC Tempo-change messages. The VCV Rack environment allows you to output pulses as audio, making the clock a natural choice for syncing real-world analog gear as well. A quick controller mapping gives you a tempo fader with a 0% pitch deviation in the center, a knob to change the % deviation on either side, and a button to reset the gear you're working on (mapped outside of our software, of course.) Your Live PA rig just became a CDJ-style playback system. Two buddy modules - D1v1de and Pres1t - make core tempo tasks even simpler to approach. Our hope is that you consider VCV Rack as a single-serving tool, sometimes, like when you need a really great clock with a very flexible way to manipulate time.

What are you going to sync to?

Thanks,
Ted

### USAGE

#### Str1ker Use

Add a Str1ker module from the Str1ke x JW Modules folder. Change the position of the long fader, moving it all the way up or down. Launch Ableton Live as well (tested with 9/10 Beta/10 Trial), and drop the included AMXD file onto an empty MIDI track. Assuming you have the port open, click 7013 and then the Big Button - you see the tempo match your changed-tempo in Rack. As you move the fader or one of the Digit Knobs on Str1ker, tempo will chase in Ableton. If you turn on Link in Ableton and join a Link session, the Str1ker clock can drive the session.

You can "map" the Fader by connecting "Fader In" to a MIDI CC coming in on the Core MIDI CC to CV Converter module. Turn the "range" knob to set the +/- %-pitch deviation. The reset button will send a reset message with the clock signal. BPM In takes a BPM message in from Pres1t, and assigns it to the knobs - you could use other CV, though this is not the intended use. BPM Out sends a BPM message as CV from Str1ker to Pres1t - again, this CV is intended for a specific use but can routed anywhere.

Right-clicking will reveal a menu where you can change your OSC port (to accommodate the default port 7013 already being in use) and turn the OSC portion of the module On or Off.

#### D1v1de Use

D1v1de takes a clock out from Str1ker and divides it by the value set by the D1v1de knob. You could use this to generate any series of ticks firing from every other pulse (2) to every 64 pulses (64). In addition to ticks you also get a POS output, which scales output from 0v to 10v and gives the current position of the trailing edge of the marker bar.

The Offset knob places the 0v position. Sending a bang to Reset In will send the position marker to the top of the module, and the resulting Pos output will be determined by the position of the offset knob. For example, placing the Offset knob at the 12 noon position would make the top of the D1v1de field equal 5000v, the position before half around 10000v, and the position at half around 0v.

My favorite use for the D1v1de module is making a reverb very slowly get bigger, but still in a way relevant to time. Dividing by 64 creates 64 slow pulses that get bigger in terms of CV every 64 steps.

#### Pres1t Use

Pres1t is a tempo management solution structured to let you punch CV values out of an output by activating a spot on a grid. We made it for you to store BPMs for feeding to Str1ker, so you can easily store a bunch of tempos for a setlist's worth of songs. You can use it to store CV from anywhere that will generate voltage within Rack, though, and then send it anywhere too.

Volts In catches values. The Save button - also addressable via CV - will place the value in the cell currently selected in the grid above. Any data present in the cell will be overwritten, and there is no undue.

Creative applications for Pres1t are opened up by the X/Y, Load and Save CV inputs. You can use Pres1t as a very coarse execution of a "wavetable" for control values, driving the X or Y values with an envelope or two. You can combine this idea with a level of real-time input, as well, banging Save and Load as you navigate X and Y to recall different values. (NOTE: This is currently a bit of a funky feature, Pres1t outputs it's value for just a moment, then releases back to 0. This won't be forever.)
