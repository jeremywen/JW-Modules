// #include "JWModules.hpp"

// struct Grains : Module {
// 	enum ParamIds {
// 		NUM_PARAMS
// 	};
// 	enum InputIds {
// 		NUM_INPUTS
// 	};
// 	enum OutputIds {
// 		NUM_OUTPUTS
// 	};
// 	enum LightIds {
// 		NUM_LIGHTS
// 	};

// 	Grains() {
// 		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
// 	}

// 	void process(const ProcessArgs &args) override;

// 	json_t *dataToJson() override {
// 		json_t *rootJ = json_object();

// 		return rootJ;
// 	}

// 	void dataFromJson(json_t *rootJ) override {
// 	}

// 	void onReset() override {
// 	}

// 	void onRandomize() override {
// 	}
// };
// ///////////////////////////////////////////////////////////////////////////////////////////////////
// // STEP
// ///////////////////////////////////////////////////////////////////////////////////////////////////
// void Grains::process(const ProcessArgs &args) {
// };

// struct GrainsWidget : ModuleWidget {
// 	GrainsWidget(Grains *module);
// 	~GrainsWidget(){ 
// 	}
// 	void appendContextMenu(Menu *menu) override;
// };

// GrainsWidget::GrainsWidget(Grains *module) {
// 	setModule(module);
// 	box.size = Vec(RACK_GRID_WIDTH*40, RACK_GRID_HEIGHT);

// 	setPanel(createPanel(
// 		asset::plugin(pluginInstance, "res/Grains.svg"), 
// 		asset::plugin(pluginInstance, "res/Grains.svg")
// 	));

// 	addChild(createWidget<Screw_J>(Vec(16, 2)));
// 	addChild(createWidget<Screw_J>(Vec(16, 365)));
// 	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 2)));
// 	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));

// };

// void GrainsWidget::appendContextMenu(Menu *menu) {
// 	MenuLabel *spacerLabel = new MenuLabel();
// 	menu->addChild(spacerLabel);

// 	Grains *grains = dynamic_cast<Grains*>(module);
// }



// Model *modelGrains = createModel<Grains, GrainsWidget>("Grains");
