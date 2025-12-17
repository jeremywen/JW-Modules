#include "JWModules.hpp"

struct Quantizer : Module, QuantizeUtils {
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
	
	int scale = 0;
	int rootNote = 0;
	int octaveShift = 0;

	Quantizer() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(ROOT_NOTE_PARAM, 0.0, QuantizeUtils::NUM_NOTES-1, QuantizeUtils::NOTE_C, "Root Note");
		configParam(SCALE_PARAM, 0.0, QuantizeUtils::NUM_SCALES-1, QuantizeUtils::MINOR, "Scale");
		configParam(OCTAVE_PARAM, -5, 5 , 0, "Octave Shift");
		configInput(NOTE_INPUT, "Note");
		configInput(SCALE_INPUT, "Scale");
		configInput(VOLT_INPUT, "Voltage");
		configInput(OCTAVE_INPUT, "Octave");
		configOutput(VOLT_OUTPUT, "Quantized");
		configBypass(VOLT_INPUT, VOLT_OUTPUT);
	}

	void process(const ProcessArgs &args) override;

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "inputsOverride", json_boolean(inputsOverride));
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		json_t *inputsOverrideJ = json_object_get(rootJ, "inputsOverride");
		if (inputsOverrideJ){ inputsOverride = json_is_true(inputsOverrideJ); }
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// STEP
///////////////////////////////////////////////////////////////////////////////////////////////////
void Quantizer::process(const ProcessArgs &args) {
	int inputRootNote = rescalefjw(inputs[NOTE_INPUT].getVoltage(), 0, 10, 0, QuantizeUtils::NUM_NOTES-1);
	if(inputs[NOTE_INPUT].isConnected() && inputsOverride){
		rootNote = inputRootNote;
	} else {
		rootNote = params[ROOT_NOTE_PARAM].getValue() + inputRootNote;
	}

	int inputScale = rescalefjw(inputs[SCALE_INPUT].getVoltage(), 0, 10, 0, QuantizeUtils::NUM_SCALES-1);
	if(inputs[SCALE_INPUT].isConnected() && inputsOverride){
		scale = inputScale;
	} else {
		scale = params[SCALE_PARAM].getValue() + inputScale;
	}

	int inputOctaveShift = clampfjw(inputs[OCTAVE_INPUT].getVoltage(), -5, 5);
	if(inputs[OCTAVE_INPUT].isConnected() && inputsOverride){
		octaveShift = inputOctaveShift;
	} else {
		octaveShift = params[OCTAVE_PARAM].getValue() + inputOctaveShift;
	}
	
	int channels = inputs[VOLT_INPUT].getChannels();
	for (int c = 0; c < channels; c++) {
		float volts = closestVoltageInScale(inputs[VOLT_INPUT].getVoltage(c), rootNote, scale, true);
		outputs[VOLT_OUTPUT].setVoltage(volts + octaveShift, c);
	}
	outputs[VOLT_OUTPUT].setChannels(channels);
}

struct QuantizerWidget : ModuleWidget {
	NoteKnob *noteKnob;
	ScaleKnob *scaleKnob;
	JwSmallSnapKnob *octaveKnob;
	QuantizerWidget(Quantizer *module); 
	void step() override;
	void appendContextMenu(Menu *menu) override;
};

void QuantizerWidget::step() {
	ModuleWidget::step();
	Quantizer *qMod = dynamic_cast<Quantizer*>(module);
	if(qMod && qMod->inputsOverride){
		if(qMod->inputs[Quantizer::NOTE_INPUT].isConnected()){
			noteKnob->getParamQuantity()->setValue(qMod->rootNote);
			noteKnob->step();
		}
		if(qMod->inputs[Quantizer::SCALE_INPUT].isConnected()){
			scaleKnob->getParamQuantity()->setValue(qMod->scale);
			scaleKnob->step();
		}
		if(qMod->inputs[Quantizer::OCTAVE_INPUT].isConnected()){
			octaveKnob->getParamQuantity()->setValue(qMod->octaveShift);
			octaveKnob->step();
		}
	}
}


QuantizerWidget::QuantizerWidget(Quantizer *module) {
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*4, RACK_GRID_HEIGHT);
	
	setPanel(createPanel(
		asset::plugin(pluginInstance, "res/Quantizer.svg"), 
		asset::plugin(pluginInstance, "res/Quantizer.svg")
	));

	addChild(createWidget<Screw_J>(Vec(16, 2)));
	addChild(createWidget<Screw_J>(Vec(16, 365)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 2)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));

	///// NOTE AND SCALE CONTROLS /////
	noteKnob = dynamic_cast<NoteKnob*>(createParam<NoteKnob>(Vec(17, 60), module, Quantizer::ROOT_NOTE_PARAM));
	CenteredLabel* const noteLabel = new CenteredLabel;
	noteLabel->box.pos = Vec(15, 29);
	noteLabel->text = "C";
	noteKnob->connectLabel(noteLabel, module);
	addChild(noteLabel);
	addParam(noteKnob);
	addInput(createInput<TinyPJ301MPort>(Vec(23, 90), module, Quantizer::NOTE_INPUT));

	scaleKnob = dynamic_cast<ScaleKnob*>(createParam<ScaleKnob>(Vec(17, 133), module, Quantizer::SCALE_PARAM));
	CenteredLabel* const scaleLabel = new CenteredLabel;
	scaleLabel->box.pos = Vec(15, 65);
	scaleLabel->text = "Minor";
	scaleKnob->connectLabel(scaleLabel, module);
	addChild(scaleLabel);
	addParam(scaleKnob);
	addInput(createInput<TinyPJ301MPort>(Vec(23, 163), module, Quantizer::SCALE_INPUT));

	octaveKnob = createParam<JwSmallSnapKnob>(Vec(17, 205), module, Quantizer::OCTAVE_PARAM);
	addParam(octaveKnob);
	addInput(createInput<TinyPJ301MPort>(Vec(23, 235), module, Quantizer::OCTAVE_INPUT));

	addInput(createInput<TinyPJ301MPort>(Vec(10, 290), module, Quantizer::VOLT_INPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(35, 290), module, Quantizer::VOLT_OUTPUT));
}

void QuantizerWidget::appendContextMenu(Menu *menu) {
	MenuLabel *spacerLabel = new MenuLabel();
	menu->addChild(spacerLabel);
	
	Quantizer *quantizer = dynamic_cast<Quantizer*>(module);

	InputsOverrideItem *inputsOverrideItem = new InputsOverrideItem();
	inputsOverrideItem->text = "Inputs Override Knobs";
	inputsOverrideItem->quantizeUtils = quantizer;
	menu->addChild(inputsOverrideItem);
}
Model *modelQuantizer = createModel<Quantizer, QuantizerWidget>("Quantizer");
