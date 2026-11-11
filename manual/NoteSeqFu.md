
#### Basics

* basic idea inspired by the iOS app [Fugue Machine](https://alexandernaut.com/fuguemachine/)
* Left side of module is for sequencer control, generation, and modification.
* Right side of module is mainly for controlling the play heads and outputs.
* You can right+click the module to select the number of polyphonic channels.
* You can click and drag cells in the grid to toggle them on and off.
* You can put it in drum mode and connect a split module to each gate output

#### Left

*  **Clock Input:** when source sends a trigger, the sequence moves to the next step
*  **Note Knob:** root note if scaling pitch sent to "OUT"
*  **Scale Knob:** current musical scale or none if turned up all the way to the last value
*  **Reset Input and Button:** resets to first step in sequence (based on the play mode)
*  **Clear Input and Button:** clears the grid
*  **RND Mode:** changes what happens when you trigger the random input or button 
*  **Random Trig Input and Button:** clears the grid and generates random notes (based on the rnd mode)
*  **Random Amount Input and Knob:** determines how full the grid will be when generating random notes
*  **Shift Up Trig Input and Button:** moves the cells in the grid the number of rows set by the amount knob (notes wrap around to the bottom)
*  **Shift Down Trig Input and Button:** moves the cells in the grid down the number of rows set by the amount knob (notes wrap around to the top)
*  **Shift Chaos Trig Input and Button:** moves each cell in a random direction with the distance determined by the amount knob (notes wrap around)
*  **Shift Amount Knob:** determines how much to shift the cells
*  **Rotate Right Input and Button:** rotates the cells in the grid clockwise 90 degrees
*  **Rotate Left Input and Button:** rotates the cells in the grid counter-clockwise 90 degrees
*  **Flip Horiz Input and Button:** flips the cells in the grid right to left
*  **Flip Vert Input and Button:** flips the cells in the grid top to bottom
*  **Life Switch:** when switched to the right this turns on [conway's game of life](https://en.wikipedia.org/wiki/Conway%27s_Game_of_Life)
*  **Life Speed Knob:** determines how fast to iterate the game of life (clock divided based on clock input)

#### Top Right - Merged

*  **Merged V/oct Output:** max of 4 v/oct channels from each playhead merged into one polyphonic signal
*  **Merged Gate Output:** max of 4 v/oct gates from each playhead merged into one polyphonic signal

_NOTE: if module right click menu set to mono, merged polyphonic output has 4 channels_

_NOTE: if module right click menu set to 4, merged polyphonic output has 16 channels_

#### Right (Each play head is color coded)

*  **Start Knob:** the start offset
*  **Length Knob:** the sequence length indicated by the vertical white line and color of the play head
*  **Mode Knob:** the play mode changes how it moved through a sequence
*  **Div Knob:** the clock division to move the play head
*  **Oct Knob:** octave for the lowest note in the grid
*  **Semi Knob:** semitone offset of notes at playhead
*  **EOC Output:** at the end of the loop it will send out a trigger (first clock tick after last tick in loop)
*  **V/oct Output:** volt per octave as a polyphonic signal
*  **Gate Output:** gate as a polyphonic signal
*  **Off/On Switch:** will stop playing notes and hide the play head

#### Bottom Right

*  **Repeats Switch:** (was originally a bug) when playhead is set to a higher division and the clock ticks multiple times while the playhead is still on the note, it would repeat so i made it optional
*  **Drum Mode Switch:** [Demo Video](https://www.youtube.com/watch?v=EkFzYxhhn-g) When switched to the right, even if the note isn't 'active'(turned on) then the outputs still directly line up to the 16 lowest notes. 
So for example, you can program a drum beat with a kick at the lowest note and a snare on the 4th row up then connect your outputs to the lowest output and the 4th 
output row up and they will work more like a midi sequencer. When 'Drum Mode' is switched to the left it grabs the 16 lowest notes which are 'active'.

#### Right Click Context Menu

  * **Polyphony Channels** allows you to select the number of polyphony channels out
  * **Gate Mode**
    * **Trigger:** fires a trigger when step is on
    * **Retrigger:** retriggers a new gate when step is on
    * **Continuous:** keeps gate on until next clock tick