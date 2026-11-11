
#### Basics

Rows A, B, C, and D are step sequencers.  The order that the step sequences play is based on the text input.  So ABAC will play row A until its length, and then play row B until its length, then row A until its length, then row C until its length, then loop around to the first character.  Upper case will play the sequence forwards and lower case will play the sequence backwards.

#### Controls

  *  **Reset Button:** this resets to the beginning of the sequence
  *  **Length Knobs:** length of each sequence

#### Inputs

  *  **Row Order:** Possible characters: A, B, C, D will jump to those rows, O will jump to any other row, R will jump to any row.  S will repeat the same row as previous letter.  If the text box is empty then it will just loop over rows ABCD.  Upper case will play the sequence forwards and lower case will play the sequence backwards.
  *  **Clock Input:** clock to move the steps along to the right and to the next row
  *  **Reset Input:** this resets to the beginning of the sequence and resets all step counters.
  *  **Length Inputs:** length of each sequence

#### Outputs

  *  **Gate Out:** Sends out gates
  *  **CV Out:** Sends out cv based on quantizer settings
  *  **Velocity Out:** Sends out cv 0-10
  *  **EOC Out:** Sends out trigger at the end of the sequence of letters

#### Bottom

  *  **ROOT Knob:** root note if scaling pitch sent to "OUT"
  *  **OCTAVE Knob:** current musical octave
  *  **SCALE Knob:** current musical scale or none if turned up all the way to the last value
  *  **RND GATE Button** randomize gate only for each cell
  *  **RND GATE Input** on trigger randomize gate only for each cell
  *  **RND VEL Button** randomize velocities only for each cell
  *  **RND VEL Input** on trigger, randomize velocities only for each cell
  *  **RND NOTE Button** randomize pitch only for each cell
  *  **RND NOTE Input** on trigger, randomize pitch only for each cell
  *  **RND TXT Button** randomize text
  *  **RND TXT Input** on trigger, randomize text
  
  If you shift + click the random buttons, parameters will be set to their default values

#### Right Click Context Menu

  * **Gate Mode**
    * **Trigger:** fires a trigger when step is on
    * **Retrigger:** retriggers a new gate when step is on
    * **Continuous:** keeps gate on until next clock tick
    * **Ignore Gate for V/OCT Out:** If you want the pitch to continue changing even if the gates are not on, you can right click the module and check 
    'Ignore Gate for V/OCT Out'.  This can create interesting effects, for example, when gate is triggering an envelope with a long release.