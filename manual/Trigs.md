
#### Basics

* You can click and drag cells in the grid to toggle them on and off.
* Each track is four rows and has a color which matches it's output

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
*  **Triggers Outputs:** triggers will fire if track with same color has cell turned on
*  **OR Output:** fires if any track will fire
*  **XOR Output:** fires if only one track will fire
*  **NOR Output:** fires if no tracks will fire
*  **EOC Output:** at the end of the loop it will send out a trigger (first clock tick after last tick in loop)
