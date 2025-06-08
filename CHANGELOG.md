# Change Log

## v2.0.15 ~ 6-8-25

  * AbcdSeq: added menu options to randomize each row individually
  * AbcdSeq: added menu options to copy/paste each row individually
  * AbcdSeq: added button/input to randomize all length knobs
  * AbcdSeq: added menu option to use velocity as a probability
  * AbcdSeq: added end of cycle per row
  * Arrange: colored text fields and input/output ports

## v2.0.14 ~ 5-29-25

  * Trigs: now allows poly input for 4 different clocks/start/len/playmode/random one for each sequence
  * SimpleClock: fixed dark mode
  * Arrange/Arrange16: added poly tag since each row can be poly
  
## v2.0.13 ~ 5-28-25

  * added module Arrange16, a newer smaller version of Arrange
  
## v2.0.12 ~ 5-27-25

  * Arrange: added right click option to output absolute position instead of relative to start and length
  * Arrange: added right click option to output 10v when there is no input for a row
  * Arrange: added labels to output ports for hovering over the port to see its label

## v2.0.11 ~ 5-14-25

  * fixed randomize on Arrange

## v2.0.10 ~ 5-12-25

  * fixed reset on Arrange

## v2.0.9 ~ 5-7-25

  * minor fixes for meta module
  * added Arrange module

## v2.0.8 ~ 3-14-25

  * added cv and gate outputs per row in AbcdSeq

## v2.0.7 ~ 12-26-24

  * fixed vertical lines every 4 columns in NoteSeq, NoteSeq16, and NoteSeqFu so you can now see where half way is for example
  * fixed quantizer when it is built into a sequencer so 'None' means no quantizing.  When quantizer is on its own it means chromatic.

## v2.0.6 ~ 9-5-24

  * added Timer module
  * 'S' and 's' in AbcdSeq now play previous row
  * added EOC to AbcdSeq
  * fixed - randomize velocities was an integer
  * quantizer right click menu for 'Inputs Override Knobs'
  * added blue indicator on divisions in OnePattern and Patterns
  * added white key highlight

## v2.0.5 ~ 8-26-24

  * added AbcdSeq
  * Add5 is now Polyphonic

## v2.0.3 ~ 11-14-23

  * added dark panels from Jorge Salas and updated code to use them

## v2.0.2 ~ 2-9-22

  * Fixed dragging at different zoom levels for
    noteseq, noteseq16, noteseqfu, 1pattern, patterns, trigs, xypad 

## v1.0.29 ~ 5-11-21

  * Fixed EOC on all sequencers

## v1.0.28 ~ 5-8-21

  * Fixed all gates on after reset

## v1.0.27 ~ 5-3-21

  * New Module: DivSeq
  * 8Seq, GridSeq, DivSeq: see readme and tooltips for randomize buttons

## v1.0.25 ~ 4-6-21

  * 8seq length issue fixed

## v1.0.24 ~ 3-29-21
  
  * New Module: 8Seq

## v1.0.21 ~ 12-30-20

  * Quantizer: Added Octave Shift knob and input to go up or down octaves after quantizing.
  * Fixed Pos output in D1v1de

## v1.0.20 ~ 11-8-2020

  * Trigs: Shortened trigger pulse to match my other module triggers.
  * Fixed Lydian scale in all quantizers.

## v1.0.19 ~ 11-7-2020

  * New Module: Trigs
  * Tree module now resizable (Thanks to SteveRussell33)
  * ThingThing module now resizable (Thanks to me)
  * Fixed: Tree and ThingThing no longer draw outside of bounds when LightOff Active\

## v1.0.18 ~ 11-2-2020

  * NoteSeqFu: added High/Low knobs and inputs to limit the range of notes
  * Graphic dispays in all modules now have a black background
  * Graphic displays work with Lights Off module
  * New Module: Tree visual module

## v1.0.17 ~ 10-28-2020

  * New Module: 1Pattern (basically one row of Patterns module)
  * New Module: Add5 (because Caudal is bipolar (like me))
  * NoteSeqFu 'Start' now effects running sequence
  * Added 'Start' knobs and inputs to NoteSeq and NoteSeq16
  * Added 'Length' inputs to NoteSeq16
  * Added 'Probability' knobs and randomizer to GridSeq
  * Added Shift 'Chaos' feature to NoteSeq and NoteSeqFu: shifts each note in a random direction and distance determined by amount
  * Can now shift click randomize buttons in GridSeq to init parameters to defaults
  * Fixed position of playhead on reset in NoteSeq, NoteSeq16, and NoteSeqFu

## v1.0.16 ~ 10-16-2020

  * New Module: NoteSeqFu

## v1.0.15 ~ 10-3-2020

  * Added EOC (end of cycle) to NoteSeq16
  * Added right click options to simple clock
  * Added tooltips to all knobs and buttons
  * Can now enter a BPM in simple clock and see it in the tooltip
  * Added module manual urls

## v1.0.14 ~ 9-1-2020
  * Added EOC (end of cycle) to NoteSeq
  * Fixed Reset in D1v1de
  * Added a changelog

## v0.0.0 up to v1.0.13
  * Created 20 more modules.

## v0.0.0 ~ 10-16-2017
  * [Started](https://github.com/jeremywen/JW-Modules/commit/b4037bc606ea106d8f1e8d495a2a9440cd8d921e)   JW-Modules with SimpleClock

  
