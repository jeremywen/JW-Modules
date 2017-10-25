#include "rack.hpp"

using namespace rack;
extern Plugin *plugin;

struct SimpleClockWidget : ModuleWidget {
	SimpleClockWidget();
};

struct RMSWidget : ModuleWidget {
	RMSWidget();
};

struct GridSeqWidget : ModuleWidget {
	std::vector<ParamWidget*> seqKnobs;
	GridSeqWidget();
	~GridSeqWidget(){ seqKnobs.clear(); }
	Menu *createContextMenu();
	// void draw(NVGcontext *vg);

	enum Notes {
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
		NONE,
		NUM_SCALES
	};
};

struct SmallWhiteKnob : RoundKnob {
	SmallWhiteKnob() {
		setSVG(SVG::load(assetPlugin(plugin, "res/SmallWhiteKnob.svg")));
	}
	rack::Label* linkedLabel = nullptr;
	
	void connectLabel(rack::Label* label) {
		linkedLabel = label;
		if (linkedLabel) {
			linkedLabel->text = formatCurrentValue();
		}
	}

	void onChange() override {
		RoundKnob::onChange();
		if (linkedLabel) {
			linkedLabel->text = formatCurrentValue();
		}
	}

	virtual std::string formatCurrentValue() {
		return std::to_string(static_cast<unsigned int>(value));
	}
};

struct MyBlueValueLight : ColorValueLight {
	MyBlueValueLight() {
		baseColor = nvgRGB(25, 150, 252);
	}
};

struct NoteKnob : SmallWhiteKnob {
	std::string formatCurrentValue() override {
		switch(int(value)){
			case GridSeqWidget::NOTE_C:       return "C";
			case GridSeqWidget::NOTE_C_SHARP: return "C#";
			case GridSeqWidget::NOTE_D:       return "D";
			case GridSeqWidget::NOTE_D_SHARP: return "D#";
			case GridSeqWidget::NOTE_E:       return "E";
			case GridSeqWidget::NOTE_F:       return "F";
			case GridSeqWidget::NOTE_F_SHARP: return "F#";
			case GridSeqWidget::NOTE_G:       return "G";
			case GridSeqWidget::NOTE_G_SHARP: return "G#";
			case GridSeqWidget::NOTE_A:       return "A";
			case GridSeqWidget::NOTE_A_SHARP: return "A#";
			case GridSeqWidget::NOTE_B:       return "B";
			default: return "";
		}
	}
};

struct ScaleKnob : SmallWhiteKnob {
	std::string formatCurrentValue() override {
		switch(int(value)){
			case GridSeqWidget::AEOLIAN:        return "Aeolian";
			case GridSeqWidget::BLUES:          return "Blues";
			case GridSeqWidget::CHROMATIC:      return "Chromatic";
			case GridSeqWidget::DIATONIC_MINOR: return "Diatonic Minor";
			case GridSeqWidget::DORIAN:         return "Dorian";
			case GridSeqWidget::HARMONIC_MINOR: return "Harmonic Minor";
			case GridSeqWidget::INDIAN:         return "Indian";
			case GridSeqWidget::LOCRIAN:        return "Locrian";
			case GridSeqWidget::LYDIAN:         return "Lydian";
			case GridSeqWidget::MAJOR:          return "Major";
			case GridSeqWidget::MELODIC_MINOR:  return "Melodic Minor";
			case GridSeqWidget::MINOR:          return "Minor";
			case GridSeqWidget::MIXOLYDIAN:     return "Mixolydian";
			case GridSeqWidget::NATURAL_MINOR:  return "Natural Minor";
			case GridSeqWidget::PENTATONIC:     return "Pentatonic";
			case GridSeqWidget::PHRYGIAN:       return "Phrygian";
			case GridSeqWidget::TURKISH:        return "Turkish";
			case GridSeqWidget::NONE:           return "None";
			default: return "";
		}
	}
};



