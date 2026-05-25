#include "JWModules.hpp"

struct FM4Dice : Module {
	static constexpr int MAX_STEPS = 8;
	static constexpr int OP_COUNT = 4;
	enum ParamIds {
		LENGTH_PARAM,
		STEP_COUNT_PARAM,
		DICE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		TRIGGER_INPUT,
		RANDOMIZE_TRIGGER_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		AUDIO_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};
	enum Waveform {
		WAVE_SINE,
		WAVE_TRI,
		WAVE_SAW,
		WAVE_SQUARE
	};
	enum Algorithm {
		ALG_STACK,
		ALG_TWO_STACKS,
		ALG_TRIPLE_INTO_CARRIER,
		ALG_TWO_INTO_CARRIER_PLUS_ONE,
		ALG_PARALLEL_PAIR,
		ALG_CASCADE_PLUS_FM_BRANCH,
		NUM_ALGORITHMS
	};

	struct OperatorState {
		float ratio = 1.f;
		float level = 0.7f;
		int wave = WAVE_SINE;
		float ampAttack = 0.01f;
		float ampDecay = 0.35f;
		float pitchAttack = 0.005f;
		float pitchDecay = 0.3f;
		float pitchAmountOct = 0.f;
		float index = 0.f;
		float feedback = 0.12f;
	};

	struct StepState {
		float baseFrequency = 110.f;
		int algorithm = ALG_STACK;
		OperatorState ops[OP_COUNT];
	};

	dsp::SchmittTrigger triggerInputTrig;
	dsp::SchmittTrigger diceButtonTrig;
	bool randomizeInputPrimed = false;
	bool randomizeInputWasHigh = false;
	float phase[OP_COUNT] = {};
	float feedbackState[OP_COUNT] = {};
	StepState steps[MAX_STEPS];
	OperatorState ops[OP_COUNT];
	float noteElapsed = 0.f;
	bool noteActive = false;
	float baseFrequency = 110.f;
	int algorithm = ALG_STACK;
	int currentStep = -1;
	float outputSmooth = 0.f;
	float dcBlockX1 = 0.f;
	float dcBlockY1 = 0.f;

	FM4Dice() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(LENGTH_PARAM, 0.005f, 5.0f, 0.4f, "Decay", " s");
		configParam(STEP_COUNT_PARAM, 1.0f, 8.0f, 8.0f, "Step count");
		paramQuantities[STEP_COUNT_PARAM]->snapEnabled = true;
		configButton(DICE_PARAM, "Randomize operators");
		configInput(TRIGGER_INPUT, "Trigger");
		configInput(RANDOMIZE_TRIGGER_INPUT, "Randomize trigger");
		configOutput(AUDIO_OUTPUT, "Audio");
		randomizeAllSteps();
		applyStepState(0);
	}

	void onReset() override {
		for (int i = 0; i < OP_COUNT; i++) {
			phase[i] = 0.f;
			feedbackState[i] = 0.f;
		}
		noteElapsed = 0.f;
		noteActive = false;
		currentStep = -1;
		randomizeInputPrimed = false;
		randomizeInputWasHigh = false;
		outputSmooth = 0.f;
		dcBlockX1 = 0.f;
		dcBlockY1 = 0.f;
	}

	void onRandomize() override {
		randomizeAllSteps();
		if (currentStep >= 0) {
			applyStepState(currentStep);
		}
		else {
			applyStepState(0);
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "stepCount", json_integer(clampijw((int)std::round(params[STEP_COUNT_PARAM].getValue()), 1, MAX_STEPS)));
		json_object_set_new(rootJ, "currentStep", json_integer(currentStep));
		json_object_set_new(rootJ, "algorithm", json_integer(algorithm));
		json_object_set_new(rootJ, "baseFrequency", json_real(baseFrequency));
		json_object_set_new(rootJ, "noteLength", json_real(params[LENGTH_PARAM].getValue()));
		json_object_set_new(rootJ, "noteActive", json_boolean(noteActive));
		json_object_set_new(rootJ, "noteElapsed", json_real(noteElapsed));

		json_t* stepsJ = json_array();
		for (int s = 0; s < MAX_STEPS; s++) {
			json_t* stepJ = json_object();
			json_object_set_new(stepJ, "algorithm", json_integer(steps[s].algorithm));
			json_object_set_new(stepJ, "baseFrequency", json_real(steps[s].baseFrequency));

			json_t* opsJ = json_array();
			for (int op = 0; op < OP_COUNT; op++) {
				json_t* opJ = json_object();
				json_object_set_new(opJ, "ratio", json_real(steps[s].ops[op].ratio));
				json_object_set_new(opJ, "level", json_real(steps[s].ops[op].level));
				json_object_set_new(opJ, "wave", json_integer(steps[s].ops[op].wave));
				json_object_set_new(opJ, "ampAttack", json_real(steps[s].ops[op].ampAttack));
				json_object_set_new(opJ, "ampDecay", json_real(steps[s].ops[op].ampDecay));
				json_object_set_new(opJ, "pitchAttack", json_real(steps[s].ops[op].pitchAttack));
				json_object_set_new(opJ, "pitchDecay", json_real(steps[s].ops[op].pitchDecay));
				json_object_set_new(opJ, "pitchAmountOct", json_real(steps[s].ops[op].pitchAmountOct));
				json_object_set_new(opJ, "index", json_real(steps[s].ops[op].index));
				json_object_set_new(opJ, "feedback", json_real(steps[s].ops[op].feedback));
				json_array_append_new(opsJ, opJ);
			}
			json_object_set_new(stepJ, "ops", opsJ);
			json_array_append_new(stepsJ, stepJ);
		}
		json_object_set_new(rootJ, "steps", stepsJ);

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		if (!rootJ) return;
		json_t* stepCountJ = json_object_get(rootJ, "stepCount");
		if (stepCountJ) {
			params[STEP_COUNT_PARAM].setValue((float)clampijw((int)json_integer_value(stepCountJ), 1, MAX_STEPS));
		}
		json_t* currentStepJ = json_object_get(rootJ, "currentStep");
		if (currentStepJ) {
			currentStep = clampijw((int)json_integer_value(currentStepJ), -1, MAX_STEPS - 1);
		}

		json_t* algorithmJ = json_object_get(rootJ, "algorithm");
		if (algorithmJ) {
			algorithm = clampijw((int)json_integer_value(algorithmJ), 0, NUM_ALGORITHMS - 1);
		}
		json_t* baseFrequencyJ = json_object_get(rootJ, "baseFrequency");
		if (baseFrequencyJ) {
			baseFrequency = clampfjw((float)json_real_value(baseFrequencyJ), 20.f, 2000.f);
		}
		json_t* noteLengthJ = json_object_get(rootJ, "noteLength");
		if (noteLengthJ) {
			params[LENGTH_PARAM].setValue(clampfjw((float)json_real_value(noteLengthJ), 0.005f, 5.f));
		}
		json_t* noteActiveJ = json_object_get(rootJ, "noteActive");
		if (noteActiveJ) {
			noteActive = json_is_true(noteActiveJ);
		}
		json_t* noteElapsedJ = json_object_get(rootJ, "noteElapsed");
		if (noteElapsedJ) {
			noteElapsed = std::max(0.f, (float)json_real_value(noteElapsedJ));
		}

		json_t* stepsJ = json_object_get(rootJ, "steps");
		if (stepsJ && json_is_array(stepsJ)) {
			size_t s;
			json_t* stepJ;
			json_array_foreach(stepsJ, s, stepJ) {
				if (s >= MAX_STEPS || !json_is_object(stepJ)) break;
				json_t* v;
				v = json_object_get(stepJ, "algorithm"); if (v) steps[s].algorithm = clampijw((int)json_integer_value(v), 0, NUM_ALGORITHMS - 1);
				v = json_object_get(stepJ, "baseFrequency"); if (v) steps[s].baseFrequency = clampfjw((float)json_real_value(v), 20.f, 2000.f);

				json_t* opsJ = json_object_get(stepJ, "ops");
				if (opsJ && json_is_array(opsJ)) {
					size_t i;
					json_t* opJ;
					json_array_foreach(opsJ, i, opJ) {
						if (i >= OP_COUNT || !json_is_object(opJ)) break;
						json_t* ov;
						ov = json_object_get(opJ, "ratio"); if (ov) steps[s].ops[i].ratio = clampfjw((float)json_real_value(ov), 0.125f, 12.f);
						ov = json_object_get(opJ, "level"); if (ov) steps[s].ops[i].level = clampfjw((float)json_real_value(ov), 0.f, 1.f);
						ov = json_object_get(opJ, "wave"); if (ov) steps[s].ops[i].wave = clampijw((int)json_integer_value(ov), 0, 3);
						ov = json_object_get(opJ, "ampAttack"); if (ov) steps[s].ops[i].ampAttack = clampfjw((float)json_real_value(ov), 0.001f, 0.5f);
						ov = json_object_get(opJ, "ampDecay"); if (ov) steps[s].ops[i].ampDecay = clampfjw((float)json_real_value(ov), 0.01f, 5.f);
						ov = json_object_get(opJ, "pitchAttack"); if (ov) steps[s].ops[i].pitchAttack = clampfjw((float)json_real_value(ov), 0.001f, 0.5f);
						ov = json_object_get(opJ, "pitchDecay"); if (ov) steps[s].ops[i].pitchDecay = clampfjw((float)json_real_value(ov), 0.01f, 5.f);
						ov = json_object_get(opJ, "pitchAmountOct"); if (ov) steps[s].ops[i].pitchAmountOct = clampfjw((float)json_real_value(ov), -4.f, 4.f);
						ov = json_object_get(opJ, "index"); if (ov) steps[s].ops[i].index = clampfjw((float)json_real_value(ov), 0.f, 8.f);
						ov = json_object_get(opJ, "feedback"); if (ov) steps[s].ops[i].feedback = clampfjw((float)json_real_value(ov), 0.f, 1.f);
					}
				}
			}
		}

		if (currentStep >= 0 && currentStep < MAX_STEPS) {
			applyStepState(currentStep);
		}
		else {
			applyStepState(0);
		}

		if (currentStep >= 0) {
			int stepCount = clampijw((int)std::round(params[STEP_COUNT_PARAM].getValue()), 1, MAX_STEPS);
			currentStep = currentStep % stepCount;
		}

	}

	void triggerNote() {
		noteElapsed = 0.f;
		noteActive = true;
		for (int i = 0; i < OP_COUNT; i++) {
			feedbackState[i] *= 0.15f;
		}
	}

	void randomizeStepState(StepState& step) {
		step.algorithm = (int)std::floor(random::uniform() * (float)NUM_ALGORITHMS);
		step.baseFrequency = rescale(random::uniform(), 0.f, 1.f, 35.f, 880.f);
		for (int op = 0; op < OP_COUNT; op++) {
			step.ops[op].ratio = 0.125f + random::uniform() * (12.f - 0.125f);
			step.ops[op].level = random::uniform();
			step.ops[op].wave = (int)std::floor(random::uniform() * 4.f);
			step.ops[op].ampAttack = 0.001f + random::uniform() * (0.2f - 0.001f);
			step.ops[op].ampDecay = 0.03f + random::uniform() * (2.5f - 0.03f);
			step.ops[op].pitchAttack = 0.001f + random::uniform() * (0.15f - 0.001f);
			step.ops[op].pitchDecay = 0.02f + random::uniform() * (2.0f - 0.02f);
			step.ops[op].pitchAmountOct = rescale(random::uniform(), 0.f, 1.f, -3.f, 3.f);
			if (op == 0) {
				step.ops[op].index = 0.f;
				step.ops[op].feedback = random::uniform() * 0.2f;
			}
			else {
				step.ops[op].index = random::uniform() * 6.f;
				step.ops[op].feedback = random::uniform() * 0.35f;
			}
		}
	}

	void applyStepState(int stepIndex) {
		if (stepIndex < 0 || stepIndex >= MAX_STEPS) return;
		algorithm = steps[stepIndex].algorithm;
		baseFrequency = steps[stepIndex].baseFrequency;
		for (int i = 0; i < OP_COUNT; i++) {
			ops[i] = steps[stepIndex].ops[i];
		}
	}

	void randomizeAllSteps() {
		for (int s = 0; s < MAX_STEPS; s++) {
			randomizeStepState(steps[s]);
		}
		if (currentStep >= 0 && currentStep < MAX_STEPS) {
			applyStepState(currentStep);
		}
		else {
			applyStepState(0);
		}
	}

	float envAD(float t, float attack, float decay) {
		float safeAttack = std::max(attack, 0.0001f);
		if (t < safeAttack) {
			return clampfjw(t / safeAttack, 0.f, 1.f);
		}
		float safeDecay = std::max(decay, 0.0001f);
		return clampfjw(1.f - ((t - safeAttack) / safeDecay), 0.f, 1.f);
	}

	float shape(float phaseValue, int wave) {
		float x = std::fmod(phaseValue, 2.f * (float)M_PI);
		if (x < 0.f) {
			x += 2.f * (float)M_PI;
		}
		switch (wave) {
			case WAVE_TRI: {
				float n = x / (float)M_PI;
				return (n < 1.f) ? (2.f * n - 1.f) : (3.f - 2.f * n);
			}
			case WAVE_SAW:
				return (x / (float)M_PI) - 1.f;
			case WAVE_SQUARE:
				return x < (float)M_PI ? 1.f : -1.f;
			case WAVE_SINE:
			default:
				return std::sin(x);
		}
	}

	float opPhaseMod(int op, const float* opSignal) {
		switch (algorithm) {
			case ALG_STACK:
				if (op == 0) return opSignal[1] * ops[1].index * 6.f;
				if (op == 1) return opSignal[2] * ops[2].index * 6.f;
				if (op == 2) return opSignal[3] * ops[3].index * 6.f;
				return 0.f;
			case ALG_TWO_STACKS:
				if (op == 0) return opSignal[1] * ops[1].index * 6.f;
				if (op == 2) return opSignal[3] * ops[3].index * 6.f;
				return 0.f;
			case ALG_TRIPLE_INTO_CARRIER:
				if (op == 0) return (opSignal[1] * ops[1].index + opSignal[2] * ops[2].index + opSignal[3] * ops[3].index) * 4.f;
				return 0.f;
			case ALG_TWO_INTO_CARRIER_PLUS_ONE:
				if (op == 0) return (opSignal[2] * ops[2].index + opSignal[3] * ops[3].index) * 5.f;
				return 0.f;
			case ALG_PARALLEL_PAIR:
				if (op == 0) return opSignal[2] * ops[2].index * 6.f;
				if (op == 1) return opSignal[3] * ops[3].index * 6.f;
				return 0.f;
			case ALG_CASCADE_PLUS_FM_BRANCH:
				if (op == 0) return (opSignal[1] * ops[1].index + opSignal[3] * ops[3].index) * 4.f;
				if (op == 1) return opSignal[2] * ops[2].index * 6.f;
				return 0.f;
			default:
				return 0.f;
		}
	}

	float mixOutput(const float* opSignal) {
		switch (algorithm) {
			case ALG_STACK:
			case ALG_TRIPLE_INTO_CARRIER:
			case ALG_TWO_INTO_CARRIER_PLUS_ONE:
			case ALG_CASCADE_PLUS_FM_BRANCH:
				return opSignal[0];
			case ALG_TWO_STACKS:
				return 0.7f * opSignal[0] + 0.7f * opSignal[2];
			case ALG_PARALLEL_PAIR:
				return 0.7f * opSignal[0] + 0.7f * opSignal[1];
			default:
				return opSignal[0];
		}
	}

	void process(const ProcessArgs& args) override {
		bool randomizeInputRising = false;
		float randomizeInputVoltage = inputs[RANDOMIZE_TRIGGER_INPUT].getVoltage();
		bool randomizeInputHigh = randomizeInputVoltage >= 1.f;
		if (!randomizeInputPrimed) {
			randomizeInputWasHigh = randomizeInputHigh;
			randomizeInputPrimed = true;
		}
		else {
			randomizeInputRising = (!randomizeInputWasHigh && randomizeInputHigh);
			randomizeInputWasHigh = randomizeInputHigh;
		}

		if (diceButtonTrig.process(params[DICE_PARAM].getValue()) || randomizeInputRising) {
			randomizeAllSteps();
		}
		if (triggerInputTrig.process(inputs[TRIGGER_INPUT].getVoltage())) {
			int stepCount = clampijw((int)std::round(params[STEP_COUNT_PARAM].getValue()), 1, MAX_STEPS);
			currentStep = (currentStep + 1) % stepCount;
			applyStepState(currentStep);
			triggerNote();
		}

		float output = 0.f;
		if (noteActive) {
			noteElapsed += args.sampleTime;
			float noteLength = std::max(params[LENGTH_PARAM].getValue(), 0.005f);
			float noteEnv = clampfjw(1.f - (noteElapsed / noteLength), 0.f, 1.f);
			if (noteEnv <= 0.f) {
				noteActive = false;
			}
			else {
				float opSignal[OP_COUNT] = {};
				for (int op = OP_COUNT - 1; op >= 0; op--) {
					float ampEnv = envAD(noteElapsed, ops[op].ampAttack, ops[op].ampDecay);
					float pitchEnv = envAD(noteElapsed, ops[op].pitchAttack, ops[op].pitchDecay);
					float pitchMul = std::pow(2.f, pitchEnv * ops[op].pitchAmountOct);
					float opFreq = baseFrequency * ops[op].ratio * pitchMul;
					float phaseMod = opPhaseMod(op, opSignal);
					float selfFeedbackMod = feedbackState[op] * ops[op].feedback * 4.f;
					phase[op] += 2.f * (float)M_PI * opFreq * args.sampleTime;
					float osc = shape(phase[op] + phaseMod + selfFeedbackMod, ops[op].wave);
					opSignal[op] = osc * ops[op].level * ampEnv;
					// Gentle one-pole smoothing + soft clipping keeps feedback rich without getting noisy.
					float fbInput = std::tanh(opSignal[op] * 1.8f);
					feedbackState[op] = feedbackState[op] * 0.9f + fbInput * 0.1f;
				}
				output = clampfjw(mixOutput(opSignal) * noteEnv * 14.0f, -10.f, 10.f);
			}
		}

		// Final anti-click stage: short smoothing and DC blocking before clamp.
		float smoothAlpha = 1.f - std::exp(-args.sampleTime / 0.0022f);
		outputSmooth += (output - outputSmooth) * smoothAlpha;
		float dcOut = outputSmooth - dcBlockX1 + 0.995f * dcBlockY1;
		dcBlockX1 = outputSmooth;
		dcBlockY1 = dcOut;
		output = clampfjw(dcOut * 5.0f, -10.f, 10.f);

		outputs[AUDIO_OUTPUT].setVoltage(output);
		outputs[AUDIO_OUTPUT].setChannels(1);
	}
};

struct FM4DiceWidget : ModuleWidget {
	FM4DiceWidget(FM4Dice* module) {
		setModule(module);
		box.size = Vec(RACK_GRID_WIDTH * 3, RACK_GRID_HEIGHT);

		setPanel(createPanel(
			asset::plugin(pluginInstance, "res/FM4Dice.svg"),
			asset::plugin(pluginInstance, "res/FM4Dice.svg")
		));

		addChild(createWidget<Screw_J>(Vec(16, 2)));
		addChild(createWidget<Screw_W>(Vec(box.size.x - 29, 365)));

		addParam(createParamCentered<SmallWhiteKnob>(Vec(22.f, 96.f), module, FM4Dice::STEP_COUNT_PARAM));
		addParam(createParamCentered<SmallButton>(Vec(22.f, 160.f), module, FM4Dice::DICE_PARAM));
		addInput(createInputCentered<TinyPJ301MPort>(Vec(22.f, 185.f), module, FM4Dice::RANDOMIZE_TRIGGER_INPUT));
		addParam(createParamCentered<SmallWhiteKnob>(Vec(22.f, 231.f), module, FM4Dice::LENGTH_PARAM));
		addInput(createInputCentered<PJ301MPort>(Vec(22.f, 290.f), module, FM4Dice::TRIGGER_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(Vec(22.f, 335.f), module, FM4Dice::AUDIO_OUTPUT));
	}
};

Model* modelFM4Dice = createModel<FM4Dice, FM4DiceWidget>("FM4Dice");
