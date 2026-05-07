#include "JWModules.hpp"
#include <math.h>

struct GlitchThing : Module {
	enum ParamIds {
		DISSOLVE_PARAM,
		CHANCE_PARAM,
		SMEAR_PARAM,
		GLITCH_PARAM,
		RATE_PARAM,
		DEPTH_PARAM,
		FILTER_PARAM,
		SHAPE_PARAM,
		MOD_MODE_PARAM,
		AUX_HOLD_PARAM,
		DRY_WET_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		IN_L_INPUT,
		IN_R_INPUT,
		DISSOLVE_CV_INPUT,
		CHANCE_CV_INPUT,
		RATE_CV_INPUT,
		DEPTH_CV_INPUT,
		FILTER_CV_INPUT,
		SMEAR_CV_INPUT,
		GLITCH_CV_INPUT,
		DRY_WET_CV_INPUT,
		AUX_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT_L_OUTPUT,
		OUT_R_OUTPUT,
		ENV_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		AUX_LIGHT,
		FILTER_MODE_LIGHT,
		NUM_LIGHTS
	};

	static const int MAX_DELAY_SAMPLES = 1 << 19;
	float delayL[MAX_DELAY_SAMPLES] = {};
	float delayR[MAX_DELAY_SAMPLES] = {};
	float preL[MAX_DELAY_SAMPLES] = {};
	float preR[MAX_DELAY_SAMPLES] = {};
	int writeIndex = 0;
	int sampleHoldCounter = 0;
	float heldL = 0.f;
	float heldR = 0.f;
	float envFollower = 0.f;
	float phase = 0.f;
	float randSineTarget = 0.f;
	float randSquare = -1.f;
	float diffL = 0.f;
	float diffR = 0.f;
	float filterStateL = 0.f;
	float filterStateR = 0.f;
	float vibPhase = 0.f;
	float triggerPhase = 0.f;
	float glitchJump = 0.f;
	bool filterHighPass = false;
	dsp::SchmittTrigger auxEdge;

	GlitchThing() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(DISSOLVE_PARAM, 0.f, 1.f, 0.f, "Dissolve");
		configParam(CHANCE_PARAM, 0.f, 1.f, 0.3f, "Chance");
		configParam(SMEAR_PARAM, 0.f, 1.f, 0.2f, "Smear");
		configParam(GLITCH_PARAM, 0.f, 1.f, 0.25f, "Glitch");
		configParam(RATE_PARAM, 0.f, 1.f, 0.2f, "Rate");
		configParam(DEPTH_PARAM, 0.f, 1.f, 0.4f, "Depth");
		configParam(FILTER_PARAM, 0.f, 1.f, 0.5f, "Filter");
		configParam(SHAPE_PARAM, 0.f, 6.f, 0.f, "LFO Shape");
		configParam(MOD_MODE_PARAM, 0.f, 2.f, 0.f, "Modulation Mode");
		configButton(AUX_HOLD_PARAM, "Aux Hold");
		configParam(DRY_WET_PARAM, 0.f, 1.f, 0.5f, "Dry/Wet");

		paramQuantities[SHAPE_PARAM]->snapEnabled = true;
		paramQuantities[MOD_MODE_PARAM]->snapEnabled = true;

		configInput(IN_L_INPUT, "Left In");
		configInput(IN_R_INPUT, "Right In");
		configInput(DISSOLVE_CV_INPUT, "Dissolve CV");
		configInput(CHANCE_CV_INPUT, "Chance CV");
		configInput(RATE_CV_INPUT, "Rate CV");
		configInput(DEPTH_CV_INPUT, "Depth CV");
		configInput(FILTER_CV_INPUT, "Filter CV");
		configInput(SMEAR_CV_INPUT, "Smear CV");
		configInput(GLITCH_CV_INPUT, "Glitch CV");
		configInput(DRY_WET_CV_INPUT, "Dry/Wet CV");
		configInput(AUX_INPUT, "Aux Hold");

		configOutput(OUT_L_OUTPUT, "Left Out");
		configOutput(OUT_R_OUTPUT, "Right Out");
		configOutput(ENV_OUTPUT, "Envelope / Mod");
	}

	float clampfLocal(float x, float lo, float hi) {
		if (x < lo) return lo;
		if (x > hi) return hi;
		return x;
	}

	float mix(float a, float b, float t) {
		return a + (b - a) * t;
	}

	float equalPowerMix(float dry, float wet, float t) {
		float blend = clampfLocal(t, 0.f, 1.f);
		float dryGain = cosf(blend * 0.5f * M_PI);
		float wetGain = sinf(blend * 0.5f * M_PI);
		return dry * dryGain + wet * wetGain;
	}

	float nextLfo(int shape, float phaseValue) {
		float p = phaseValue - floorf(phaseValue);
		switch (shape) {
			case 0: return sinf(2.f * M_PI * p);
			case 1: return p < 0.5f ? 1.f : -1.f;
			case 2: return (2.f * p) - 1.f;
			case 3: return 1.f - (2.f * p);
			case 4:
				if (p < 0.001f) randSineTarget = random::uniform() * 2.f - 1.f;
				return sinf(2.f * M_PI * p) * randSineTarget;
			case 5:
				if (p < 0.001f) randSquare = random::uniform() < 0.5f ? -1.f : 1.f;
				return randSquare;
			case 6:
			default:
				return envFollower * 2.f - 1.f;
		}
	}

	float readWrapped(float* buffer, float indexF) {
		while (indexF < 0.f) indexF += (float)MAX_DELAY_SAMPLES;
		while (indexF >= (float)MAX_DELAY_SAMPLES) indexF -= (float)MAX_DELAY_SAMPLES;
		int i0 = (int)indexF;
		int i1 = i0 + 1;
		if (i1 >= MAX_DELAY_SAMPLES) i1 = 0;
		float frac = indexF - (float)i0;
		return mix(buffer[i0], buffer[i1], frac);
	}

	float postFilter(float x, float cutoff, bool highPass, float& state) {
		float c = clampfLocal(cutoff, 0.001f, 0.999f);
		state += c * (x - state);
		if (highPass) {
			return x - state;
		}
		return state;
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "filterHighPass", json_integer(filterHighPass ? 1 : 0));
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		json_t *hpJ = json_object_get(rootJ, "filterHighPass");
		if (hpJ) {
			filterHighPass = json_integer_value(hpJ) != 0;
		}
	}

	void onReset() override {
		sampleHoldCounter = 0;
		heldL = heldR = 0.f;
		envFollower = 0.f;
		phase = 0.f;
		vibPhase = 0.f;
		triggerPhase = 0.f;
		diffL = diffR = 0.f;
		filterStateL = filterStateR = 0.f;
	}

	void process(const ProcessArgs &args) override {
		float sr = args.sampleRate;

		bool auxHeld = params[AUX_HOLD_PARAM].getValue() > 0.5f || inputs[AUX_INPUT].getVoltage() > 1.f;
		if (auxEdge.process(auxHeld ? 10.f : 0.f) && auxHeld) {
			filterHighPass = !filterHighPass;
		}

		float inL = inputs[IN_L_INPUT].getVoltage();
		float inR = inputs[IN_R_INPUT].isConnected() ? inputs[IN_R_INPUT].getVoltage() : inL;

		float dissolve = params[DISSOLVE_PARAM].getValue();
		dissolve += inputs[DISSOLVE_CV_INPUT].getVoltage() * 0.1f;
		dissolve = clampfLocal(dissolve, 0.f, 1.f);

		float chance = params[CHANCE_PARAM].getValue();
		chance += inputs[CHANCE_CV_INPUT].getVoltage() * 0.1f;
		chance = clampfLocal(chance, 0.f, 1.f);

		float rate = params[RATE_PARAM].getValue();
		rate += inputs[RATE_CV_INPUT].getVoltage() * 0.1f;
		rate = clampfLocal(rate, 0.f, 1.f);

		float depth = params[DEPTH_PARAM].getValue();
		depth += inputs[DEPTH_CV_INPUT].getVoltage() * 0.1f;
		depth = clampfLocal(depth, 0.f, 1.f);

		float smear = params[SMEAR_PARAM].getValue();
		smear += inputs[SMEAR_CV_INPUT].getVoltage() * 0.1f;
		smear = clampfLocal(smear, 0.f, 1.f);
		float glitch = params[GLITCH_PARAM].getValue();
		glitch += inputs[GLITCH_CV_INPUT].getVoltage() * 0.1f;
		glitch = clampfLocal(glitch, 0.f, 1.f);
		int shape = (int)params[SHAPE_PARAM].getValue();
		int modMode = (int)params[MOD_MODE_PARAM].getValue();

		// Aux mode repurposes controls as secondary functions.
		float width = auxHeld ? depth : 0.3f + 0.7f * depth;
		float subdivision = auxHeld ? (1.f + floorf(rate * 7.f)) : 1.f;
		float lfoRateHz = 0.05f + rate * 14.f;
		lfoRateHz *= subdivision;

		envFollower += (fabsf(inL) - envFollower) * 0.0025f;
		envFollower = clampfLocal(envFollower, 0.f, 1.f);

		phase += lfoRateHz / sr;
		if (phase >= 1.f) phase -= floorf(phase);
		float lfo = nextLfo(shape, phase);

		triggerPhase += lfoRateHz / sr;
		if (triggerPhase >= 1.f) {
			triggerPhase -= 1.f;
			if (random::uniform() < chance) {
				if (glitch < 0.5f) {
					glitchJump = floorf(glitch * 8.f) * 0.5f;
				}
				else {
					glitchJump = floorf(random::uniform() * 6.f) - 2.f;
				}
			}
		}

		float processedL = inL;
		float processedR = inR;
		if (dissolve < 0.5f) {
			float amt = dissolve * 2.f;
			int holdSamples = (int)(1.f + amt * amt * 2048.f);
			if (--sampleHoldCounter <= 0) {
				sampleHoldCounter = holdSamples;
				heldL = inL;
				heldR = inR;
			}
			processedL = mix(inL, heldL, amt);
			processedR = mix(inR, heldR, amt);
		}
		else {
			float amt = (dissolve - 0.5f) * 2.f;
			float segScale = powf(2.f, floorf(amt * 5.f));
			float reverseSpan = 1200.f * segScale;
			float rp = (float)writeIndex - reverseSpan;
			processedL = mix(inL, readWrapped(preL, rp), amt);
			processedR = mix(inR, readWrapped(preR, rp), amt);
		}

		vibPhase += lfoRateHz / sr;
		if (vibPhase >= 1.f) vibPhase -= floorf(vibPhase);
		float vib = sinf(2.f * M_PI * vibPhase) * depth * 20.f;
		float vibSpread = width * 10.f;
		float vibReadL = readWrapped(preL, (float)writeIndex - 1.f - vib - vibSpread);
		float vibReadR = readWrapped(preR, (float)writeIndex - 1.f + vib + vibSpread);

		if (modMode == 0) {
			float trem = 0.5f + 0.5f * lfo * depth;
			processedL *= trem;
			processedR *= trem;
		}
		else if (modMode == 1) {
			processedL = mix(processedL, vibReadL, 0.8f * depth);
			processedR = mix(processedR, vibReadR, 0.8f * depth);
		}
		else {
			float gate = envFollower > 0.25f ? 1.f : 0.f;
			chance *= gate;
		}

		float jumpDiv = powf(2.f, glitchJump);
		float delaySamples = (sr * 0.16f) / jumpDiv;
		delaySamples = clampfLocal(delaySamples, 30.f, (float)MAX_DELAY_SAMPLES - 2.f);

		float readL = readWrapped(delayL, (float)writeIndex - delaySamples);
		float readR = readWrapped(delayR, (float)writeIndex - delaySamples);
		diffL += (readL - diffL) * (0.02f + smear * 0.2f);
		diffR += (readR - diffR) * (0.02f + smear * 0.2f);

		float fb = 0.2f + smear * 0.75f;
		float writeWetL = processedL + diffL * fb;
		float writeWetR = processedR + diffR * fb;

		delayL[writeIndex] = writeWetL;
		delayR[writeIndex] = writeWetR;
		preL[writeIndex] = inL;
		preR[writeIndex] = inR;
		writeIndex++;
		if (writeIndex >= MAX_DELAY_SAMPLES) writeIndex = 0;

		float dryWet = params[DRY_WET_PARAM].getValue();
		dryWet += inputs[DRY_WET_CV_INPUT].getVoltage() * 0.1f;
		dryWet = clampfLocal(dryWet, 0.f, 1.f);

		float wet = 0.2f + 0.6f * chance + 0.2f * smear;
		float wetOutL = mix(processedL, diffL, wet);
		float wetOutR = mix(processedR, diffR, wet);
		float wetMakeup = 1.f + 0.45f * dryWet + 0.35f * wet;
		wetOutL *= wetMakeup;
		wetOutR *= wetMakeup;
		float outL = equalPowerMix(inL, wetOutL, dryWet);
		float outR = equalPowerMix(inR, wetOutR, dryWet);

		float filter = params[FILTER_PARAM].getValue();
		filter += inputs[FILTER_CV_INPUT].getVoltage() * 0.1f;
		filter = clampfLocal(filter, 0.f, 1.f);
		float filterCut = 0.005f + filter * 0.25f;
		outL = postFilter(outL, filterCut, filterHighPass, filterStateL);
		outR = postFilter(outR, filterCut, filterHighPass, filterStateR);

		float outGain = 1.8f;
		outputs[OUT_L_OUTPUT].setVoltage(clampfLocal(outL * outGain, -10.f, 10.f));
		outputs[OUT_R_OUTPUT].setVoltage(clampfLocal(outR * outGain, -10.f, 10.f));
		outputs[ENV_OUTPUT].setVoltage((envFollower * 10.f) - 5.f);

		lights[AUX_LIGHT].setBrightness(auxHeld ? 1.f : 0.f);
		lights[FILTER_MODE_LIGHT].setBrightness(filterHighPass ? 1.f : 0.f);
	}
};

struct GlitchThingWidget : ModuleWidget {
	GlitchThingWidget(GlitchThing *module);
	void appendContextMenu(Menu *menu) override;
};

GlitchThingWidget::GlitchThingWidget(GlitchThing *module) {
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH * 11, RACK_GRID_HEIGHT);

	setPanel(createPanel(
		asset::plugin(pluginInstance, "res/GlitchThing.svg"),
		asset::plugin(pluginInstance, "res/GlitchThing.svg")
	));

	addChild(createWidget<Screw_J>(Vec(16, 2)));
	addChild(createWidget<Screw_J>(Vec(16, 365)));
	addChild(createWidget<Screw_W>(Vec(box.size.x - 29, 2)));
	addChild(createWidget<Screw_W>(Vec(box.size.x - 29, 365)));

	// Row 1: Rate, Glitch, Dissolve
	addParam(createParam<SmallWhiteKnob>(Vec(18, 55), module, GlitchThing::RATE_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(27, 83), module, GlitchThing::RATE_CV_INPUT));

	addParam(createParam<SmallWhiteKnob>(Vec(66, 55), module, GlitchThing::GLITCH_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(75, 83), module, GlitchThing::GLITCH_CV_INPUT));

	addParam(createParam<SmallWhiteKnob>(Vec(114, 55), module, GlitchThing::DISSOLVE_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(123, 83), module, GlitchThing::DISSOLVE_CV_INPUT));

	// Row 2: Depth, Chance, Smear
	addParam(createParam<SmallWhiteKnob>(Vec(18, 122), module, GlitchThing::DEPTH_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(27, 150), module, GlitchThing::DEPTH_CV_INPUT));

	addParam(createParam<SmallWhiteKnob>(Vec(66, 122), module, GlitchThing::CHANCE_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(75, 150), module, GlitchThing::CHANCE_CV_INPUT));

	addParam(createParam<SmallWhiteKnob>(Vec(114, 122), module, GlitchThing::SMEAR_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(123, 150), module, GlitchThing::SMEAR_CV_INPUT));

	// Bottom small controls: Mix, Filter, Shape, Mode
	addParam(createParam<JwTinyGrayKnob>(Vec(23, 202), module, GlitchThing::DRY_WET_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(27, 225), module, GlitchThing::DRY_WET_CV_INPUT));
	addParam(createParam<JwTinyGrayKnob>(Vec(55, 202), module, GlitchThing::FILTER_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(59, 225), module, GlitchThing::FILTER_CV_INPUT));
	addParam(createParam<JwTinyGrayKnob>(Vec(87, 202), module, GlitchThing::SHAPE_PARAM));
	addParam(createParam<JwTinyGrayKnob>(Vec(119, 202), module, GlitchThing::MOD_MODE_PARAM));

	// Aux / filter mode utilities
	addParam(createParam<LEDButton>(Vec(91, 251), module, GlitchThing::AUX_HOLD_PARAM));
	addChild(createLight<MediumLight<BlueLight>>(Vec(95, 255), module, GlitchThing::AUX_LIGHT));
	addChild(createLight<MediumLight<RedLight>>(Vec(123, 255), module, GlitchThing::FILTER_MODE_LIGHT));

	// Audio I/O
	addInput(createInput<PJ301MPort>(Vec(16, 314), module, GlitchThing::IN_L_INPUT));
	addInput(createInput<PJ301MPort>(Vec(64, 314), module, GlitchThing::IN_R_INPUT));

	addOutput(createOutput<PJ301MPort>(Vec(16, 344), module, GlitchThing::OUT_L_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(64, 344), module, GlitchThing::OUT_R_OUTPUT));
	addOutput(createOutput<TinyPJ301MPort>(Vec(119, 344), module, GlitchThing::ENV_OUTPUT));
	addInput(createInput<TinyPJ301MPort>(Vec(119, 314), module, GlitchThing::AUX_INPUT));
}

void GlitchThingWidget::appendContextMenu(Menu *menu) {
	MenuLabel *spacerLabel = new MenuLabel();
	menu->addChild(spacerLabel);
}

Model *modelGlitchThing = createModel<GlitchThing, GlitchThingWidget>("GlitchThing");
