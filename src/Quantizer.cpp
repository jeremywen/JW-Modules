#include "JWModules.hpp"

struct Quantizer : Module,QuantizeUtils {
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

	Quantizer() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {}

	void step() override;

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		return rootJ;
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// STEP
///////////////////////////////////////////////////////////////////////////////////////////////////
void Quantizer::step() {
	int rootNote = params[ROOT_NOTE_PARAM].value + rescalefjw(inputs[NOTE_INPUT].value, 0, 10, 0, QuantizeUtils::NUM_NOTES-1);
	int scale = params[SCALE_PARAM].value + rescalefjw(inputs[SCALE_INPUT].value, 0, 10, 0, QuantizeUtils::NUM_SCALES-1);
	outputs[VOLT_OUTPUT].value = closestVoltageInScale(inputs[VOLT_INPUT].value, rootNote, scale);
}

struct QuantizerWidget : ModuleWidget { 
	QuantizerWidget(Quantizer *module); 
};

QuantizerWidget::QuantizerWidget(Quantizer *module) : ModuleWidget(module) {
	box.size = Vec(RACK_GRID_WIDTH*4, RACK_GRID_HEIGHT);

	{
		SVGPanel *panel = new SVGPanel();
		panel->box.size = box.size;
		panel->setBackground(SVG::load(assetPlugin(pluginInstance, "res/WavHeadPanel.svg")));
		addChild(panel);
	}

	addChild(createWidget<Screw_J>(Vec(16, 1)));
	addChild(createWidget<Screw_J>(Vec(16, 365)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 1)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));

	CenteredLabel* const titleLabel = new CenteredLabel;
	titleLabel->box.pos = Vec(15, 15);
	titleLabel->text = "Quantizer";
	addChild(titleLabel);

	//TODO add labels to all params

	///// NOTE AND SCALE CONTROLS /////
	NoteKnob *noteKnob = dynamic_cast<NoteKnob*>(createParam<NoteKnob>(Vec(17, 78), module, Quantizer::ROOT_NOTE_PARAM, 0.0, QuantizeUtils::NUM_NOTES-1, QuantizeUtils::NOTE_C));
	CenteredLabel* const noteLabel = new CenteredLabel;
	noteLabel->box.pos = Vec(15, 35);
	noteLabel->text = "note here";
	noteKnob->connectLabel(noteLabel, module);
	addChild(noteLabel);
	addParam(noteKnob);
	addInput(createPort<TinyPJ301MPort>(Vec(23, 110), PortWidget::INPUT, module, Quantizer::NOTE_INPUT));

	ScaleKnob *scaleKnob = dynamic_cast<ScaleKnob*>(createParam<ScaleKnob>(Vec(17, 188), module, Quantizer::SCALE_PARAM, 0.0, QuantizeUtils::NUM_SCALES-1, QuantizeUtils::MINOR));
	CenteredLabel* const scaleLabel = new CenteredLabel;
	scaleLabel->box.pos = Vec(15, 90);
	scaleLabel->text = "scale here";
	scaleKnob->connectLabel(scaleLabel, module);
	addChild(scaleLabel);
	addParam(scaleKnob);
	addInput(createPort<TinyPJ301MPort>(Vec(23, 220), PortWidget::INPUT, module, Quantizer::SCALE_INPUT));


	addInput(createPort<TinyPJ301MPort>(Vec(10, 290), PortWidget::INPUT, module, Quantizer::VOLT_INPUT));
	addOutput(createPort<TinyPJ301MPort>(Vec(35, 290), PortWidget::OUTPUT, module, Quantizer::VOLT_OUTPUT));

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

Model *modelQuantizer = createModel<Quantizer, QuantizerWidget>("Quantizer");
