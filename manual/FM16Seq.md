
A sequencer with a 2 operator FM Synth for each step.  Inspired by FMS https://lo-bit.club/fms

Click the step buttons to edit the sequencer params for that step.

Right click on module to see context menu and menu items for copying and pasting steps.

* **Clock:** advances sequencer on trigger
* **Rst:** resets sequencer on trigger
* **Pitch:** polyphonic input to set pitch for each of the 16 steps
* **Index:** polyphonic input to set index for each of the 16 steps
* **ModR:** polyphonic input to set modulator ratio for each of the 16 steps
* **CarR:** polyphonic input to set carrier ratio for each of the 16 steps
* **Fdbk:** polyphonic input to set feedback for each of the 16 steps
* **G Len:** polyphonic input to set gate length for each of the 16 steps
* **Mode:** the order steps are played back
* **Len:** sequencer length
* **Lvl:** master level
* **Out:** main audio out

Step Params

* **Div:** division for step to trigger on, zero doesn't play step
* **Pitch:** pitch for step
* **FM Idx:** fm index for step
* **Level:** level for step
* **Car:** carrier ratio for step
* **Mod:** modulator ratio for step
* **Fdbk:** feedback for step
* **G Len:** gate length for step
* **Mod Envelope:** modulator envelope for step
* **Car Envelope:** carrier envelope for step

Randomize and Initialize

* **RND STEP/ALL Switch:** when clicking randomize does it effect the current step or all steps
* **RND Button:** randomize the current step or all steps
* **INIT** initialize params for all steps
* **RND** randomize param for all steps
* **RND MAX** the maximum range of randomization (for example only random low ratios)
* **Int:** only use musical ratios when randomizing (0.125, 0.25, 0.5, 1, 2...)
* **Decay Only:** only randomize decays and leave other envelope params at zero
* **Excl Zero:** exclude zero as an option when randomizing divisions
