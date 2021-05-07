#include "JWModules.hpp"

struct WavHead : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		VOLT_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};
	
	bool invert = true;
	bool neg5ToPos5 = false;
	bool snowMode = false;
	WavHead() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
	}
	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "invert", json_boolean(invert));
		json_object_set_new(rootJ, "neg5ToPos5", json_boolean(neg5ToPos5));
		json_object_set_new(rootJ, "snowMode", json_boolean(snowMode));
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		json_t *invertJ = json_object_get(rootJ, "invert");
		if (invertJ){ invert = json_is_true(invertJ); }

		json_t *neg5ToPos5J = json_object_get(rootJ, "neg5ToPos5");
		if (neg5ToPos5J){ neg5ToPos5 = json_is_true(neg5ToPos5J); }

		json_t *snowModeJ = json_object_get(rootJ, "snowMode");
		if (snowModeJ){ snowMode = json_is_true(snowModeJ); }
	}

};

struct WavHeadWidget : ModuleWidget {
	WavHeadWidget(WavHead *module);
	void step() override;
	Widget* widgetToMove[16];
	Widget* snowflakesArr[10];
	void appendContextMenu(Menu *menu) override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SvgLightWidget : LightWidget {
	std::shared_ptr<Svg> svg;

	/** Sets the box size to the svg image size */
	void wrap();

	/** Sets and wraps the SVG */
	void setSvg(std::shared_ptr<Svg> svg);
	DEPRECATED void setSVG(std::shared_ptr<Svg> svg) {
		setSvg(svg);
	}

	void draw(const DrawArgs& args) override;
};
void SvgLightWidget::wrap() {
	if (svg && svg->handle) {
		box.size = math::Vec(svg->handle->width, svg->handle->height);
	}
	else {
		box.size = math::Vec();
	}
}

void SvgLightWidget::setSvg(std::shared_ptr<Svg> svg) {
	this->svg = svg;
	wrap();
}

void SvgLightWidget::draw(const DrawArgs& args) {
	if (svg && svg->handle) {
		svgDraw(args.vg, svg->handle);
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct Snowflake : SvgLightWidget {
	Snowflake() {
		setSVG(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SnowFlake.svg")));
		// box.size = sw->box.size;
	}
};

struct WavHeadLogo : SvgLightWidget {
	WavHeadLogo() {
		setSVG(APP->window->loadSvg(asset::plugin(pluginInstance, "res/WavHeadSmall.svg")));
		// box.size = sw->box.size;
	}
};

void WavHeadWidget::step() {
	if(module != NULL){
		ModuleWidget::step();
		WavHead *wavHead = dynamic_cast<WavHead*>(module);
		float minVolts = wavHead->neg5ToPos5 ? -5 : 0;
		float maxVolts = minVolts + 10;
		float minY = wavHead->invert ? 250 : 15;
		float maxY = wavHead->invert ? 15 : 250;

		for(int i=0; i<16; i++){
			widgetToMove[i]->visible = false;
		}

		int channels = module->inputs[WavHead::VOLT_INPUT].getChannels();
		if(!module->inputs[WavHead::VOLT_INPUT].isConnected() || channels == 0){
			widgetToMove[0]->visible = true;
		}
		for(int c=0; c<channels; c++){
			float clamped = clampfjw(module->inputs[WavHead::VOLT_INPUT].getVoltage(c), minVolts, maxVolts);
			widgetToMove[c]->box.pos.y = rescalefjw(clamped, minVolts, maxVolts, minY, maxY);
			widgetToMove[c]->visible = true;
		}

		if(wavHead->snowMode){
			for(int i=0; i<10; i++){
				if(snowflakesArr[i]->box.pos.y > box.size.y){
					snowflakesArr[i]->box.pos.y = -random::uniform()*200-30;
				} else {
					snowflakesArr[i]->box.pos.y += random::uniform();
				}
			}
		} else {
			for(int i=0; i<10; i++){
				snowflakesArr[i]->box.pos.y = -random::uniform()*200-30;
			}
		}
	}
};

WavHeadWidget::WavHeadWidget(WavHead *module) {
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*4, RACK_GRID_HEIGHT);

	BGPanel *panel = new BGPanel(nvgRGB(230, 230, 230));
	panel->box.size = box.size;
	addChild(panel);

	for(int i=0; i<16; i++){
		widgetToMove[i] = createWidget<WavHeadLogo>(Vec(5, 250));
		addChild(widgetToMove[i]);
	}
	addChild(createWidget<Screw_J>(Vec(16, 2)));
	addChild(createWidget<Screw_J>(Vec(16, 365)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 2)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));

	for(int i=0; i<10; i++){
		snowflakesArr[i] = createWidget<Snowflake>(Vec(random::uniform()*box.size.x, -random::uniform()*200-30));
		addChild(snowflakesArr[i]);
	}

	addInput(createInput<PJ301MPort>(Vec(18, 330), module, WavHead::VOLT_INPUT));
}

struct InvertMenuItem : MenuItem {
	WavHead *wavHead;
	void onAction(const event::Action &e) override {
		wavHead->invert = !wavHead->invert;
	}
	void step() override {
		rightText = (wavHead->invert) ? "✔" : "";
		MenuItem::step();
	}
};

struct Neg5MenuItem : MenuItem {
	WavHead *wavHead;
	void onAction(const event::Action &e) override {
		wavHead->neg5ToPos5 = !wavHead->neg5ToPos5;
	}
	void step() override {
		rightText = (wavHead->neg5ToPos5) ? "✔" : "";
		MenuItem::step();
	}
};

struct SnowModeMenuItem : MenuItem {
	WavHead *wavHead;
	void onAction(const event::Action &e) override {
		wavHead->snowMode = !wavHead->snowMode;
	}
	void step() override {
		rightText = (wavHead->snowMode) ? "✔" : "";
		MenuItem::step();
	}
};

void WavHeadWidget::appendContextMenu(Menu *menu) {
	WavHead *wavHead = dynamic_cast<WavHead*>(module);

	MenuLabel *spacerLabel = new MenuLabel();
	menu->addChild(spacerLabel);

	InvertMenuItem *invertMenuItem = new InvertMenuItem();
	invertMenuItem->text = "Invert";
	invertMenuItem->wavHead = wavHead;
	menu->addChild(invertMenuItem);

	Neg5MenuItem *neg5MenuItem = new Neg5MenuItem();
	neg5MenuItem->text = "-5 to +5";
	neg5MenuItem->wavHead = wavHead;
	menu->addChild(neg5MenuItem);

	SnowModeMenuItem *snowModeMenuItem = new SnowModeMenuItem();
	snowModeMenuItem->text = "Snow Mode";
	snowModeMenuItem->wavHead = wavHead;
	menu->addChild(snowModeMenuItem);

}

Model *modelWavHead = createModel<WavHead, WavHeadWidget>("WavHead");