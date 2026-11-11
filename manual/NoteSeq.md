
#### Basics

* Left side of module is for sequencer control, generation, and modification.
* Right side of module is mainly for controlling the outputs.
* You can right+click the module to select the number of polyphonic channels.
* You can click and drag cells in the grid to toggle them on and off.
* Here is a video of a [patch from init](https://www.youtube.com/watch?v=P6cNFfb0w1s)

#### Left

*  **Clock Input:** when source sends a trigger, the sequence moves to the next step
*  **Length Input:** cv for length
*  **Length Knob:** the sequence length indicated by the vertical purple line
*  **Mode:** the play mode changes how it moved through a sequence
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

#### Right

*  **Individual Poly Outputs:** Send out the lowest 16 active notes up from the 'Lowest Note' OR send out the lowest 16 rows up from the 'Lowest Note'.  This is based on the 'Drum Mode Switch' (See Below).  The lights indicate which outputs are sending out values.
*  **Mono Outputs:** can be used to select one note from a column of many notes.
*  **Highest Note Knob:** will set the orange line right above the highest note sent out the poly outputs.
*  **Lowest Note Knob:** will set the yellow line right below the lowest note sent out the poly outputs.
*  **Drum Mode Switch:** [Demo Video](https://www.youtube.com/watch?v=EkFzYxhhn-g) When switched to the right, even if the note isn't 'active'(turned on) then the outputs still directly line up to the 16 lowest notes. 
So for example, you can program a drum beat with a kick at the lowest note and a snare on the 4th row up then connect your outputs to the lowest output and the 4th 
output row up and they will work more like a midi sequencer. When 'Drum Mode' is switched to the left it grabs the 16 lowest notes which are 'active'.
*  **Octave Knob:** octave for the lowest note in the grid
*  **Note Knob:** root note if scaling pitch sent to "OUT"
*  **Scale Knob:** current musical scale or none if turned up all the way to the last value
*  **EOC Output:** at the end of the loop it will send out a trigger (first clock tick after last tick in loop)
*  **Poly Outputs:** v/oct and gate as a polyphonic signal

#### Right Click Context Menu

  * **Polyphony Channels** allows you to select the number of polyphony channels out
  * **Gate Mode**
    * **Trigger:** fires a trigger when step is on
    * **Retrigger:** retriggers a new gate when step is on
    * **Continuous:** keeps gate on until next clock tick