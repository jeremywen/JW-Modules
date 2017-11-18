#include "JWModules.hpp"
#include "dsp/digital.hpp"

struct Quantizer : Module {
	enum ParamIds {
		ROOT_NOTE_PARAM,
		SCALE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		NOTE_INPUT,
		SCALE_INPUT,
		VOLT_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		VOLT_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};


	//copied from http://www.grantmuller.com/MidiReference/doc/midiReference/ScaleReference.html
	int SCALE_AEOLIAN        [7] = {0, 2, 3, 5, 7, 8, 10};
	int SCALE_BLUES          [9] = {0, 2, 3, 4, 5, 7, 9, 10, 11};
	int SCALE_CHROMATIC      [12]= {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
	int SCALE_DIATONIC_MINOR [7] = {0, 2, 3, 5, 7, 8, 10};
	int SCALE_DORIAN         [7] = {0, 2, 3, 5, 7, 9, 10};
	int SCALE_HARMONIC_MINOR [7] = {0, 2, 3, 5, 7, 8, 11};
	int SCALE_INDIAN         [7] = {0, 1, 1, 4, 5, 8, 10};
	int SCALE_LOCRIAN        [7] = {0, 1, 3, 5, 6, 8, 10};
	int SCALE_LYDIAN         [7] = {0, 2, 4, 6, 7, 9, 10};
	int SCALE_MAJOR          [7] = {0, 2, 4, 5, 7, 9, 11};
	int SCALE_MELODIC_MINOR  [9] = {0, 2, 3, 5, 7, 8, 9, 10, 11};
	int SCALE_MINOR          [7] = {0, 2, 3, 5, 7, 8, 10};
	int SCALE_MIXOLYDIAN     [7] = {0, 2, 4, 5, 7, 9, 10};
	int SCALE_NATURAL_MINOR  [7] = {0, 2, 3, 5, 7, 8, 10};
	int SCALE_PENTATONIC     [5] = {0, 2, 4, 7, 9};
	int SCALE_PHRYGIAN       [7] = {0, 1, 3, 5, 7, 8, 10};
	int SCALE_TURKISH        [7] = {0, 1, 3, 5, 7, 10, 11};


	Quantizer() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {}

	void step();

	json_t *toJson() {
		json_t *rootJ = json_object();
		return rootJ;
	}

	void fromJson(json_t *rootJ) {
	}

	void reset() {
	}

	void randomize() {
	}

	float closestVoltageInScale(float voltsIn){
		int rootNote = params[ROOT_NOTE_PARAM].value + rescalef(inputs[NOTE_INPUT].value, 0, 10, 0, QuantizerWidget::NUM_NOTES);
		int curScaleVal = params[SCALE_PARAM].value + rescalef(inputs[SCALE_INPUT].value, 0, 10, 0, QuantizerWidget::NUM_SCALES);
		int *curScaleArr;
		int notesInScale = 0;
		switch(curScaleVal){
			case QuantizerWidget::AEOLIAN:        curScaleArr = SCALE_AEOLIAN;       notesInScale=LENGTHOF(SCALE_AEOLIAN); break;
			case QuantizerWidget::BLUES:          curScaleArr = SCALE_BLUES;         notesInScale=LENGTHOF(SCALE_BLUES); break;
			case QuantizerWidget::CHROMATIC:      curScaleArr = SCALE_CHROMATIC;     notesInScale=LENGTHOF(SCALE_CHROMATIC); break;
			case QuantizerWidget::DIATONIC_MINOR: curScaleArr = SCALE_DIATONIC_MINOR;notesInScale=LENGTHOF(SCALE_DIATONIC_MINOR); break;
			case QuantizerWidget::DORIAN:         curScaleArr = SCALE_DORIAN;        notesInScale=LENGTHOF(SCALE_DORIAN); break;
			case QuantizerWidget::HARMONIC_MINOR: curScaleArr = SCALE_HARMONIC_MINOR;notesInScale=LENGTHOF(SCALE_HARMONIC_MINOR); break;
			case QuantizerWidget::INDIAN:         curScaleArr = SCALE_INDIAN;        notesInScale=LENGTHOF(SCALE_INDIAN); break;
			case QuantizerWidget::LOCRIAN:        curScaleArr = SCALE_LOCRIAN;       notesInScale=LENGTHOF(SCALE_LOCRIAN); break;
			case QuantizerWidget::LYDIAN:         curScaleArr = SCALE_LYDIAN;        notesInScale=LENGTHOF(SCALE_LYDIAN); break;
			case QuantizerWidget::MAJOR:          curScaleArr = SCALE_MAJOR;         notesInScale=LENGTHOF(SCALE_MAJOR); break;
			case QuantizerWidget::MELODIC_MINOR:  curScaleArr = SCALE_MELODIC_MINOR; notesInScale=LENGTHOF(SCALE_MELODIC_MINOR); break;
			case QuantizerWidget::MINOR:          curScaleArr = SCALE_MINOR;         notesInScale=LENGTHOF(SCALE_MINOR); break;
			case QuantizerWidget::MIXOLYDIAN:     curScaleArr = SCALE_MIXOLYDIAN;    notesInScale=LENGTHOF(SCALE_MIXOLYDIAN); break;
			case QuantizerWidget::NATURAL_MINOR:  curScaleArr = SCALE_NATURAL_MINOR; notesInScale=LENGTHOF(SCALE_NATURAL_MINOR); break;
			case QuantizerWidget::PENTATONIC:     curScaleArr = SCALE_PENTATONIC;    notesInScale=LENGTHOF(SCALE_PENTATONIC); break;
			case QuantizerWidget::PHRYGIAN:       curScaleArr = SCALE_PHRYGIAN;      notesInScale=LENGTHOF(SCALE_PHRYGIAN); break;
			case QuantizerWidget::TURKISH:        curScaleArr = SCALE_TURKISH;       notesInScale=LENGTHOF(SCALE_TURKISH); break;
			case QuantizerWidget::NONE:           return voltsIn;
		}

		float closestVal = 10.0;
		float closestDist = 10.0;
		int octaveInVolts = int(voltsIn);
		for (int i = 0; i < notesInScale; i++) {
			float scaleNoteInVolts = octaveInVolts + ((rootNote + curScaleArr[i]) / 12.0);
			float distAway = fabs(voltsIn - scaleNoteInVolts);
			if(distAway < closestDist){
				closestVal = scaleNoteInVolts;
				closestDist = distAway;
			}
		}
		return closestVal;
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// STEP
///////////////////////////////////////////////////////////////////////////////////////////////////
void Quantizer::step() {
	outputs[VOLT_OUTPUT].value = closestVoltageInScale(inputs[VOLT_INPUT].value);
}

QuantizerWidget::QuantizerWidget() {
	Quantizer *module = new Quantizer();
	setModule(module);
	box.size = Vec(15*4, 380);

	{
		SVGPanel *panel = new SVGPanel();
		panel->box.size = box.size;
		panel->setBackground(SVG::load(assetPlugin(plugin, "res/WavHeadPanel.svg")));
		addChild(panel);
	}

	addChild(createScrew<Screw_J>(Vec(15, 0)));
	addChild(createScrew<Screw_J>(Vec(15, 365)));
	addChild(createScrew<Screw_W>(Vec(box.size.x-30, 0)));
	addChild(createScrew<Screw_W>(Vec(box.size.x-30, 365)));

	CenteredLabel* const titleLabel = new CenteredLabel;
	titleLabel->box.pos = Vec(15, 15);
	titleLabel->text = "Quantizer";
	addChild(titleLabel);

	///// NOTE AND SCALE CONTROLS /////
	NoteKnob *noteKnob = dynamic_cast<NoteKnob*>(createParam<NoteKnob>(Vec(19, 80), module, Quantizer::ROOT_NOTE_PARAM, 0.0, NUM_NOTES-1, NOTE_C));
	CenteredLabel* const noteLabel = new CenteredLabel;
	noteLabel->box.pos = Vec(15, 35);
	noteLabel->text = "note here";
	noteKnob->connectLabel(noteLabel);
	addChild(noteLabel);
	addParam(noteKnob);
	addInput(createInput<TinyPJ301MPort>(Vec(23, 110), module, Quantizer::NOTE_INPUT));

	ScaleKnob *scaleKnob = dynamic_cast<ScaleKnob*>(createParam<ScaleKnob>(Vec(19, 190), module, Quantizer::SCALE_PARAM, 0.0, NUM_SCALES-1, MINOR));
	CenteredLabel* const scaleLabel = new CenteredLabel;
	scaleLabel->box.pos = Vec(15, 90);
	scaleLabel->text = "scale here";
	scaleKnob->connectLabel(scaleLabel);
	addChild(scaleLabel);
	addParam(scaleKnob);
	addInput(createInput<TinyPJ301MPort>(Vec(23, 220), module, Quantizer::SCALE_INPUT));


	addInput(createInput<TinyPJ301MPort>(Vec(10, 290), module, Quantizer::VOLT_INPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(35, 290), module, Quantizer::VOLT_OUTPUT));

	CenteredLabel* const voctLabel = new CenteredLabel;
	voctLabel->box.pos = Vec(15, 140);
	voctLabel->text = "V/OCT";
	addChild(voctLabel);

	CenteredLabel* const inLabel = new CenteredLabel;
	inLabel->box.pos = Vec(8, 160);
	inLabel->text = "In";
	addChild(inLabel);

	CenteredLabel* const outLabel = new CenteredLabel;
	outLabel->box.pos = Vec(22, 160);
	outLabel->text = "Out";
	addChild(outLabel);
}
