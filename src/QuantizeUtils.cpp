#include "rack.hpp"
#include <iterator> // for std::begin, std::end
#include <algorithm>

struct QuantizeUtils {

	bool inputsOverride = false;


	//copied & fixed these scales http://www.grantmuller.com/MidiReference/doc/midiReference/ScaleReference.html
	//more scales http://lawriecape.co.uk/theblog/index.php/archives/881
	int SCALE_AEOLIAN        [8] = {0, 2, 3, 5, 7, 8, 10, 12};
	int SCALE_BLUES          [7] = {0, 3, 5, 6, 7, 10, 12}; //FIXED!
	int SCALE_CHROMATIC      [13]= {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
	int SCALE_DIATONIC_MINOR [8] = {0, 2, 3, 5, 7, 8, 10, 12};
	int SCALE_DORIAN         [8] = {0, 2, 3, 5, 7, 9, 10, 12};
	int SCALE_HARMONIC_MINOR [8] = {0, 2, 3, 5, 7, 8, 11, 12};
	int SCALE_INDIAN         [8] = {0, 1, 1, 4, 5, 8, 10, 12};
	int SCALE_LOCRIAN        [8] = {0, 1, 3, 5, 6, 8, 10, 12};
	int SCALE_LYDIAN         [8] = {0, 2, 4, 6, 7, 9, 11, 12};
	int SCALE_MAJOR          [8] = {0, 2, 4, 5, 7, 9, 11, 12};
	int SCALE_MELODIC_MINOR  [10] = {0, 2, 3, 5, 7, 8, 9, 10, 11, 12};
	int SCALE_MINOR          [8] = {0, 2, 3, 5, 7, 8, 10, 12};
	int SCALE_MIXOLYDIAN     [8] = {0, 2, 4, 5, 7, 9, 10, 12};
	int SCALE_NATURAL_MINOR  [8] = {0, 2, 3, 5, 7, 8, 10, 12};
	int SCALE_PENTATONIC     [6] = {0, 2, 4, 7, 9, 12};
	int SCALE_PENTATONIC_MINOR [6] = {0, 3, 5, 7, 10, 12};
	int SCALE_WHOLE_TONE     [7] = {0, 2, 4, 6, 8, 10, 12};
	int SCALE_DIMINISHED     [9] = {0, 2, 3, 5, 6, 8, 9, 11, 12};
	int SCALE_HARMONIC_MAJOR [8] = {0, 2, 4, 5, 7, 8, 11, 12};
	int SCALE_DOUBLE_HARMONIC[8] = {0, 1, 4, 5, 7, 8, 11, 12};
	int SCALE_MAJOR_BEBOP    [9] = {0, 2, 4, 5, 7, 8, 9, 11, 12};
	int SCALE_MINOR_BEBOP    [9] = {0, 2, 3, 5, 7, 8, 9, 10, 12};
	int SCALE_PHRYGIAN_DOMINANT [8] = {0, 1, 4, 5, 7, 8, 10, 12};
	int SCALE_HUNGARIAN_MINOR [8] = {0, 2, 3, 6, 7, 8, 11, 12};
	int SCALE_ALTERED        [8] = {0, 1, 3, 4, 6, 8, 10, 12};
	int SCALE_PHRYGIAN       [8] = {0, 1, 3, 5, 7, 8, 10, 12};
	int SCALE_TURKISH        [8] = {0, 1, 3, 5, 7, 10, 11, 12};

	enum NoteEnum {
		NOTE_C, 
		NOTE_C_SHARP,
		NOTE_D,
		NOTE_D_SHARP,
		NOTE_E,
		NOTE_F,
		NOTE_F_SHARP,
		NOTE_G,
		NOTE_G_SHARP,
		NOTE_A,
		NOTE_A_SHARP,
		NOTE_B,
		NUM_NOTES
	};

	enum ScaleEnum {
		AEOLIAN,
		BLUES,
		CHROMATIC,
		DIATONIC_MINOR,
		DORIAN,
		HARMONIC_MINOR,
		INDIAN,
		LOCRIAN,
		LYDIAN,
		MAJOR,
		MELODIC_MINOR,
		MINOR,
		MIXOLYDIAN,
		NATURAL_MINOR,
		PENTATONIC,
		PHRYGIAN,
		TURKISH,
		NONE, //keep this here so that we don't break existing patches that expect none to be here
		//below scales added 5-22-26
		PENTATONIC_MINOR,
		WHOLE_TONE,
		DIMINISHED,
		HARMONIC_MAJOR,
		DOUBLE_HARMONIC,
		MAJOR_BEBOP,
		MINOR_BEBOP,
		PHRYGIAN_DOMINANT,
		HUNGARIAN_MINOR,
		ALTERED,
		NONE2,// adding to have none at end and not break existing patches
		NUM_SCALES
	};

	// long printIter = 0;
	float closestVoltageInScale(float voltsIn, int rootNote, int currScale, bool noneIsChromatic=false) {
		if(!noneIsChromatic && (currScale == NONE || currScale == NONE2)){
			return voltsIn;
		}

		int *curScaleArr;
		int notesInScale = 0;
		switch(currScale){
			case AEOLIAN:        curScaleArr = SCALE_AEOLIAN;       notesInScale=LENGTHOF(SCALE_AEOLIAN); break;
			case BLUES:          curScaleArr = SCALE_BLUES;         notesInScale=LENGTHOF(SCALE_BLUES); break;
			case CHROMATIC:      curScaleArr = SCALE_CHROMATIC;     notesInScale=LENGTHOF(SCALE_CHROMATIC); break;
			case DIATONIC_MINOR: curScaleArr = SCALE_DIATONIC_MINOR;notesInScale=LENGTHOF(SCALE_DIATONIC_MINOR); break;
			case DORIAN:         curScaleArr = SCALE_DORIAN;        notesInScale=LENGTHOF(SCALE_DORIAN); break;
			case HARMONIC_MINOR: curScaleArr = SCALE_HARMONIC_MINOR;notesInScale=LENGTHOF(SCALE_HARMONIC_MINOR); break;
			case INDIAN:         curScaleArr = SCALE_INDIAN;        notesInScale=LENGTHOF(SCALE_INDIAN); break;
			case LOCRIAN:        curScaleArr = SCALE_LOCRIAN;       notesInScale=LENGTHOF(SCALE_LOCRIAN); break;
			case LYDIAN:         curScaleArr = SCALE_LYDIAN;        notesInScale=LENGTHOF(SCALE_LYDIAN); break;
			case MAJOR:          curScaleArr = SCALE_MAJOR;         notesInScale=LENGTHOF(SCALE_MAJOR); break;
			case MELODIC_MINOR:  curScaleArr = SCALE_MELODIC_MINOR; notesInScale=LENGTHOF(SCALE_MELODIC_MINOR); break;
			case MINOR:          curScaleArr = SCALE_MINOR;         notesInScale=LENGTHOF(SCALE_MINOR); break;
			case MIXOLYDIAN:     curScaleArr = SCALE_MIXOLYDIAN;    notesInScale=LENGTHOF(SCALE_MIXOLYDIAN); break;
			case NATURAL_MINOR:  curScaleArr = SCALE_NATURAL_MINOR; notesInScale=LENGTHOF(SCALE_NATURAL_MINOR); break;
			case PENTATONIC:     curScaleArr = SCALE_PENTATONIC;    notesInScale=LENGTHOF(SCALE_PENTATONIC); break;
			case PHRYGIAN:       curScaleArr = SCALE_PHRYGIAN;      notesInScale=LENGTHOF(SCALE_PHRYGIAN); break;
			case TURKISH:        curScaleArr = SCALE_TURKISH;       notesInScale=LENGTHOF(SCALE_TURKISH); break;
			case NONE:           curScaleArr = SCALE_CHROMATIC;     notesInScale=LENGTHOF(SCALE_CHROMATIC); break;
			case NONE2:           curScaleArr = SCALE_CHROMATIC;     notesInScale=LENGTHOF(SCALE_CHROMATIC); break;
			case PENTATONIC_MINOR: curScaleArr = SCALE_PENTATONIC_MINOR; notesInScale=LENGTHOF(SCALE_PENTATONIC_MINOR); break;
			case WHOLE_TONE:     curScaleArr = SCALE_WHOLE_TONE;    notesInScale=LENGTHOF(SCALE_WHOLE_TONE); break;
			case DIMINISHED:     curScaleArr = SCALE_DIMINISHED;    notesInScale=LENGTHOF(SCALE_DIMINISHED); break;
			case HARMONIC_MAJOR: curScaleArr = SCALE_HARMONIC_MAJOR;notesInScale=LENGTHOF(SCALE_HARMONIC_MAJOR); break;
			case DOUBLE_HARMONIC:curScaleArr = SCALE_DOUBLE_HARMONIC;notesInScale=LENGTHOF(SCALE_DOUBLE_HARMONIC); break;
			case MAJOR_BEBOP:    curScaleArr = SCALE_MAJOR_BEBOP;   notesInScale=LENGTHOF(SCALE_MAJOR_BEBOP); break;
			case MINOR_BEBOP:    curScaleArr = SCALE_MINOR_BEBOP;   notesInScale=LENGTHOF(SCALE_MINOR_BEBOP); break;
			case PHRYGIAN_DOMINANT: curScaleArr = SCALE_PHRYGIAN_DOMINANT; notesInScale=LENGTHOF(SCALE_PHRYGIAN_DOMINANT); break;
			case HUNGARIAN_MINOR:curScaleArr = SCALE_HUNGARIAN_MINOR;notesInScale=LENGTHOF(SCALE_HUNGARIAN_MINOR); break;
			case ALTERED:        curScaleArr = SCALE_ALTERED;       notesInScale=LENGTHOF(SCALE_ALTERED); break;
		}

		//C1 == -2.00, C2 == -1.00, C3 == 0.00, C4 == 1.00
		//B1 == -1.08, B2 == -0.08, B3 == 0.92, B4 == 1.92
		float closestVal = 10.0;
		float closestDist = 10.0;
		float scaleNoteInVolts = 0;
		float distAway = 0;
		// Quantize in root-relative space so repeated quantization is stable.
		float rootVolts = rootNote / 12.0f;
		float voltsRootRelative = voltsIn - rootVolts;
		int octaveInVolts = int(floorf(voltsRootRelative));
		float voltMinusOct = voltsRootRelative - octaveInVolts;
		for (int i=0; i < notesInScale; i++) {
			scaleNoteInVolts = curScaleArr[i] / 12.0;
			distAway = fabs(voltMinusOct - scaleNoteInVolts);
			if(distAway < closestDist){
				closestVal = scaleNoteInVolts;
				closestDist = distAway;
			}

			// if(printIter%100000==0){
			// 	printf("i:%i, rootNote:%i, voltsIn:%f, octaveInVolts:%i, closestVal:%f, closestDist:%f\n", 
			// 	        i,    rootNote,    voltsIn,    octaveInVolts,    closestVal,    closestDist);
			// 	printIter = 0;
			// }
		}
		// printIter++;
		float voltsOut = octaveInVolts + rootVolts + closestVal;
		// if(printIter%100000==0){
		// 	printf("voltsIn:%f, voltsOut:%f, closestVal:%f\n", 
		// 			voltsIn,    voltsOut,    closestVal);
		// 	printIter = 0;
		// }
		return voltsOut;
	}

	std::string noteName(int note) {
		switch(note){
			case NOTE_C:       return "C";
			case NOTE_C_SHARP: return "C#";
			case NOTE_D:       return "D";
			case NOTE_D_SHARP: return "D#";
			case NOTE_E:       return "E";
			case NOTE_F:       return "F";
			case NOTE_F_SHARP: return "F#";
			case NOTE_G:       return "G";
			case NOTE_G_SHARP: return "G#";
			case NOTE_A:       return "A";
			case NOTE_A_SHARP: return "A#";
			case NOTE_B:       return "B";
			default: return "";
		}
	}

	std::string scaleName(int scale) {
		switch(scale){
			case AEOLIAN:        return "Aeolian";
			case BLUES:          return "Blues";
			case CHROMATIC:      return "Chromat";
			case DIATONIC_MINOR: return "Diat Min";
			case DORIAN:         return "Dorian";
			case HARMONIC_MINOR: return "Harm Min";
			case INDIAN:         return "Indian";
			case LOCRIAN:        return "Locrian";
			case LYDIAN:         return "Lydian";
			case MAJOR:          return "Major";
			case MELODIC_MINOR:  return "Mel Min";
			case MINOR:          return "Minor";
			case MIXOLYDIAN:     return "Mixolyd";
			case NATURAL_MINOR:  return "Nat Min";
			case PENTATONIC:     return "Penta";
			case PHRYGIAN:       return "Phrygian";
			case TURKISH:        return "Turkish";
			case PENTATONIC_MINOR:return "Penta Min";
			case WHOLE_TONE:     return "Whole Tone";
			case DIMINISHED:     return "Diminished";
			case HARMONIC_MAJOR: return "Harm Major";
			case DOUBLE_HARMONIC:return "Double Harm";
			case MAJOR_BEBOP:    return "Bebop Maj";
			case MINOR_BEBOP:    return "Bebop Min";
			case PHRYGIAN_DOMINANT:return "Phryg Dom";
			case HUNGARIAN_MINOR:return "Hung Min";
			case ALTERED:        return "Altered";
			case NONE:           return "None";//old, keep this here so that we don't break existing patches that expect none to be here
			case NONE2:          return "None";//new, added to have none at end and not break existing patches
			default: return "";
		}
	}
};