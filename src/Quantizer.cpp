#include "JWModules.hpp"

struct Quantizer : Module,QuantizeUtils {
	enum ParamIds {
		ROOT_NOTE_PARAM,
		SCALE_PARAM,
		OCTAVE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		NOTE_INPUT,
		SCALE_INPUT,
		VOLT_INPUT,
		OCTAVE_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		VOLT_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	Quantizer() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(ROOT_NOTE_PARAM, 0.0, QuantizeUtils::NUM_NOTES-1, QuantizeUtils::NOTE_C, "Root Note");
		configParam(SCALE_PARAM, 0.0, QuantizeUtils::NUM_SCALES-1, QuantizeUtils::MINOR, "Scale");
		configParam(OCTAVE_PARAM, -5, 5 , 0, "Octave Shift");
	}

	void process(const ProcessArgs &args) override;

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		return rootJ;
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// STEP
///////////////////////////////////////////////////////////////////////////////////////////////////
void Quantizer::process(const ProcessArgs &args) {
	int rootNote = params[ROOT_NOTE_PARAM].getValue() + rescalefjw(inputs[NOTE_INPUT].getVoltage(), 0, 10, 0, QuantizeUtils::NUM_NOTES-1);
	int scale = params[SCALE_PARAM].getValue() + rescalefjw(inputs[SCALE_INPUT].getVoltage(), 0, 10, 0, QuantizeUtils::NUM_SCALES-1);
	int octaveShift = params[OCTAVE_PARAM].getValue() + clampfjw(inputs[OCTAVE_INPUT].getVoltage(), -5, 5);
	int channels = inputs[VOLT_INPUT].getChannels();
	for (int c = 0; c < channels; c++) {
		float volts = closestVoltageInScale(inputs[VOLT_INPUT].getVoltage(c), rootNote, scale);
		outputs[VOLT_OUTPUT].setVoltage(volts + octaveShift, c);
	}
	outputs[VOLT_OUTPUT].setChannels(channels);
}

struct QuantizerWidget : ModuleWidget { 
	QuantizerWidget(Quantizer *module); 
};

QuantizerWidget::QuantizerWidget(Quantizer *module) {
		setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*4, RACK_GRID_HEIGHT);

	{
		SVGPanel *panel = new SVGPanel();
		panel->box.size = box.size;
		panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/WavHeadPanel.svg")));
		addChild(panel);
	}

	addChild(createWidget<Screw_J>(Vec(16, 2)));
	addChild(createWidget<Screw_J>(Vec(16, 365)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 2)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));

	CenteredLabel* const titleLabel = new CenteredLabel;
	titleLabel->box.pos = Vec(15, 15);
	titleLabel->text = "Quantizer";
	addChild(titleLabel);

	///// NOTE AND SCALE CONTROLS /////
	NoteKnob *noteKnob = dynamic_cast<NoteKnob*>(createParam<NoteKnob>(Vec(17, 60), module, Quantizer::ROOT_NOTE_PARAM));
	CenteredLabel* const noteLabel = new CenteredLabel;
	noteLabel->box.pos = Vec(15, 29);
	noteLabel->text = "C";
	noteKnob->connectLabel(noteLabel, module);
	addChild(noteLabel);
	addParam(noteKnob);
	addInput(createInput<TinyPJ301MPort>(Vec(23, 90), module, Quantizer::NOTE_INPUT));

	ScaleKnob *scaleKnob = dynamic_cast<ScaleKnob*>(createParam<ScaleKnob>(Vec(17, 133), module, Quantizer::SCALE_PARAM));
	CenteredLabel* const scaleLabel = new CenteredLabel;
	scaleLabel->box.pos = Vec(15, 65);
	scaleLabel->text = "Minor";
	scaleKnob->connectLabel(scaleLabel, module);
	addChild(scaleLabel);
	addParam(scaleKnob);
	addInput(createInput<TinyPJ301MPort>(Vec(23, 163), module, Quantizer::SCALE_INPUT));

	addParam(createParam<JwSmallSnapKnob>(Vec(17, 205), module, Quantizer::OCTAVE_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(23, 235), module, Quantizer::OCTAVE_INPUT));


	addInput(createInput<TinyPJ301MPort>(Vec(10, 290), module, Quantizer::VOLT_INPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(35, 290), module, Quantizer::VOLT_OUTPUT));

	CenteredLabel* const octLabel = new CenteredLabel;
	octLabel->box.pos = Vec(15, 101.5);
	octLabel->text = "Oct Shift";
	addChild(octLabel);

	CenteredLabel* const voctLabel = new CenteredLabel;
	voctLabel->box.pos = Vec(15, 142);
	voctLabel->text = "V/Oct";
	addChild(voctLabel);

	CenteredLabel* const inLabel = new CenteredLabel;
	inLabel->box.pos = Vec(8, 159);
	inLabel->text = "In";
	addChild(inLabel);

	CenteredLabel* const outLabel = new CenteredLabel;
	outLabel->box.pos = Vec(22, 159);
	outLabel->text = "Out";
	addChild(outLabel);
}

Model *modelQuantizer = createModel<Quantizer, QuantizerWidget>("Quantizer");
