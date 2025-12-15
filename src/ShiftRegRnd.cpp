#include <string.h>
#include <algorithm>
#include "JWModules.hpp"

struct ShiftRegRnd : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		VOLT_INPUT,
		TRIGGER_INPUT,
		TRIGGER_RANDOMIZE,
		NUM_INPUTS
	};
	enum OutputIds {
		VOLT_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		LED_BASE,
		LED_BASE_LEN = LED_BASE + 16, // 8 lights * 2 channels (green/red)
		NUM_LIGHTS
	};
	dsp::SchmittTrigger inTrig;
	dsp::SchmittTrigger rndTrig;
	float voltages[8] = {0.f};
	int channels = 1;
	int randomIndices[16] = {0};

	ShiftRegRnd() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configInput(VOLT_INPUT, "Voltage IN");
		configInput(TRIGGER_INPUT, "Trigger IN");
		configInput(TRIGGER_RANDOMIZE, "Trigger Randomize");
		configOutput(VOLT_OUTPUT, "Voltage OUT (random from shift register)");
		configBypass(VOLT_INPUT, VOLT_OUTPUT);
		updateRandomIndices();
	}

	void updateRandomIndices() {
		for(int i=0; i<channels; i++){
			randomIndices[i] = int(random::uniform() * 8);
		}
	}

	~ShiftRegRnd() {
	}

	void onRandomize() override {
	}

	void onReset() override {
		for(int i=0; i<8; i++){
			voltages[i] = 0.f;
		}
	}

	void onSampleRateChange() override {
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "channels", json_integer(channels));
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		json_t *channelsJ = json_object_get(rootJ, "channels");
		if (channelsJ){
			channels = json_integer_value(channelsJ);
		}
	}

	void process(const ProcessArgs &args) override {
		if (rndTrig.process(inputs[TRIGGER_RANDOMIZE].getVoltage())) {
			updateRandomIndices();
			// Output random voltages from the array for each channel
			for(int i=0; i<channels; i++){
				outputs[VOLT_OUTPUT].setVoltage(voltages[randomIndices[i]], i);
			}
		}
		if (inTrig.process(inputs[TRIGGER_INPUT].getVoltage())) {
			// Shift all voltages right
			for(int i=7; i>0; i--){
				voltages[i] = voltages[i-1];
			}
			// Insert new voltage at position 0
			voltages[0] = inputs[VOLT_INPUT].getVoltage();
		}

		// Update 8 LED color lights: green for +10V, red for -10V with gradient
		for (int i = 0; i < 8; i++) {
			float v = voltages[i];
			float green = clamp(v / 10.f, 0.f, 1.f);
			float red = clamp(-v / 10.f, 0.f, 1.f);
			// Each GreenRedLight consumes two light indices: green then red
			lights[LED_BASE + i * 2 + 0].setSmoothBrightness(green, args.sampleTime);
			lights[LED_BASE + i * 2 + 1].setSmoothBrightness(red, args.sampleTime);
		}
		outputs[VOLT_OUTPUT].setChannels(channels);
	}

};

struct ShiftRegRndWidget : ModuleWidget { 
	ShiftRegRndWidget(ShiftRegRnd *module); 
	void appendContextMenu(Menu *menu) override;
};

struct SRRChannelValueItem : MenuItem {
	ShiftRegRnd *module;
	int channels;
	void onAction(const event::Action &e) override {
		module->channels = channels;
		module->updateRandomIndices();
	}
};

struct SRRChannelItem : MenuItem {
	ShiftRegRnd *module;
	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		for (int channels = 1; channels <= 16; channels++) {
			SRRChannelValueItem *item = new SRRChannelValueItem;
			if (channels == 1)
				item->text = "Monophonic";
			else
				item->text = string::f("%d", channels);
			item->rightText = CHECKMARK(module->channels == channels);
			item->module = module;
			item->channels = channels;
			menu->addChild(item);
		}
		return menu;
	}
};

ShiftRegRndWidget::ShiftRegRndWidget(ShiftRegRnd *module) {
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*3, RACK_GRID_HEIGHT);

	setPanel(createPanel(
		asset::plugin(pluginInstance, "res/ShiftRegRnd.svg"), 
		asset::plugin(pluginInstance, "res/dark/ShiftRegRnd.svg")
	));

	addChild(createWidget<Screw_J>(Vec(16, 2)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));

	addInput(createInput<PJ301MPort>(Vec(10, 70), module, ShiftRegRnd::VOLT_INPUT));
	addInput(createInput<PJ301MPort>(Vec(10, 120), module, ShiftRegRnd::TRIGGER_INPUT));
	addInput(createInput<PJ301MPort>(Vec(10, 170), module, ShiftRegRnd::TRIGGER_RANDOMIZE));
	addOutput(createOutput<PJ301MPort>(Vec(10, 220), module, ShiftRegRnd::VOLT_OUTPUT));

	// LED column showing voltage sign and magnitude (-10..+10V)
	for (int i = 0; i < 8; i++) {
		addChild(createLight<SmallLight<GreenRedLight>>(Vec(34, 80 + i * 24), module, ShiftRegRnd::LED_BASE + i * 2));
	}

}

void ShiftRegRndWidget::appendContextMenu(Menu *menu) {
	ShiftRegRnd *shiftRegRnd = dynamic_cast<ShiftRegRnd*>(module);
	MenuLabel *spacerLabel = new MenuLabel();
	menu->addChild(spacerLabel);

	SRRChannelItem *channelItem = new SRRChannelItem;
	channelItem->text = "Polyphony channels";
	channelItem->rightText = string::f("%d", shiftRegRnd->channels) + " " +RIGHT_ARROW;
	channelItem->module = shiftRegRnd;
	menu->addChild(channelItem);
}

Model *modelShiftRegRnd = createModel<ShiftRegRnd, ShiftRegRndWidget>("ShiftRegRnd");
