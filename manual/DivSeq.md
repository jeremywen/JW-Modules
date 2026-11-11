
Inspired by the max for live device [PX-18](https://maxforlive.com/library/device/235/px-18)

#### Basics

16 step sequencer where each step can have a division to play.  So if a step division (small knob) is 3 then that step would play every 3 steps.

NOTE: You may want to trigger a reset after changing division knobs.  This will reset all counters.

#### Controls

  *  **Length Knob:** length of sequence
  *  **Reset Button:** this resets to the beginning of the sequence and resets all step counters.

#### Inputs

  *  **Clock Input:** clock to move the steps along in a standard pattern
  *  **Length Input:** length of sequence
  *  **Reset Input:** this resets to the beginning of the sequence and resets all step counters.

#### Outputs

  *  **Gate Out:** Sends out gates debepending on probobility
  *  **V/oct Out:** Sends out pitch
  *  **EOC Out:** Sends out end of cycle triggeer at the end of the length of the loop (first clock tick after last tick in loop)

#### Bottom

  *  **ROOT Knob:** root note if scaling pitch sent to "OUT"
  *  **OCTAVE Knob:** current musical octave
  *  **SCALE Knob:** current musical scale or none if turned up all the way to the last value
  *  **RND GATE Button** randomize gate only for each cell
  *  **RND GATE Input** on trigger randomize gate only for each cell
  *  **RND DIV Button** randomize divisions only for each cell
  *  **RND DIV Input** on trigger, randomize divisions only for each cell
  *  **RND NOTE Button** randomize pitch only for each cell
  *  **RND NOTE Input** on trigger, randomize pitch only for each cell
  
  If you shift + click the random buttons, parameters will be set to their default values

#### Right Click Context Menu

  * **Gate Mode**
    * **Trigger:** fires a trigger when step is on
    * **Retrigger:** retriggers a new gate when step is on
    * **Continuous:** keeps gate on until next clock tick
    * **Ignore Gate for V/OCT Out:** If you want the pitch to continue changing even if the gates are not on, you can right click the module and check 
    'Ignore Gate for V/OCT Out'.  This can create interesting effects, for example, when gate is triggering an envelope with a long release.
  * **Random Button Mode** (applies to two bottom right randomize buttons)
    * **Random:** full range possible when randomizing knobs
    * **First is Minimum:** the first knob on the top left will be used as the minimum value when randomizing all knobs
    * **First is Maximum:** the first knob on the top left will be used as the maximum value when randomizing all knobs