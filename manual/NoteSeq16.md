
#### Basics

* You can click and drag cells in the grid to toggle them on and off.
* You can right+click the module to select the number of polyphonic channels.

#### Top

*  **Clock Input:** when source sends a trigger, the sequence moves to the next step
*  **Reset Input:** resets to first step in sequence (based on the play mode)
*  **Start Input:** cv for start
*  **Start Knob:** the start offset
*  **Length Input:** cv for length
*  **Length Knob:** the sequence length indicated by the vertical purple line
*  **Mode:** the play mode changes how it moved through a sequence

#### Bottom

*  **Clear Button:** clears the grid
*  **Random Trig Input and Button:** clears the grid and generates random notes (based on the rnd mode)
*  **Random Amount Knob:** determines how full the grid will be when generating random notes
*  **Rotate:** rotates gride clockwise
*  **Flip:** flips grid vertically
*  **Shift:** shifts grid up 1 row
*  **Octave Knob:** octave for the lowest note in the grid
*  **Note Knob:** root note if scaling pitch sent to "OUT"
*  **Scale Knob:** current musical scale or none if turned up all the way to the last value
*  **Drum Mode Switch:** When switched to the right, even if the note isn't 'active'(turned on) then the outputs still directly line up to the 16 lowest notes. 
So for example, you can program a drum beat with a kick at the lowest note and a snare on the 4th row up then connect your outputs to the lowest output and the 4th 
output row up and they will work more like a midi sequencer. When 'Drum Mode' is switched to the left it grabs the 16 lowest notes which are 'active'.
*  **Poly Outs:** Send out up to 16 active notes.  This is based on the 'Drum Mode' switch. 
*  **EOC Output:** at the end of the loop it will send out a trigger (first clock tick after last tick in loop)

#### Right Click Context Menu

  * **Polyphony Channels** allows you to select the number of polyphony channels out
  * **Gate Mode**
    * **Trigger:** fires a trigger when step is on
    * **Retrigger:** retriggers a new gate when step is on
    * **Continuous:** keeps gate on until next clock tick