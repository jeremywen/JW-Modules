#pragma once
#include "rack0.hpp"
#include "QuantizeUtils.cpp"

using namespace rack;
extern Plugin *pluginInstance;

////////////////////////////////////////////// PANELS //////////////////////////////////////////////

struct BGPanel : Widget {
	Widget *panelBorder;
	NVGcolor color;

	BGPanel(NVGcolor _color) {
		panelBorder = new PanelBorder;
		color = _color;
		addChild(panelBorder);
	}

	void step() override {
		panelBorder->box.size = box.size;
		Widget::step();
	}

	void draw(const DrawArgs &args) override {
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0.0, 0.0, box.size.x, box.size.y);
		nvgFillColor(args.vg, color);
		nvgFill(args.vg);
		Widget::draw(args);
	}
};

////////////////////////////////////////////// LABELS //////////////////////////////////////////////

struct CenteredLabel : Widget {
	std::string text;
	int fontSize;
	CenteredLabel(int _fontSize = 12) {
		fontSize = _fontSize;
		box.size.y = BND_WIDGET_HEIGHT;
	}
	void draw(NVGcontext *vg) override {
		nvgTextAlign(vg, NVG_ALIGN_CENTER);
		nvgFillColor(vg, nvgRGB(25, 150, 252));
		nvgFontSize(vg, fontSize);
		nvgText(vg, box.pos.x, box.pos.y, text.c_str(), NULL);
	}
};

////////////////////////////////////////////// KNOBS //////////////////////////////////////////////

struct SmallWhiteKnob : RoundKnob {
	CenteredLabel* linkedLabel = NULL;
	Module* linkedModule = NULL;

	SmallWhiteKnob() {
		setSVG(SVG::load(assetPlugin(pluginInstance, "res/SmallWhiteKnob.svg")));
	}
	
	void connectLabel(CenteredLabel* label, Module* module) {
		linkedLabel = label;
		linkedModule = module;
		if (linkedModule && linkedLabel) {
			linkedLabel->text = formatCurrentValue();
		}
	}

	void onChange(const event::Change &e) override {
		RoundKnob::onChange(e);
		if (linkedModule && linkedLabel) {
			linkedLabel->text = formatCurrentValue();
		}
	}

	virtual std::string formatCurrentValue() {
		if(paramQuantity != NULL){
			return std::to_string(static_cast<unsigned int>(paramQuantity->getValue()));
		}
	}
};

struct NoteKnob : SmallWhiteKnob {
	QuantizeUtils *quantizeUtils;
	NoteKnob(){
		snap = true;
	}
	std::string formatCurrentValue() override {
		return "";
	//TODO FIX
	// 	return quantizeUtils->noteName(int(paramQuantity->getValue()));
	}
};

struct ScaleKnob : SmallWhiteKnob {
	QuantizeUtils *quantizeUtils;
	ScaleKnob(){
		snap = true;
	}
	std::string formatCurrentValue() override {
		return "";
	//TODO FIX
	// 	return quantizeUtils->scaleName(int(paramQuantity->getValue()));
	}
};

struct JwSmallSnapKnob : SmallWhiteKnob {
	JwSmallSnapKnob() {
		snap = true;
	}
};

struct JwTinyKnob : RoundKnob {
	JwTinyKnob() {
		setSVG(SVG::load(assetPlugin(pluginInstance, "res/TinyWhiteKnob.svg")));
	}
};

struct BPMPartKnob : JwSmallSnapKnob {	
	BPMPartKnob(){} 
};

////////////////////////////////////////////// SWITCHES //////////////////////////////////////////////

struct JwHorizontalSwitch : SVGSwitch {
	JwHorizontalSwitch() {
		addFrame(SVG::load(assetPlugin(pluginInstance, "res/Switch_Horizontal_0.svg")));
		addFrame(SVG::load(assetPlugin(pluginInstance, "res/Switch_Horizontal_1.svg")));
	}
};

struct JwVerticalSwitch : SVGSwitch {
	JwVerticalSwitch() {
		addFrame(SVG::load(assetPlugin(pluginInstance, "res/Switch_Vertical_0.svg")));
		addFrame(SVG::load(assetPlugin(pluginInstance, "res/Switch_Vertical_1.svg")));
	}
};

struct BowlSwitch : SVGSwitch {
	BowlSwitch() {
		addFrame(SVG::load(assetPlugin(pluginInstance, "res/Bowl-no-food.svg")));
		addFrame(SVG::load(assetPlugin(pluginInstance, "res/Bowl-food.svg")));
	}
};

////////////////////////////////////////////// PORTS //////////////////////////////////////////////

struct TinyPJ301MPort : SvgPort {
	TinyPJ301MPort() {
		setSvg(SVG::load(assetPlugin(pluginInstance, "res/TinyPJ301M.svg")));
	}
};

struct Orange_TinyPJ301MPort : SvgPort {
	Orange_TinyPJ301MPort() {
		setSvg(SVG::load(assetPlugin(pluginInstance, "res/TinyPJ301M_orange.svg")));
	}
};

struct Yellow_TinyPJ301MPort : SvgPort {
	Yellow_TinyPJ301MPort() {
		setSvg(SVG::load(assetPlugin(pluginInstance, "res/TinyPJ301M_yellow.svg")));
	}
};

struct Purple_TinyPJ301MPort : SvgPort {
	Purple_TinyPJ301MPort() {
		setSvg(SVG::load(assetPlugin(pluginInstance, "res/TinyPJ301M_purple.svg")));
	}
};

struct Blue_TinyPJ301MPort : SvgPort {
	Blue_TinyPJ301MPort() {
		setSvg(SVG::load(assetPlugin(pluginInstance, "res/TinyPJ301M_blue.svg")));
	}
};

struct White_TinyPJ301MPort : SvgPort {
	White_TinyPJ301MPort() {
		setSvg(SVG::load(assetPlugin(pluginInstance, "res/TinyPJ301M_white.svg")));
	}
};

////////////////////////////////////////////// LIGHTS //////////////////////////////////////////////

struct MyBlueValueLight : ModuleLightWidget {
	MyBlueValueLight() {
		firstLightId = 1;
		addBaseColor(nvgRGB(25, 150, 252));
	}
};

struct MyGreenValueLight : ModuleLightWidget {
	MyGreenValueLight() {
		firstLightId = 1;
		addBaseColor(nvgRGB(0, 200, 0));
	}
};

struct MyRedValueLight : ModuleLightWidget {
	MyRedValueLight() {
		firstLightId = 1;
		addBaseColor(nvgRGB(200, 0, 0));
	}
};

////////////////////////////////////////////// BUTTONS //////////////////////////////////////////////

struct RightMoveButton : SVGSwitch {
	RightMoveButton() {
		addFrame(SVG::load(assetPlugin(pluginInstance, "res/RightButton.svg")));
		addFrame(SVG::load(assetPlugin(pluginInstance, "res/RightButtonDown.svg")));
	}
};

struct LeftMoveButton : SVGSwitch {
	LeftMoveButton() {
		addFrame(SVG::load(assetPlugin(pluginInstance, "res/LeftButton.svg")));
		addFrame(SVG::load(assetPlugin(pluginInstance, "res/LeftButtonDown.svg")));
	}
};

struct DownMoveButton : SVGSwitch {
	DownMoveButton() {
		addFrame(SVG::load(assetPlugin(pluginInstance, "res/DownButton.svg")));
		addFrame(SVG::load(assetPlugin(pluginInstance, "res/DownButtonDown.svg")));
	}
};

struct UpMoveButton : SVGSwitch {
	UpMoveButton() {
		addFrame(SVG::load(assetPlugin(pluginInstance, "res/UpButton.svg")));
		addFrame(SVG::load(assetPlugin(pluginInstance, "res/UpButtonDown.svg")));
	}
};

struct RndMoveButton : SVGSwitch {
	RndMoveButton() {
		addFrame(SVG::load(assetPlugin(pluginInstance, "res/RndButton.svg")));
		addFrame(SVG::load(assetPlugin(pluginInstance, "res/RndButtonDown.svg")));
	}
};

struct RepMoveButton : SVGSwitch {
	RepMoveButton() {
		addFrame(SVG::load(assetPlugin(pluginInstance, "res/RepButton.svg")));
		addFrame(SVG::load(assetPlugin(pluginInstance, "res/RepButtonDown.svg")));
	}
};

struct TinyButton : SVGSwitch {
	TinyButton() {
		addFrame(SVG::load(assetPlugin(pluginInstance, "res/TinyButtonUp.svg")));
		addFrame(SVG::load(assetPlugin(pluginInstance, "res/TinyButtonDown.svg")));
	}
};

struct SmallButton : SVGSwitch {
	SmallButton() {
		addFrame(SVG::load(assetPlugin(pluginInstance, "res/SmallButtonUp.svg")));
		addFrame(SVG::load(assetPlugin(pluginInstance, "res/SmallButtonDown.svg")));
	}
};

////////////////////////////////////////////// SCREWS //////////////////////////////////////////////

struct Snowflake : SVGScrew {
	Snowflake() {
		sw->setSVG(SVG::load(assetPlugin(pluginInstance, "res/SnowFlake.svg")));
		box.size = sw->box.size;
	}
};

struct WavHeadLogo : SVGScrew {
	WavHeadLogo() {
		sw->setSVG(SVG::load(assetPlugin(pluginInstance, "res/WavHeadSmall.svg")));
		box.size = sw->box.size;
	}
};

struct Screw_J : SVGScrew {
	Screw_J() {
		sw->setSVG(SVG::load(assetPlugin(pluginInstance, "res/Screw_J.svg")));
		box.size = sw->box.size;
	}
};

struct Screw_W : SVGScrew {
	Screw_W() {
		sw->setSVG(SVG::load(assetPlugin(pluginInstance, "res/Screw_W.svg")));
		box.size = sw->box.size;
	}
};

struct CatScrew : SVGScrew {
	CatScrew() {
		sw->setSVG(SVG::load(assetPlugin(pluginInstance, "res/Cat.svg")));
		box.size = sw->box.size;
	}
};

struct HairballScrew : SVGScrew {
	HairballScrew() {
		sw->setSVG(SVG::load(assetPlugin(pluginInstance, "res/Hairball.svg")));
		box.size = sw->box.size;
	}
};

////////////////////////////////////////////// WIDGETS //////////////////////////////////////////////

extern Model *modelBouncyBalls;
extern Model *modelSimpleClock;
extern Model *modelMinMax;
extern Model *modelQuantizer;
extern Model *modelNoteSeq;
extern Model *modelWavHead;
extern Model *modelXYPad;
extern Model *modelFullScope;
extern Model *modelGridSeq;
extern Model *modelThingThing;
extern Model *modelCat;
extern Model *modelBlankPanelSmall;
extern Model *modelBlankPanelMedium;
extern Model *modelBlankPanelLarge;

inline int clampijw(int x, int minimum, int maximum) {
	return min(max(x, minimum), maximum);
}
inline float clampfjw(float x, float minimum, float maximum) {
	return fminf(fmaxf(x, minimum), maximum);
}
inline float rescalefjw(float x, float xMin, float xMax, float yMin, float yMax) {
	return yMin + (x - xMin) / (xMax - xMin) * (yMax - yMin);
}

