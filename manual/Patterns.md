
Here is the [post by chaircrusher that inspired Patterns](https://community.vcvrack.com/t/using-xors-with-clocks-for-rhythmic-variations/5293)

#### Basics

* Each row is a clock divider.  Each cell is a division 1 to 16, left to right.
* Each row has and 'or' output and an 'xor' output.
* The 'or' output will send a trigger if it hit any of the divisions.
* The 'xor' output will send a trigger if it hit exactly one of the divisions.
* You can click and drag cells in the grid to toggle them on and off.
* You can right+click the module to select the number of polyphonic channels.
* Ever four rows there is a thicker lines in the grid which correspond to the lines in the outputs every four rows.

#### Top

*  **Clock Input:** when source sends a trigger, advances the clock one step
*  **Reset Input:** resets to first step 
*  **Rotate Button:** rotates the grid to the right
*  **Shift Button:** shifts the grid up
*  **Clear Button:** clears the grid

#### Bottom

*  **Random Trig Input and Button:** clears the grid and generates random cells (based on the rnd mode)
*  **Random Amount Knob:** determines how full the grid will be when generating random cells
*  **Poly Outs:** Send out 'or' outputs or send out 'xor' outputs. 