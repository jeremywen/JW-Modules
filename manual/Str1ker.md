
SEE ALSO: [Str1ke Tempo Management System Documentation](./Str1ke%20Tempo%20Management%20System.md)

_Outputs 1/16 notes by default - See right click menu to change the rate..._

_Max For Live Device for setting lives BPM to the same value as Str1ker_

[Str1kerBridge.amxd](./res/Str1kerBridge.amxd)

#### Controls

  *  **On/Off Switch:** turns the clock on and off
  *  **Main Fader:** Adjusts BPM - range determined by range knob
  *  **100's knob:** Changes BPM by increments of 100
  *  **10's knob:** Changes BPM by increments of 10
  *  **1's knob:** Changes BPM by increments of 1
  *  **.00's knob:** Changes BPM by increments of 0.01

#### Inputs

  *  **Fader In:** Modulates the fader position
  *  **Reset In:** Resets clock
  *  **BPM In:** Sets BPM 0-10v scales to 0-1000 BPM

#### Outputs

  *  **Clock Out:** Sends out clock ticks
  *  **BPM Out:** Sends out BPM 0-10v scales to 0-1000 BPM
  *  **Reset Out:** Sends out clock reset

#### Right Click Context Menu

  *  **1/4 Notes:** The clock will tick on the 1/4 notes of the BPM
  *  **1/8 Notes:** The clock will tick on the 1/8 notes of the BPM
  *  **1/16 Notes:** The clock will tick on the 1/16 notes of the BPM
  *  **1/32 Notes:** The clock will tick on the 1/32 notes of the BPM
  *  **1/64 Notes:** The clock will tick on the 1/64 notes of the BPM
  *  **OSC On:** Check if you want the clock to send out osc bpm values
  *  **OSC Port 7013:** sets the osc port to 7013
  *  **OSC Port 8013:** sets the osc port to 8013
  *  **OSC Port 9013:** sets the osc port to 9013
  *  **OSC Port 10013:** sets the osc port to 10013