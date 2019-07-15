#include "JWModules.hpp"

struct BlankPanel : Module {
	BlankPanel(){
		config(0, 0, 0, 0);
	}
};
struct BlankPanelSmallWidget : ModuleWidget { BlankPanelSmallWidget(Module *module); };
struct BlankPanelMediumWidget : ModuleWidget { BlankPanelMediumWidget(Module *module); };
struct BlankPanelLargeWidget : ModuleWidget { BlankPanelLargeWidget(Module *module); };
struct BlankPanelCBWidget : ModuleWidget { BlankPanelCBWidget(Module *module); };

BlankPanelSmallWidget::BlankPanelSmallWidget(Module *module) {
	setModule(module);
	box.size = Vec(15*3, 380);

	SVGPanel *panel = new SVGPanel();
	panel->box.size = box.size;
	panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/BlankPanelSmall.svg")));
	addChild(panel);

	addChild(createWidget<Screw_J>(Vec(16, 0)));
	addChild(createWidget<Screw_W>(Vec(16, 365)));
}

BlankPanelMediumWidget::BlankPanelMediumWidget(Module *module) {
	setModule(module);
	box.size = Vec(15*6, 380);

	SVGPanel *panel = new SVGPanel();
	panel->box.size = box.size;
	panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/BlankPanelMedium.svg")));
	addChild(panel);

	addChild(createWidget<Screw_J>(Vec(16, 0)));
	addChild(createWidget<Screw_J>(Vec(16, 365)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 0)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));
}

BlankPanelLargeWidget::BlankPanelLargeWidget(Module *module) {
	setModule(module);
	box.size = Vec(15*12, 380);

	SVGPanel *panel = new SVGPanel();
	panel->box.size = box.size;
	panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/BlankPanelLarge.svg")));
	addChild(panel);

	addChild(createWidget<Screw_J>(Vec(16, 0)));
	addChild(createWidget<Screw_J>(Vec(16, 365)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 0)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));
}

BlankPanelCBWidget::BlankPanelCBWidget(Module *module) {
	setModule(module);
	box.size = Vec(15*6, 380);

	SVGPanel *panel = new SVGPanel();
	panel->box.size = box.size;
	panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/CoolBreeze.svg")));
	addChild(panel);

	addChild(createWidget<Screw_J>(Vec(16, 0)));
	addChild(createWidget<Screw_J>(Vec(16, 365)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 0)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));
}

Model *modelBlankPanelSmall = createModel<Module, BlankPanelSmallWidget>("BlankPanel_SM");
Model *modelBlankPanelMedium = createModel<Module, BlankPanelMediumWidget>("BlankPanel_MD");
Model *modelBlankPanelLarge = createModel<Module, BlankPanelLargeWidget>("BlankPanel_LG");
Model *modelCoolBreeze = createModel<Module, BlankPanelCBWidget>("CoolBreeze");
