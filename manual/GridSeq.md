
[Video](https://www.youtube.com/watch?v=Bnxzqi5jwcU)

[Example Patch](./doc/gridseq-example-patch-1.png)

#### Basics

  * 16 Step Sequencer
  * When a direction input is sent a trigger and a cell is entered, a voltage will be sent to the OUT port if the cell's gate is on.
  * Each cell has a main voltage knob, a smaller probability knob, and a gate button.

#### Top Direction Inputs 

  *  **Right Input:** on trigger move right one cell and send out value if gate is on for that cell
  *  **Left Input:** on trigger move left one cell and send out value if gate is on for that cell
  *  **Down Input:** on trigger move down one cell and send out value if gate is on for that cell
  *  **Up Input:** on trigger move up one cell and send out value if gate is on for that cell
  *  **RND:** on trigger move one cell in a random direction and send out value if gate is on for that cell
  *  **REPEAT:**  stay on current cell and send out value if gate is on for that cell

  **NOTE 1:** You can click the arrows or words above the inputs to trigger them.

#### Left

  *  **RUN:** values are sent to outputs if running
  *  **RESET Button:** move to top left cell on click
  *  **RESET Input:** move to top left cell on trigger
  *  **GATE OUT:** sends out gate if current cell gate is on and sequencer is running
  *  **GATE OUT YX:** instead of using the gate of the current cell it uses the gate of the cell at the inverted position (so if top right is current, bottom left is used)
  *  **OUT:** sends out the current value (pitch knob) for the current cell if the gate is on and sequencer is running
  *  **OUT YX:** instead of using the value of the current cell it uses the value of the cell at the inverted position (so if top right is current, bottom left is used)

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