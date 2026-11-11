
#### Basics

It's like gridseq but simpler (no step direction handling) and you can use the probability as cv instead of not probabability.

#### Controls

  *  **Length Knob:** length of sequence
  *  **Prob Switch:** If on(up), then probability is used to decide if a gate is sent out. If off(down), then probability is NOT used to decide if a gate is sent out. (this way you can just use the probability as cv and not related to the gates)

#### Inputs

  *  **Clock Input:** clock to move the steps along in a standard pattern
  *  **Length Input:** length of sequence
  *  **Reset Input:** reset back to beginning

#### Outputs

  *  **Gate Out:** Sends out gates debepending on probobility
  *  **V/oct Out:** Sends out pitch
  *  **EOC Out:** Sends out end of cycle triggeer at the end of the length of the loop (first clock tick after last tick in loop)
  *  **Prob Out:** Sends out voltage of current probability knob value

#### Bottom

  *  **ROOT Knob:** root note if scaling pitch sent to "OUT"
  *  **OCTAVE Knob:** current musical octave
  *  **SCALE Knob:** current musical scale or none if turned up all the way to the last value
  *  **RND GATE Button** randomize gate only for each cell
  *  **RND GATE Input** on trigger randomize gate only for each cell
  *  **RND PROB Button** randomize probability only for each cell
  *  **RND PROB Input** on trigger, randomize probability only for each cell
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