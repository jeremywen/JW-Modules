#include "rack.hpp"
#include "QuantizeUtils.cpp"

using namespace rack;
extern Plugin *plugin;

struct SimpleClockWidget : ModuleWidget {
	SimpleClockWidget();
};

struct MinMaxWidget : ModuleWidget {
	MinMaxWidget();
};

struct WavHeadWidget : ModuleWidget {
	WavHeadWidget();
	void step();
	Widget *widgetToMove;
	Menu *createContextMenu();
};

struct XYPadWidget : ModuleWidget {
	XYPadWidget();
	Menu *createContextMenu();
};

struct BouncyBallWidget : ModuleWidget {
	BouncyBallWidget();
};

struct QuantizerWidget : ModuleWidget {
	QuantizerWidget();
};

struct FullScopeWidget : ModuleWidget {
	Panel *panel;
	Widget *rightHandle;
	TransparentWidget *display;
	FullScopeWidget();
	void step();
	json_t *toJson();
	void fromJson(json_t *rootJ);
	Menu *createContextMenu();
};

struct GridSeqWidget : ModuleWidget {
	std::vector<ParamWidget*> seqKnobs;
	std::vector<ParamWidget*> gateButtons;
	GridSeqWidget();
	~GridSeqWidget(){ 
		seqKnobs.clear(); 
		gateButtons.clear(); 
	}
	Menu *createContextMenu();
};

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

struct SmallWhiteKnob : RoundKnob {
	SmallWhiteKnob() {
		setSVG(SVG::load(assetPlugin(plugin, "res/SmallWhiteKnob.svg")));
	}
	CenteredLabel* linkedLabel = nullptr;
	
	void connectLabel(CenteredLabel* label) {
		linkedLabel = label;
		if (linkedLabel) {
			linkedLabel->text = formatCurrentValue();
		}
	}

	void onChange(EventChange &e) override {
		RoundKnob::onChange(e);
		if (linkedLabel) {
			linkedLabel->text = formatCurrentValue();
		}
	}

	virtual std::string formatCurrentValue() {
		return std::to_string(static_cast<unsigned int>(value));
	}
};

struct JwTinyKnob : RoundKnob {
	JwTinyKnob() {
		setSVG(SVG::load(assetPlugin(plugin, "res/SmallWhiteKnob.svg")));
		box.size = Vec(15, 15);
	}
};

struct JwSmallSnapKnob : SmallWhiteKnob {
	JwSmallSnapKnob() {
		snap = true;
	}
};

struct WavHeadLogo : SVGScrew {
	WavHeadLogo() {
		sw->setSVG(SVG::load(assetPlugin(plugin, "res/WavHeadSmall.svg")));
		box.size = sw->box.size;
	}
};

struct Screw_J : SVGScrew {
	Screw_J() {
		sw->setSVG(SVG::load(assetPlugin(plugin, "res/Screw_J.svg")));
		box.size = sw->box.size;
	}
};

struct Screw_W : SVGScrew {
	Screw_W() {
		sw->setSVG(SVG::load(assetPlugin(plugin, "res/Screw_W.svg")));
		box.size = sw->box.size;
	}
};

struct TinyPJ301MPort : SVGPort {
	TinyPJ301MPort() {
		background->svg = SVG::load(assetPlugin(plugin, "res/TinyPJ301M.svg"));
		background->wrap();
		box.size = background->box.size;
	}
};

struct MyBlueValueLight : ModuleLightWidget {
	MyBlueValueLight() {
		firstLightId = 1;
		addBaseColor(nvgRGB(25, 150, 252));
	}
};

struct RightMoveButton : SVGSwitch, MomentarySwitch {
	RightMoveButton() {
		addFrame(SVG::load(assetPlugin(plugin, "res/RightButton.svg")));
		addFrame(SVG::load(assetPlugin(plugin, "res/RightButtonDown.svg")));
	}
};

struct LeftMoveButton : SVGSwitch, MomentarySwitch {
	LeftMoveButton() {
		addFrame(SVG::load(assetPlugin(plugin, "res/LeftButton.svg")));
		addFrame(SVG::load(assetPlugin(plugin, "res/LeftButtonDown.svg")));
	}
};

struct DownMoveButton : SVGSwitch, MomentarySwitch {
	DownMoveButton() {
		addFrame(SVG::load(assetPlugin(plugin, "res/DownButton.svg")));
		addFrame(SVG::load(assetPlugin(plugin, "res/DownButtonDown.svg")));
	}
};

struct UpMoveButton : SVGSwitch, MomentarySwitch {
	UpMoveButton() {
		addFrame(SVG::load(assetPlugin(plugin, "res/UpButton.svg")));
		addFrame(SVG::load(assetPlugin(plugin, "res/UpButtonDown.svg")));
	}
};

struct RndMoveButton : SVGSwitch, MomentarySwitch {
	RndMoveButton() {
		addFrame(SVG::load(assetPlugin(plugin, "res/RndButton.svg")));
		addFrame(SVG::load(assetPlugin(plugin, "res/RndButtonDown.svg")));
	}
};

struct RepMoveButton : SVGSwitch, MomentarySwitch {
	RepMoveButton() {
		addFrame(SVG::load(assetPlugin(plugin, "res/RepButton.svg")));
		addFrame(SVG::load(assetPlugin(plugin, "res/RepButtonDown.svg")));
	}
};

struct TinyButton : SVGSwitch, MomentarySwitch {
	TinyButton() {
		addFrame(SVG::load(assetPlugin(plugin, "res/TinyButtonUp.svg")));
		addFrame(SVG::load(assetPlugin(plugin, "res/TinyButtonDown.svg")));
	}
};

struct SmallButton : SVGSwitch, MomentarySwitch {
	SmallButton() {
		addFrame(SVG::load(assetPlugin(plugin, "res/SmallButtonUp.svg")));
		addFrame(SVG::load(assetPlugin(plugin, "res/SmallButtonDown.svg")));
	}
};

struct ModuleResizeHandle : Widget {
	float minWidth;
	bool right = false;
	float dragX;
	Rect originalBox;
	ModuleResizeHandle(float _minWidth) {
		box.size = Vec(RACK_GRID_WIDTH * 1, RACK_GRID_HEIGHT);
		minWidth = _minWidth;
	}
	void onMouseDown(EventMouseDown &e) override {
		if (e.button == 0) {
			e.consumed = true;
			e.target = this;
		}
	}
	void onDragStart(EventDragStart &e) override {
		dragX = gRackWidget->lastMousePos.x;
		ModuleWidget *m = getAncestorOfType<ModuleWidget>();
		originalBox = m->box;
	}
	void onDragMove(EventDragMove &e) override {
		ModuleWidget *m = getAncestorOfType<ModuleWidget>();

		float newDragX = gRackWidget->lastMousePos.x;
		float deltaX = newDragX - dragX;

		Rect newBox = originalBox;
		if (right) {
			newBox.size.x += deltaX;
			newBox.size.x = fmaxf(newBox.size.x, minWidth);
			newBox.size.x = roundf(newBox.size.x / RACK_GRID_WIDTH) * RACK_GRID_WIDTH;
		}
		else {
			newBox.size.x -= deltaX;
			newBox.size.x = fmaxf(newBox.size.x, minWidth);
			newBox.size.x = roundf(newBox.size.x / RACK_GRID_WIDTH) * RACK_GRID_WIDTH;
			newBox.pos.x = originalBox.pos.x + originalBox.size.x - newBox.size.x;
		}
		gRackWidget->requestModuleBox(m, newBox);
	}
};

struct NoteKnob : SmallWhiteKnob {
	QuantizeUtils *quantizeUtils;
	NoteKnob(){
		snap = true;
	}
	std::string formatCurrentValue() override {
		return quantizeUtils->noteName(int(value));
	}
};

struct ScaleKnob : SmallWhiteKnob {
	QuantizeUtils *quantizeUtils;
	ScaleKnob(){
		snap = true;
	}
	std::string formatCurrentValue() override {
		return quantizeUtils->scaleName(int(value));
	}
};

struct BPMKnob : SmallWhiteKnob {
	BPMKnob(){}
	std::string formatCurrentValue() {
		return std::to_string(static_cast<unsigned int>(powf(2.0, value)*60.0)) + " BPM";
	}
};
