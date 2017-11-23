#include "JWModules.hpp"
#include "dsp/digital.hpp"

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

	void step();

	json_t *toJson() {
		json_t *rootJ = json_object();
		return rootJ;
	}

	void fromJson(json_t *rootJ) {}
	void reset() {}
	void randomize() {}
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// STEP
///////////////////////////////////////////////////////////////////////////////////////////////////
void Quantizer::step() {
	int rootNote = params[ROOT_NOTE_PARAM].value + rescalef(inputs[NOTE_INPUT].value, 0, 10, 0, QuantizeUtils::NUM_NOTES-1);
	int scale = params[SCALE_PARAM].value + rescalef(inputs[SCALE_INPUT].value, 0, 10, 0, QuantizeUtils::NUM_SCALES-1);
	outputs[VOLT_OUTPUT].value = closestVoltageInScale(inputs[VOLT_INPUT].value, rootNote, scale);
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
	NoteKnob *noteKnob = dynamic_cast<NoteKnob*>(createParam<NoteKnob>(Vec(17, 78), module, Quantizer::ROOT_NOTE_PARAM, 0.0, QuantizeUtils::NUM_NOTES-1, QuantizeUtils::NOTE_C));
	CenteredLabel* const noteLabel = new CenteredLabel;
	noteLabel->box.pos = Vec(15, 35);
	noteLabel->text = "note here";
	noteKnob->connectLabel(noteLabel);
	addChild(noteLabel);
	addParam(noteKnob);
	addInput(createInput<TinyPJ301MPort>(Vec(23, 110), module, Quantizer::NOTE_INPUT));

	ScaleKnob *scaleKnob = dynamic_cast<ScaleKnob*>(createParam<ScaleKnob>(Vec(17, 188), module, Quantizer::SCALE_PARAM, 0.0, QuantizeUtils::NUM_SCALES-1, QuantizeUtils::MINOR));
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
