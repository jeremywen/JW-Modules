
Trigger gates and notes with other cv with ratchets and probability.

* **CLK:** advances sequencer on trigger, polyphonic input for each track
* **RST:** resets sequencer on trigger, polyphonic input for each track
* **START:** sets start position of sequencer, polyphonic input for each track
* **LEN:** sets end position of sequencer, polyphonic input for each track
* **MODE:** sets play mode of sequencer, polyphonic input for each track
* **CLR:** clears sequencer, polyphonic input for each track
* **RND:** randomizes sequencer, polyphonic input for each track
* **SEED:** sets random number seed

CELL

* **PITCH:** the pitch of the cell
* **OCT:** the octave of the cell
* **PROB:** the probability of the cell
* **DIV:** the division of the cell, for example if 2 then plays every other time
* **RATCH:** the ratchets of the cell
* **CV:** an extra cv for the cell
* **CV2:** an extra cv for the cell

INIT/RND

These buttons init or randomize the param above for every cell.

LIFE/DEATH

Turn on life switch.  Death output triggers when life stops.

* **RND GATES:** randomizes the gates only
* **NOTE:** sets the root note for the scale
* **OCT:** sets the octave
* **SCALE:** sets the scale
* **RANGE:** allows you to set a small or large pitch range for all notes

OUTPUTS (See right click menu for polyphonic output on first row)

* **V/OCT:** v/oct output for each track
* **GATE:** gate output for each track
* **CV:** cv output for each track
* **CV2:** cv2 output for each track
* **EOC:** eoc output for each track