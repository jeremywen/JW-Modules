#include "rack.hpp"


using namespace rack;


extern Plugin *plugin;

////////////////////
// module widgets
////////////////////

struct SimpleClockWidget : ModuleWidget {
	SimpleClockWidget();
};

struct GridSeqWidget : ModuleWidget {
	enum ScalesEnum {
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
		SCALE_COUNT
	};
	//copied from http://www.grantmuller.com/MidiReference/doc/midiReference/ScaleReference.html
	int scale_AEOLIAN        [7] = {0, 2, 3, 5, 7, 8, 10};
	int scale_BLUES          [9] = {0, 2, 3, 4, 5, 7, 9, 10, 11};
	int scale_CHROMATIC      [12]= {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
	int scale_DIATONIC_MINOR [7] = {0, 2, 3, 5, 7, 8, 10};
	int scale_DORIAN         [7] = {0, 2, 3, 5, 7, 9, 10};
	int scale_HARMONIC_MINOR [7] = {0, 2, 3, 5, 7, 8, 11};
	int scale_INDIAN         [7] = {0, 1, 1, 4, 5, 8, 10};
	int scale_LOCRIAN        [7] = {0, 1, 3, 5, 6, 8, 10};
	int scale_LYDIAN         [7] = {0, 2, 4, 6, 7, 9, 10};
	int scale_MAJOR          [7] = {0, 2, 4, 5, 7, 9, 11};
	int scale_MELODIC_MINOR  [9] = {0, 2, 3, 5, 7, 8, 9, 10, 11};
	int scale_MINOR          [7] = {0, 2, 3, 5, 7, 8, 10};
	int scale_MIXOLYDIAN     [7] = {0, 2, 4, 5, 7, 9, 10};
	int scale_NATURAL_MINOR  [7] = {0, 2, 3, 5, 7, 8, 10};
	int scale_PENTATONIC     [5] = {0, 2, 4, 7, 9};
	int scale_PHRYGIAN       [7] = {0, 1, 3, 5, 7, 8, 10};
	int scale_TURKISH        [7] = {0, 1, 3, 5, 7, 10, 11};

	std::vector<ParamWidget*> seqKnobs;
	GridSeqWidget();
	~GridSeqWidget();
	void randomize();
	Menu *createContextMenu();
};

struct RMSWidget : ModuleWidget {
	RMSWidget();
};

struct SmallWhiteKnob : RoundKnob {
	SmallWhiteKnob() {
		setSVG(SVG::load(assetPlugin(plugin, "res/SmallWhiteKnob.svg")));
	}
};

struct MyBlueValueLight : ColorValueLight {
	MyBlueValueLight() {
		baseColor = nvgRGB(25, 150, 252);
	}
};

