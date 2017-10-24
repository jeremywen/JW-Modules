#include <string.h>
#include "JWModules.hpp"
#include "dsp/digital.hpp"


#define BUFFER_SIZE 512

struct RMS : Module {
	enum ParamIds {
		TIME_PARAM,
		TRIG_PARAM,
		EXTERNAL_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		X_INPUT,
		Y_INPUT,
		TRIG_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		NUM_OUTPUTS
	};

	float bufferX[BUFFER_SIZE] = {};
	float bufferY[BUFFER_SIZE] = {};
	int bufferIndex = 0;
	float frameIndex = 0;

	SchmittTrigger sumTrigger;
	SchmittTrigger extTrigger;
	bool lissajous = false;
	bool external = false;
	float lights[4] = {};
	SchmittTrigger resetTrigger;

	RMS() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS) {}
	void step();

	json_t *toJson() {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "lissajous", json_integer((int) lissajous));
		json_object_set_new(rootJ, "external", json_integer((int) external));
		return rootJ;
	}

	void fromJson(json_t *rootJ) {
		json_t *sumJ = json_object_get(rootJ, "lissajous");
		if (sumJ)
			lissajous = json_integer_value(sumJ);

		json_t *extJ = json_object_get(rootJ, "external");
		if (extJ)
			external = json_integer_value(extJ);
	}

	void initialize() {
		lissajous = false;
		external = false;
	}
};


void RMS::step() {
	// Modes
	lights[0] = 0;
	lights[1] = 0;

	if (extTrigger.process(params[EXTERNAL_PARAM].value)) {
		external = !external;
	}
	lights[2] = external ? 0.0 : 1.0;
	lights[3] = external ? 1.0 : 0.0;

	// Compute time
	float deltaTime = powf(2.0, params[TIME_PARAM].value);
	int frameCount = (int)ceilf(deltaTime * gSampleRate);

	// Add frame to buffer
	if (bufferIndex < BUFFER_SIZE) {
		if (++frameIndex > frameCount) {
			frameIndex = 0;
			bufferX[bufferIndex] = inputs[X_INPUT].value;
			bufferY[bufferIndex] = inputs[Y_INPUT].value;
			bufferIndex++;
		}
	}

	// Are we waiting on the next trigger?
	if (bufferIndex >= BUFFER_SIZE) {
		// Trigger immediately if external but nothing plugged in, or in Lissajous mode
		if (lissajous || (external && !inputs[TRIG_INPUT].active)) {
			bufferIndex = 0;
			frameIndex = 0;
			return;
		}

		// Reset the Schmitt trigger so we don't trigger immediately if the input is high
		if (frameIndex == 0) {
			resetTrigger.reset();
		}
		frameIndex++;

		// Must go below 0.1V to trigger
		resetTrigger.setThresholds(params[TRIG_PARAM].value - 0.1, params[TRIG_PARAM].value);
		float gate = external ? inputs[TRIG_INPUT].value : inputs[X_INPUT].value;

		// Reset if triggered
		float holdTime = 0.1;
		if (resetTrigger.process(gate) || (frameIndex >= gSampleRate * holdTime)) {
			bufferIndex = 0; frameIndex = 0; return;
		}

		// Reset if we've waited too long
		if (frameIndex >= gSampleRate * holdTime) {
			bufferIndex = 0; frameIndex = 0; return;
		}
	}
}


struct RMSDisplay : TransparentWidget {
	RMS *module;
	int frame = 0;
	std::shared_ptr<Font> font;

	struct Stats {
		float vrms, vpp, vmin, vmax;
		void calculate(float *values) {
			vrms = 0.0;
			vmax = -INFINITY;
			vmin = INFINITY;
			for (int i = 0; i < BUFFER_SIZE; i++) {
				float v = values[i];
				vrms += v*v;
				vmax = fmaxf(vmax, v);
				vmin = fminf(vmin, v);
			}
			vrms = sqrtf(vrms / BUFFER_SIZE);
			vpp = vmax - vmin;
		}
	};
	Stats statsX, statsY;

	RMSDisplay() {
		font = Font::load(assetPlugin(plugin, "res/DejaVuSansMono.ttf"));
	}

	void drawStats(NVGcontext *vg, Vec pos, const char *title, Stats *stats) {
		nvgFontSize(vg, 24);
		nvgFontFaceId(vg, font->handle);
		nvgTextLetterSpacing(vg, -2);

		nvgFillColor(vg, nvgRGBA(0xff, 0xff, 0xff, 0x80));
		char text[128];
		snprintf(text, sizeof(text), "%5.2f", stats->vrms);
		nvgText(vg, pos.x + 10, pos.y + 20, text, NULL);
	}

	void draw(NVGcontext *vg) {
		float gainX = 1;
		float gainY = 1;
		float offsetX = 0;
		float offsetY = 0;

		float valuesX[BUFFER_SIZE];
		float valuesY[BUFFER_SIZE];
		for (int i = 0; i < BUFFER_SIZE; i++) {
			int j = i;
			// Lock display to buffer if buffer update deltaTime <= 2^-11
			if (module->lissajous)
				j = (i + module->bufferIndex) % BUFFER_SIZE;
			valuesX[i] = (module->bufferX[j] + offsetX) * gainX / 10.0;
			valuesY[i] = (module->bufferY[j] + offsetY) * gainY / 10.0;
		}

		// Calculate and draw stats
		if (++frame >= 4) {
			frame = 0;
			statsX.calculate(module->bufferX);
			statsY.calculate(module->bufferY);
		}
		drawStats(vg, Vec(0, 20), "X", &statsX);
	}
};


RMSWidget::RMSWidget() {
	RMS *module = new RMS();
	setModule(module);
	box.size = Vec(15*6, 380);

	{
		SVGPanel *panel = new SVGPanel();
		panel->box.size = box.size;
		panel->setBackground(SVG::load(assetPlugin(plugin, "res/RMS.svg")));
		addChild(panel);
	}

	addChild(createScrew<ScrewSilver>(Vec(15, 0)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x-30, 0)));
	addChild(createScrew<ScrewSilver>(Vec(15, 365)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x-30, 365)));

	{
		RMSDisplay *display = new RMSDisplay();
		display->module = module;
		display->box.pos = Vec(0, 44);
		display->box.size = Vec(box.size.x, 140);
		addChild(display);
	}

	addParam(createParam<Davies1900hSmallBlackKnob>(Vec(10, 209), module, RMS::TIME_PARAM, -6.0, -16.0, -14.0));
	addParam(createParam<Davies1900hSmallBlackKnob>(Vec(53, 209), module, RMS::TRIG_PARAM, -10.0, 10.0, 0.0));
	addParam(createParam<CKD6>(Vec(52, 262), module, RMS::EXTERNAL_PARAM, 0.0, 1.0, 0.0));

	addInput(createInput<PJ301MPort>(Vec(10, 319), module, RMS::X_INPUT));
	addInput(createInput<PJ301MPort>(Vec(54, 319), module, RMS::TRIG_INPUT));

	addChild(createValueLight<TinyLight<GreenValueLight>>(Vec(50, 251), &module->lights[2]));
	addChild(createValueLight<TinyLight<GreenValueLight>>(Vec(50, 296), &module->lights[3]));
}
