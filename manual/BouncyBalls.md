
#### Basics

* The top section contains all the inputs.
* The bottom sections contains all the outputs.
* The colors of the inputs/outputs correspond to the ball colors.
* The top two rows have buttons next to the inputs for manually triggering.
* The next three rows have knobs next to the inputs for manually controlling the values.
* Click in the dark area to lock/unlock the paddle.
* Click the 'ON' button to toggle the paddle visibility.

#### Inputs

*  **RST:** starts ball back at center with initial velocity
*  **TRIG:** speeds up ball based on velocity settings
*  **VELX:** velocity in the x direction, to the right is positive (towards east), to the left is negative (towards west)
*  **VELY:** velocity in the y direction, to the right is positive (towards south), to the left is negative (towards north)
*  **MULT:** velocity multiplier

#### Outputs

*  **X:** x position for each ball
*  **Y:** y position for each ball
*  **N:** (north) trigger out for each ball that hits the top
*  **E:** (east) trigger out for each ball that hits the right side
*  **S:** (south) trigger out for each ball that hits the bottom
*  **W:** (west) trigger out for each ball that hits the left side
*  **EDGE:** trigger out for each ball that hits any side
*  **PAD:** trigger out for each ball that hits the white pad

#### Modifying the Output Signal

*  **X Scale Knob:** multiplies x output by a value between 0.01 and 1.0
*  **Y Scale Knob:** multiplies y output by a value between 0.01 and 1.0
*  **X Offset Knob:** +/- 5v added to X Output
*  **Y Offset Knob:** +/- 5v added to Y Output