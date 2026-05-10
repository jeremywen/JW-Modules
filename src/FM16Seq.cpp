#include "JWModules.hpp"
#include <cmath>

struct FM16Seq : Module {
	static constexpr int STEPS = 16;
	static constexpr float TWO_PI = 6.28318530718f;

	enum ParamIds {
		RESET_BUTTON_PARAM,
		MASTER_LEVEL_PARAM,
		SEQUENCE_LENGTH_PARAM,
		EDIT_ACTIVE_PARAM,
		EDIT_PITCH_PARAM,
		EDIT_CAR_RATIO_PARAM,
		EDIT_MOD_RATIO_PARAM,
		EDIT_MOD_FEEDBACK_PARAM,
		EDIT_FM_INDEX_PARAM,
		EDIT_CAR_ATTACK_PARAM,
		EDIT_CAR_DECAY_PARAM,
		EDIT_CAR_SUSTAIN_PARAM,
		EDIT_CAR_RELEASE_PARAM,
		EDIT_MOD_ATTACK_PARAM,
		EDIT_MOD_DECAY_PARAM,
		EDIT_MOD_SUSTAIN_PARAM,
		EDIT_MOD_RELEASE_PARAM,
		EDIT_LEVEL_PARAM,
		STEP_SELECT_PARAM,
		RANDOMIZE_RATIOS_PARAM = STEP_SELECT_PARAM + STEPS,
		RANDOMIZE_DIVISIONS_PARAM,
		RANDOMIZE_ENVELOPES_PARAM,
		RANDOMIZE_PITCHES_PARAM,
		RANDOMIZE_INDEXES_PARAM,
		RANDOMIZE_LEVELS_PARAM,
		RANDOMIZE_FEEDBACK_PARAM,
		INITIALIZE_RATIOS_PARAM,
		INITIALIZE_DIVISIONS_PARAM,
		INITIALIZE_ENVELOPES_PARAM,
		INITIALIZE_PITCHES_PARAM,
		INITIALIZE_INDEXES_PARAM,
		INITIALIZE_LEVELS_PARAM,
		INITIALIZE_FEEDBACK_PARAM,
		INTEGER_RATIOS_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT,
		RESET_INPUT,
		LENGTH_INPUT,
		PITCH_INPUT,
		FM_INDEX_INPUT,
		MOD_RATIO_INPUT,
		CAR_RATIO_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		AUDIO_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		STEP_PLAY_LIGHT,
		STEP_EDIT_LIGHT = STEP_PLAY_LIGHT + STEPS,
		NUM_LIGHTS = STEP_EDIT_LIGHT + STEPS
	};

	struct StepData {
		int division = 1;
		float pitch = 0.f;
		float carRatio = 1.f;
		float modRatio = 2.f;
		float modFeedback = 0.f;
		float fmIndex = 1.5f;
		float carAttack = 0.04f;
		float carDecay = 0.16f;
		float carSustain = 0.05f;
		float carRelease = 0.12f;
		float modAttack = 0.03f;
		float modDecay = 0.12f;
		float modSustain = 0.0f;
		float modRelease = 0.1f;
		float level = 0.8f;
	};

	struct ADSR {
		enum Stage {
			IDLE,
			ATTACK,
			DECAY,
			SUSTAIN,
			RELEASE
		};
		Stage stage = IDLE;
		float value = 0.f;

		void gateOn() {
			stage = ATTACK;
		}

		void gateOff() {
			if (stage != IDLE) {
				stage = RELEASE;
			}
		}

		float process(float sampleRate, float attack, float decay, float sustain, float release) {
			attack = std::max(attack, 0.001f);
			decay = std::max(decay, 0.001f);
			release = std::max(release, 0.001f);
			sustain = clampfjw(sustain, 0.f, 1.f);

			switch (stage) {
				case IDLE:
					value = 0.f;
					break;
				case ATTACK:
					value += 1.f / (attack * sampleRate);
					if (value >= 1.f) {
						value = 1.f;
						stage = DECAY;
					}
					break;
				case DECAY:
					value -= (1.f - sustain) / (decay * sampleRate);
					if (value <= sustain) {
						value = sustain;
						stage = SUSTAIN;
					}
					break;
				case SUSTAIN:
					value = sustain;
					break;
				case RELEASE:
					value -= 1.f / (release * sampleRate);
					if (value <= 0.f) {
						value = 0.f;
						stage = IDLE;
					}
					break;
			}
			return value;
		}
	};

	dsp::SchmittTrigger clockTrigger;
	dsp::SchmittTrigger resetTrigger;
	dsp::SchmittTrigger stepSelectTrigger[STEPS];
	dsp::SchmittTrigger actionTrigger[14];

	int stepIndex = 15;
	bool pendingReset = true;
	int latchedStep = 0;
	int selectedStep = 0;
	int sequenceLength = 16;
	bool integerRatiosMode = false;
	bool gateHeld = false;
	float baseFreq = 261.6256f;
	float carrierPhase = 0.f;
	float modPhase = 0.f;
	StepData stepData[STEPS];
	int stepHits[STEPS] = {};

	// Polyphonic FM engines (one per channel)
	struct FMEngine {
		float carrierPhase = 0.f;
		float modPhase = 0.f;
		float modFeedbackDelayedSample = 0.f;
		ADSR carrierEnv;
		ADSR modEnv;
		float baseFreq = 261.6256f;
		bool gateHeld = false;
	};
	FMEngine engines[STEPS];

	ADSR carrierEnv;
	ADSR modEnv;

	FM16Seq() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(RESET_BUTTON_PARAM, 0.f, 1.f, 0.f, "Reset");
		configParam(MASTER_LEVEL_PARAM, 0.f, 1.f, 0.7f, "Master level");
		configParam(SEQUENCE_LENGTH_PARAM, 1.f, 16.f, 16.f, "Sequence length");
		paramQuantities[SEQUENCE_LENGTH_PARAM]->snapEnabled = true;
		configParam(EDIT_ACTIVE_PARAM, 0.f, 4.f, 1.f, "Step division");
		paramQuantities[EDIT_ACTIVE_PARAM]->snapEnabled = true;
		configParam(EDIT_PITCH_PARAM, -24.f, 24.f, 0.f, "Pitch", " semitones");
		configParam(EDIT_CAR_RATIO_PARAM, 0.125f, 16.f, 1.f, "Carrier ratio");
		configParam(EDIT_MOD_RATIO_PARAM, 0.125f, 16.f, 2.f, "Mod ratio");
		configParam(EDIT_MOD_FEEDBACK_PARAM, 0.f, 1.f, 0.f, "Mod feedback");
		configParam(EDIT_FM_INDEX_PARAM, 0.f, 10.f, 1.5f, "FM index");
		configParam(EDIT_CAR_ATTACK_PARAM, 0.f, 1.f, 0.03f, "Carrier attack");
		configParam(EDIT_CAR_DECAY_PARAM, 0.f, 1.f, 0.2f, "Carrier decay");
		configParam(EDIT_CAR_SUSTAIN_PARAM, 0.f, 1.f, 0.8f, "Carrier sustain");
		configParam(EDIT_CAR_RELEASE_PARAM, 0.f, 1.f, 0.25f, "Carrier release");
		configParam(EDIT_MOD_ATTACK_PARAM, 0.f, 1.f, 0.02f, "Mod attack");
		configParam(EDIT_MOD_DECAY_PARAM, 0.f, 1.f, 0.18f, "Mod decay");
		configParam(EDIT_MOD_SUSTAIN_PARAM, 0.f, 1.f, 0.6f, "Mod sustain");
		configParam(EDIT_MOD_RELEASE_PARAM, 0.f, 1.f, 0.2f, "Mod release");
		configParam(EDIT_LEVEL_PARAM, 0.f, 1.f, 0.8f, "Step level");
		for (int i = 0; i < STEPS; i++) {
			configParam(STEP_SELECT_PARAM + i, 0.f, 1.f, 0.f, string::f("Select step %d", i + 1));
		}
		configParam(RANDOMIZE_RATIOS_PARAM, 0.f, 1.f, 0.f, "Randomize ratios");
		configParam(RANDOMIZE_DIVISIONS_PARAM, 0.f, 1.f, 0.f, "Randomize step divisions");
		configParam(RANDOMIZE_ENVELOPES_PARAM, 0.f, 1.f, 0.f, "Randomize envelopes");
		configParam(RANDOMIZE_PITCHES_PARAM, 0.f, 1.f, 0.f, "Randomize pitches");
		configParam(RANDOMIZE_INDEXES_PARAM, 0.f, 1.f, 0.f, "Randomize indexes");
		configParam(RANDOMIZE_LEVELS_PARAM, 0.f, 1.f, 0.f, "Randomize levels");
		configParam(RANDOMIZE_FEEDBACK_PARAM, 0.f, 1.f, 0.f, "Randomize feedback");
		configParam(INITIALIZE_RATIOS_PARAM, 0.f, 1.f, 0.f, "Initialize ratios");
		configParam(INITIALIZE_DIVISIONS_PARAM, 0.f, 1.f, 0.f, "Initialize step divisions");
		configParam(INITIALIZE_ENVELOPES_PARAM, 0.f, 1.f, 0.f, "Initialize envelopes");
		configParam(INITIALIZE_PITCHES_PARAM, 0.f, 1.f, 0.f, "Initialize pitches");
		configParam(INITIALIZE_INDEXES_PARAM, 0.f, 1.f, 0.f, "Initialize indexes");
		configParam(INITIALIZE_LEVELS_PARAM, 0.f, 1.f, 0.f, "Initialize levels");
		configParam(INITIALIZE_FEEDBACK_PARAM, 0.f, 1.f, 0.f, "Initialize feedback");
		configParam(INTEGER_RATIOS_PARAM, 0.f, 1.f, 0.f, "Integer ratios mode");
		paramQuantities[INTEGER_RATIOS_PARAM]->snapEnabled = true;

		configInput(CLOCK_INPUT, "Clock");
		configInput(RESET_INPUT, "Reset");
		configInput(LENGTH_INPUT, "Sequence length CV");
		configInput(PITCH_INPUT, "V/Oct pitch (16 poly channels)");
		configInput(FM_INDEX_INPUT, "FM index CV (16 poly channels)");
		configInput(MOD_RATIO_INPUT, "Mod ratio CV (16 poly channels)");
		configInput(CAR_RATIO_INPUT, "Carrier ratio CV (16 poly channels)");

		configOutput(AUDIO_OUTPUT, "Audio");
		for (int i = 0; i < STEPS; i++) {
			stepData[i].division = 1;
			stepData[i].carAttack = 0.04f;
			stepData[i].carDecay = 0.16f;
			stepData[i].carSustain = 0.05f;
			stepData[i].carRelease = 0.12f;
			stepData[i].modAttack = 0.03f;
			stepData[i].modDecay = 0.12f;
			stepData[i].modSustain = 0.0f;
			stepData[i].modRelease = 0.1f;
			stepHits[i] = 0;
		}
		loadEditorFromSelectedStep();
	}

	void loadEditorFromSelectedStep() {
		StepData& s = stepData[selectedStep];
		params[EDIT_ACTIVE_PARAM].setValue((float)s.division);
		params[EDIT_PITCH_PARAM].setValue(s.pitch);
		params[EDIT_CAR_RATIO_PARAM].setValue(s.carRatio);
		params[EDIT_MOD_RATIO_PARAM].setValue(s.modRatio);
		params[EDIT_MOD_FEEDBACK_PARAM].setValue(s.modFeedback);
		params[EDIT_FM_INDEX_PARAM].setValue(s.fmIndex);
		params[EDIT_CAR_ATTACK_PARAM].setValue(s.carAttack);
		params[EDIT_CAR_DECAY_PARAM].setValue(s.carDecay);
		params[EDIT_CAR_SUSTAIN_PARAM].setValue(s.carSustain);
		params[EDIT_CAR_RELEASE_PARAM].setValue(s.carRelease);
		params[EDIT_MOD_ATTACK_PARAM].setValue(s.modAttack);
		params[EDIT_MOD_DECAY_PARAM].setValue(s.modDecay);
		params[EDIT_MOD_SUSTAIN_PARAM].setValue(s.modSustain);
		params[EDIT_MOD_RELEASE_PARAM].setValue(s.modRelease);
		params[EDIT_LEVEL_PARAM].setValue(s.level);
	}

	void saveEditorToSelectedStep() {
		StepData& s = stepData[selectedStep];
		s.division = clampijw((int) std::round(params[EDIT_ACTIVE_PARAM].getValue()), 0, 4);
		s.pitch = params[EDIT_PITCH_PARAM].getValue();
		s.carRatio = params[EDIT_CAR_RATIO_PARAM].getValue();
		s.modRatio = params[EDIT_MOD_RATIO_PARAM].getValue();
		s.modFeedback = params[EDIT_MOD_FEEDBACK_PARAM].getValue();
		s.fmIndex = params[EDIT_FM_INDEX_PARAM].getValue();
		s.carAttack = params[EDIT_CAR_ATTACK_PARAM].getValue();
		s.carDecay = params[EDIT_CAR_DECAY_PARAM].getValue();
		s.carSustain = params[EDIT_CAR_SUSTAIN_PARAM].getValue();
		s.carRelease = params[EDIT_CAR_RELEASE_PARAM].getValue();
		s.modAttack = params[EDIT_MOD_ATTACK_PARAM].getValue();
		s.modDecay = params[EDIT_MOD_DECAY_PARAM].getValue();
		s.modSustain = params[EDIT_MOD_SUSTAIN_PARAM].getValue();
		s.modRelease = params[EDIT_MOD_RELEASE_PARAM].getValue();
		s.level = params[EDIT_LEVEL_PARAM].getValue();
	}

	float envKnobToSeconds(float knobValue) {
		// Square mapping gives better precision at short percussive envelope times.
		return 0.001f + (knobValue * knobValue) * 4.0f;
	}

	void onReset() override {
		sequenceLength = 16;
		stepIndex = 15;
		latchedStep = 0;
		selectedStep = 0;
		pendingReset = true;
		gateHeld = false;
		carrierPhase = 0.f;
		modPhase = 0.f;
		carrierEnv = ADSR();
		modEnv = ADSR();
		for (int i = 0; i < STEPS; i++) {
			engines[i] = FMEngine();
			stepData[i].division = 1;
			stepData[i].pitch = 0.f;
			stepData[i].carRatio = 1.f;
			stepData[i].modRatio = 2.f;
			stepData[i].modFeedback = 0.f;
			stepData[i].fmIndex = 1.5f;
			stepData[i].carAttack = 0.04f;
			stepData[i].carDecay = 0.16f;
			stepData[i].carSustain = 0.05f;
			stepData[i].carRelease = 0.12f;
			stepData[i].modAttack = 0.03f;
			stepData[i].modDecay = 0.12f;
			stepData[i].modSustain = 0.0f;
			stepData[i].modRelease = 0.1f;
			stepData[i].level = 0.8f;
			stepHits[i] = 0;
		}
		loadEditorFromSelectedStep();
	}

	void onRandomize() override {
		for (int i = 0; i < STEPS; i++) {
			stepData[i].division = (int) std::floor(random::uniform() * 5.f);
			stepData[i].pitch = std::round((random::uniform() * 48.f) - 24.f);
			stepData[i].carRatio = 0.5f + random::uniform() * 4.f;
			stepData[i].modRatio = 0.5f + random::uniform() * 6.f;
			stepData[i].modFeedback = random::uniform();
			stepData[i].fmIndex = random::uniform() * 4.f;
			stepData[i].carAttack = random::uniform();
			stepData[i].carDecay = random::uniform();
			stepData[i].carSustain = random::uniform();
			stepData[i].carRelease = random::uniform();
			stepData[i].modAttack = random::uniform();
			stepData[i].modDecay = random::uniform();
			stepData[i].modSustain = random::uniform();
			stepData[i].modRelease = random::uniform();
			stepData[i].level = 0.2f + random::uniform() * 0.8f;
		}
		loadEditorFromSelectedStep();
	}

	void randomizeRatiosOnly() {
		for (int i = 0; i < STEPS; i++) {
			if (integerRatiosMode) {
				stepData[i].carRatio = (float)(1 + rack::random::u32() % 8);
				stepData[i].modRatio = (float)(1 + rack::random::u32() % 8);
			} else {
				stepData[i].carRatio = 0.5f + random::uniform() * 4.f;
				stepData[i].modRatio = 0.5f + random::uniform() * 6.f;
			}
		}
		loadEditorFromSelectedStep();
	}

	void initializeRatiosOnly() {
		for (int i = 0; i < STEPS; i++) {
			stepData[i].carRatio = 1.f;
			stepData[i].modRatio = 2.f;
		}
		loadEditorFromSelectedStep();
	}

	void randomizeStepDivisionsOnly() {
		for (int i = 0; i < STEPS; i++) {
			stepData[i].division = (int) std::floor(random::uniform() * 5.f);
			stepHits[i] = 0;
		}
		loadEditorFromSelectedStep();
	}

	void initializeStepDivisionsOnly() {
		for (int i = 0; i < STEPS; i++) {
			stepData[i].division = 1;
			stepHits[i] = 0;
		}
		loadEditorFromSelectedStep();
	}

	void randomizeEnvelopesOnly() {
		for (int i = 0; i < STEPS; i++) {
			stepData[i].carAttack = random::uniform();
			stepData[i].carDecay = random::uniform();
			stepData[i].carSustain = random::uniform();
			stepData[i].carRelease = random::uniform();
			stepData[i].modAttack = random::uniform();
			stepData[i].modDecay = random::uniform();
			stepData[i].modSustain = random::uniform();
			stepData[i].modRelease = random::uniform();
		}
		loadEditorFromSelectedStep();
	}

	void initializeEnvelopesOnly() {
		for (int i = 0; i < STEPS; i++) {
			stepData[i].carAttack = 0.04f;
			stepData[i].carDecay = 0.16f;
			stepData[i].carSustain = 0.05f;
			stepData[i].carRelease = 0.12f;
			stepData[i].modAttack = 0.03f;
			stepData[i].modDecay = 0.12f;
			stepData[i].modSustain = 0.0f;
			stepData[i].modRelease = 0.1f;
		}
		loadEditorFromSelectedStep();
	}

	void randomizePitchesOnly() {
		for (int i = 0; i < STEPS; i++) {
			stepData[i].pitch = std::round((random::uniform() * 48.f) - 24.f);
		}
		loadEditorFromSelectedStep();
	}

	void initializePitchesOnly() {
		for (int i = 0; i < STEPS; i++) {
			stepData[i].pitch = 0.f;
		}
		loadEditorFromSelectedStep();
	}

	void randomizeIndexesOnly() {
		for (int i = 0; i < STEPS; i++) {
			stepData[i].fmIndex = random::uniform() * 4.f;
		}
		loadEditorFromSelectedStep();
	}

	void initializeIndexesOnly() {
		for (int i = 0; i < STEPS; i++) {
			stepData[i].fmIndex = 1.5f;
		}
		loadEditorFromSelectedStep();
	}

	void randomizeLevelsOnly() {
		for (int i = 0; i < STEPS; i++) {
			stepData[i].level = 0.2f + random::uniform() * 0.8f;
		}
		loadEditorFromSelectedStep();
	}

	void initializeLevelsOnly() {
		for (int i = 0; i < STEPS; i++) {
			stepData[i].level = 0.8f;
		}
		loadEditorFromSelectedStep();
	}

	void randomizeFeedbackOnly() {
		for (int i = 0; i < STEPS; i++) {
			stepData[i].modFeedback = random::uniform();
		}
		loadEditorFromSelectedStep();
	}

	void initializeFeedbackOnly() {
		for (int i = 0; i < STEPS; i++) {
			stepData[i].modFeedback = 0.f;
		}
		loadEditorFromSelectedStep();
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "selectedStep", json_integer(selectedStep));
		json_object_set_new(rootJ, "sequenceLength", json_integer(sequenceLength));
		json_object_set_new(rootJ, "integerRatiosMode", json_boolean(integerRatiosMode));
		json_t* stepsJ = json_array();
		for (int i = 0; i < STEPS; i++) {
			json_t* stepJ = json_object();
			json_object_set_new(stepJ, "division", json_integer(stepData[i].division));
			json_object_set_new(stepJ, "pitch", json_real(stepData[i].pitch));
			json_object_set_new(stepJ, "carRatio", json_real(stepData[i].carRatio));
			json_object_set_new(stepJ, "modRatio", json_real(stepData[i].modRatio));
			json_object_set_new(stepJ, "modFeedback", json_real(stepData[i].modFeedback));
			json_object_set_new(stepJ, "fmIndex", json_real(stepData[i].fmIndex));
			json_object_set_new(stepJ, "carAttack", json_real(stepData[i].carAttack));
			json_object_set_new(stepJ, "carDecay", json_real(stepData[i].carDecay));
			json_object_set_new(stepJ, "carSustain", json_real(stepData[i].carSustain));
			json_object_set_new(stepJ, "carRelease", json_real(stepData[i].carRelease));
			json_object_set_new(stepJ, "modAttack", json_real(stepData[i].modAttack));
			json_object_set_new(stepJ, "modDecay", json_real(stepData[i].modDecay));
			json_object_set_new(stepJ, "modSustain", json_real(stepData[i].modSustain));
			json_object_set_new(stepJ, "modRelease", json_real(stepData[i].modRelease));
			json_object_set_new(stepJ, "level", json_real(stepData[i].level));
			json_array_append_new(stepsJ, stepJ);
		}
		json_object_set_new(rootJ, "steps", stepsJ);
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* selectedStepJ = json_object_get(rootJ, "selectedStep");
		if (selectedStepJ) {
			selectedStep = clampijw((int)json_integer_value(selectedStepJ), 0, STEPS - 1);
		}
		json_t* sequenceLengthJ = json_object_get(rootJ, "sequenceLength");
		if (sequenceLengthJ) {
			sequenceLength = clampijw((int)json_integer_value(sequenceLengthJ), 1, STEPS);
		}
		json_t* integerRatiosModeJ = json_object_get(rootJ, "integerRatiosMode");
		if (integerRatiosModeJ) {
			integerRatiosMode = json_is_true(integerRatiosModeJ);
		}
		json_t* stepsJ = json_object_get(rootJ, "steps");
		if (stepsJ && json_is_array(stepsJ)) {
			for (int i = 0; i < STEPS; i++) {
				json_t* stepJ = json_array_get(stepsJ, i);
				if (!stepJ) continue;
				json_t* v;
				v = json_object_get(stepJ, "division");
				if (v) {
					stepData[i].division = clampijw((int)json_integer_value(v), 0, 4);
				}
				else {
					// Backward compatibility with older patches that stored active boolean.
					v = json_object_get(stepJ, "active");
					if (v) stepData[i].division = json_is_true(v) ? 1 : 0;
				}
				v = json_object_get(stepJ, "pitch"); if (v) stepData[i].pitch = (float)json_number_value(v);
				v = json_object_get(stepJ, "carRatio"); if (v) stepData[i].carRatio = (float)json_number_value(v);
				v = json_object_get(stepJ, "modRatio"); if (v) stepData[i].modRatio = (float)json_number_value(v);
				v = json_object_get(stepJ, "modFeedback"); if (v) stepData[i].modFeedback = (float)json_number_value(v);
				v = json_object_get(stepJ, "fmIndex"); if (v) stepData[i].fmIndex = (float)json_number_value(v);
				v = json_object_get(stepJ, "carAttack"); if (v) stepData[i].carAttack = (float)json_number_value(v);
				v = json_object_get(stepJ, "carDecay"); if (v) stepData[i].carDecay = (float)json_number_value(v);
				v = json_object_get(stepJ, "carSustain"); if (v) stepData[i].carSustain = (float)json_number_value(v);
				v = json_object_get(stepJ, "carRelease"); if (v) stepData[i].carRelease = (float)json_number_value(v);
				v = json_object_get(stepJ, "modAttack"); if (v) stepData[i].modAttack = (float)json_number_value(v);
				v = json_object_get(stepJ, "modDecay"); if (v) stepData[i].modDecay = (float)json_number_value(v);
				v = json_object_get(stepJ, "modSustain"); if (v) stepData[i].modSustain = (float)json_number_value(v);
				v = json_object_get(stepJ, "modRelease"); if (v) stepData[i].modRelease = (float)json_number_value(v);
				v = json_object_get(stepJ, "level"); if (v) stepData[i].level = (float)json_number_value(v);
			}
		}
		loadEditorFromSelectedStep();
	}

	void process(const ProcessArgs& args) override {
		bool stepChanged = false;

		// Check for step selection changes.
		for (int i = 0; i < STEPS; i++) {
			if (stepSelectTrigger[i].process(params[STEP_SELECT_PARAM + i].getValue())) {
				if (i != selectedStep) {
					saveEditorToSelectedStep();
					selectedStep = i;
					loadEditorFromSelectedStep();
					stepChanged = true;
				}
			}
		}
		if (!stepChanged) {
			saveEditorToSelectedStep();
		}

		// Update sequence length from knob + CV (1V per step)
		float lengthControl = params[SEQUENCE_LENGTH_PARAM].getValue() + inputs[LENGTH_INPUT].getVoltage();
		sequenceLength = (int) std::round(lengthControl);
		sequenceLength = clampijw(sequenceLength, 1, STEPS);

		// Update integer ratios mode from switch
		integerRatiosMode = params[INTEGER_RATIOS_PARAM].getValue() > 0.5f;

		if (actionTrigger[0].process(params[RANDOMIZE_RATIOS_PARAM].getValue())) randomizeRatiosOnly();
		if (actionTrigger[1].process(params[RANDOMIZE_DIVISIONS_PARAM].getValue())) randomizeStepDivisionsOnly();
		if (actionTrigger[2].process(params[RANDOMIZE_ENVELOPES_PARAM].getValue())) randomizeEnvelopesOnly();
		if (actionTrigger[3].process(params[RANDOMIZE_PITCHES_PARAM].getValue())) randomizePitchesOnly();
		if (actionTrigger[4].process(params[RANDOMIZE_INDEXES_PARAM].getValue())) randomizeIndexesOnly();
		if (actionTrigger[5].process(params[RANDOMIZE_LEVELS_PARAM].getValue())) randomizeLevelsOnly();
		if (actionTrigger[6].process(params[RANDOMIZE_FEEDBACK_PARAM].getValue())) randomizeFeedbackOnly();
		if (actionTrigger[7].process(params[INITIALIZE_RATIOS_PARAM].getValue())) initializeRatiosOnly();
		if (actionTrigger[8].process(params[INITIALIZE_DIVISIONS_PARAM].getValue())) initializeStepDivisionsOnly();
		if (actionTrigger[9].process(params[INITIALIZE_ENVELOPES_PARAM].getValue())) initializeEnvelopesOnly();
		if (actionTrigger[10].process(params[INITIALIZE_PITCHES_PARAM].getValue())) initializePitchesOnly();
		if (actionTrigger[11].process(params[INITIALIZE_INDEXES_PARAM].getValue())) initializeIndexesOnly();
		if (actionTrigger[12].process(params[INITIALIZE_LEVELS_PARAM].getValue())) initializeLevelsOnly();
		if (actionTrigger[13].process(params[INITIALIZE_FEEDBACK_PARAM].getValue())) initializeFeedbackOnly();

		// Clock and sequence logic
		float clockValue = inputs[CLOCK_INPUT].getVoltage();
		float resetValue = params[RESET_BUTTON_PARAM].getValue() + inputs[RESET_INPUT].getVoltage();

		if (resetTrigger.process(resetValue)) {
			pendingReset = true;
		}

		bool stepTriggered = false;
		if (clockTrigger.process(clockValue)) {
			// Release all gates on next clock tick
			for (int i = 0; i < STEPS; i++) {
				if (engines[i].gateHeld) {
					engines[i].carrierEnv.gateOff();
					engines[i].modEnv.gateOff();
					engines[i].gateHeld = false;
				}
			}

			if (pendingReset) {
				stepIndex = sequenceLength - 1;
				pendingReset = false;
			}
			stepIndex = (stepIndex + 1) % sequenceLength;

			// Check if this step should trigger
			if (stepData[stepIndex].division == 0) {
				stepHits[stepIndex] = 0;
			} else {
				stepHits[stepIndex]++;
				if (stepHits[stepIndex] >= stepData[stepIndex].division) {
					stepHits[stepIndex] = 0;
					latchedStep = stepIndex;
					stepTriggered = true;
				}
			}
		}

		// Update lights
		for (int i = 0; i < STEPS; i++) {
			lights[STEP_PLAY_LIGHT + i].value = (i == stepIndex) ? 1.f : 0.f;
			lights[STEP_EDIT_LIGHT + i].value = (i == selectedStep) ? 1.f : 0.f;
		}

		// Get polyphonic input channel counts
		int pitchChannels = inputs[PITCH_INPUT].getChannels();
		int fmIndexChannels = inputs[FM_INDEX_INPUT].getChannels();
		int modRatioChannels = inputs[MOD_RATIO_INPUT].getChannels();
		int carRatioChannels = inputs[CAR_RATIO_INPUT].getChannels();
		int maxChannels = std::max({pitchChannels, fmIndexChannels, modRatioChannels, carRatioChannels, 1});
		maxChannels = std::min(maxChannels, STEPS);

		// When step triggers, gate on all engines
		if (stepTriggered) {
			for (int ch = 0; ch < maxChannels; ch++) {
				engines[ch].carrierEnv.gateOn();
				engines[ch].modEnv.gateOn();
				engines[ch].gateHeld = true;
			}
		}

		// Process each FM engine for each input channel
		float mixedOut = 0.f;
		for (int ch = 0; ch < maxChannels; ch++) {
			FMEngine& engine = engines[ch];
			const StepData& s = stepData[latchedStep];

			// Get CV values for this channel
			float pitchVolts = inputs[PITCH_INPUT].getVoltage(ch);
			float fmIndexCV = inputs[FM_INDEX_INPUT].getVoltage(ch);
			float modRatioCV = inputs[MOD_RATIO_INPUT].getVoltage(ch);
			float carRatioCV = inputs[CAR_RATIO_INPUT].getVoltage(ch);

			// Combine CV with stored step parameters
			float baseFreqCV = pitchVolts + (s.pitch / 12.f);
			engine.baseFreq = 261.6256f * std::pow(2.f, baseFreqCV);

			float carRatio = s.carRatio + carRatioCV;
			float modRatio = s.modRatio + modRatioCV;
			carRatio = clampfjw(carRatio, 0.125f, 32.f);
			modRatio = clampfjw(modRatio, 0.125f, 32.f);
			float modFeedback = clampfjw(s.modFeedback, 0.f, 1.f);

			float fmIndex = s.fmIndex + fmIndexCV;
			fmIndex = clampfjw(fmIndex, 0.f, 20.f);

			float carAttack = envKnobToSeconds(s.carAttack);
			float carDecay = envKnobToSeconds(s.carDecay);
			float carSustain = s.carSustain;
			float carRelease = envKnobToSeconds(s.carRelease);

			float modAttack = envKnobToSeconds(s.modAttack);
			float modDecay = envKnobToSeconds(s.modDecay);
			float modSustain = s.modSustain;
			float modRelease = envKnobToSeconds(s.modRelease);

			float carEnvValue = engine.carrierEnv.process(args.sampleRate, carAttack, carDecay, carSustain, carRelease);
			float modEnvValue = engine.modEnv.process(args.sampleRate, modAttack, modDecay, modSustain, modRelease);

			engine.modPhase += TWO_PI * (engine.baseFreq * modRatio) / args.sampleRate;
			if (engine.modPhase > TWO_PI)
				engine.modPhase -= TWO_PI;
			float feedbackOffset = 0.f;
			if (modFeedback > 0.001f) {
				feedbackOffset = modFeedback * engine.modFeedbackDelayedSample;
			}
			float modSignal = std::sin(engine.modPhase + feedbackOffset) * modEnvValue;
			engine.modFeedbackDelayedSample = modSignal * 5.f;

			engine.carrierPhase += TWO_PI * (engine.baseFreq * carRatio) / args.sampleRate;
			if (engine.carrierPhase > TWO_PI)
				engine.carrierPhase -= TWO_PI;

			float carrierSignal = std::sin(engine.carrierPhase + (modSignal * fmIndex * TWO_PI));
			float stepLevel = s.level;
			float out = carrierSignal * carEnvValue * stepLevel * 5.0f;
			mixedOut += out / (float)maxChannels;
		}

		float masterOut = mixedOut * params[MASTER_LEVEL_PARAM].getValue();
		outputs[AUDIO_OUTPUT].setVoltage(clampfjw(masterOut, -5.f, 5.f));
		outputs[AUDIO_OUTPUT].setChannels(1);
	}
};

struct FM16SeqWidget : ModuleWidget {
	FM16SeqWidget(FM16Seq* module) {
		setModule(module);
		box.size = Vec(RACK_GRID_WIDTH * 36, RACK_GRID_HEIGHT);

		setPanel(createPanel(
			asset::plugin(pluginInstance, "res/FM16Seq.svg"),
			asset::plugin(pluginInstance, "res/FM16Seq.svg")
		));

		addChild(createWidget<Screw_J>(Vec(16, 2)));
		addChild(createWidget<Screw_J>(Vec(16, 365)));
		addChild(createWidget<Screw_W>(Vec(box.size.x - 29, 2)));
		addChild(createWidget<Screw_W>(Vec(box.size.x - 29, 365)));

		for (int i = 0; i < FM16Seq::STEPS; i++) {
			float x = 20.f + (float)i * 32.f;
			float y = 56.f;
			addParam(createParamCentered<TinyButton>(Vec(x, y), module, FM16Seq::STEP_SELECT_PARAM + i));
			addChild(createLight<SmallLight<MyBlueValueLight>>(Vec(x - 6.f, y - 24.f), module, FM16Seq::STEP_PLAY_LIGHT + i));
			addChild(createLight<SmallLight<MyOrangeValueLight>>(Vec(x + 1.f, y - 24.f), module, FM16Seq::STEP_EDIT_LIGHT + i));
		}

		addParam(createParamCentered<JwSmallSnapKnob>(Vec(84.f, 140.f), module, FM16Seq::EDIT_ACTIVE_PARAM));
		addParam(createParamCentered<SmallWhiteKnob>(Vec(146.f, 140.f), module, FM16Seq::EDIT_PITCH_PARAM));
		addParam(createParamCentered<SmallWhiteKnob>(Vec(208.f, 140.f), module, FM16Seq::EDIT_FM_INDEX_PARAM));
		addParam(createParamCentered<SmallWhiteKnob>(Vec(270.f, 140.f), module, FM16Seq::EDIT_LEVEL_PARAM));

		addParam(createParamCentered<SmallWhiteKnob>(Vec(112.f, 194.f), module, FM16Seq::EDIT_CAR_RATIO_PARAM));
		addParam(createParamCentered<SmallWhiteKnob>(Vec(174.f, 194.f), module, FM16Seq::EDIT_MOD_RATIO_PARAM));
		addParam(createParamCentered<SmallWhiteKnob>(Vec(236.f, 194.f), module, FM16Seq::EDIT_MOD_FEEDBACK_PARAM));

		addParam(createParamCentered<JwTinyGrayKnob>(Vec(84.f, 248.f), module, FM16Seq::EDIT_CAR_ATTACK_PARAM));
		addParam(createParamCentered<JwTinyGrayKnob>(Vec(124.f, 248.f), module, FM16Seq::EDIT_CAR_DECAY_PARAM));
		addParam(createParamCentered<JwTinyGrayKnob>(Vec(164.f, 248.f), module, FM16Seq::EDIT_CAR_SUSTAIN_PARAM));
		addParam(createParamCentered<JwTinyGrayKnob>(Vec(204.f, 248.f), module, FM16Seq::EDIT_CAR_RELEASE_PARAM));

		addParam(createParamCentered<JwTinyGrayKnob>(Vec(84.f, 288.f), module, FM16Seq::EDIT_MOD_ATTACK_PARAM));
		addParam(createParamCentered<JwTinyGrayKnob>(Vec(124.f, 288.f), module, FM16Seq::EDIT_MOD_DECAY_PARAM));
		addParam(createParamCentered<JwTinyGrayKnob>(Vec(164.f, 288.f), module, FM16Seq::EDIT_MOD_SUSTAIN_PARAM));
		addParam(createParamCentered<JwTinyGrayKnob>(Vec(204.f, 288.f), module, FM16Seq::EDIT_MOD_RELEASE_PARAM));

		addParam(createParamCentered<SmallButton>(Vec(400.f, 344.f), module, FM16Seq::RESET_BUTTON_PARAM));
		addParam(createParamCentered<SmallWhiteKnob>(Vec(348.f, 344.f), module, FM16Seq::MASTER_LEVEL_PARAM));
		addParam(createParamCentered<JwSmallSnapKnob>(Vec(310.f, 344.f), module, FM16Seq::SEQUENCE_LENGTH_PARAM));

		addInput(createInputCentered<PJ301MPort>(Vec(42.f, 344.f), module, FM16Seq::CLOCK_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(92.f, 344.f), module, FM16Seq::RESET_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(460.f, 344.f), module, FM16Seq::LENGTH_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(142.f, 344.f), module, FM16Seq::PITCH_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(192.f, 344.f), module, FM16Seq::FM_INDEX_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(242.f, 344.f), module, FM16Seq::MOD_RATIO_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(292.f, 344.f), module, FM16Seq::CAR_RATIO_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(Vec(520.f, 344.f), module, FM16Seq::AUDIO_OUTPUT));

		const float rndX = 334.f;
		const float initX = 394.f;
		const float y0 = 136.f;
		const float yStep = 24.f;

		addParam(createParamCentered<TinyButton>(Vec(rndX, y0 + yStep * 0.f), module, FM16Seq::RANDOMIZE_RATIOS_PARAM));
		addParam(createParamCentered<TinyButton>(Vec(rndX, y0 + yStep * 1.f), module, FM16Seq::RANDOMIZE_DIVISIONS_PARAM));
		addParam(createParamCentered<TinyButton>(Vec(rndX, y0 + yStep * 2.f), module, FM16Seq::RANDOMIZE_ENVELOPES_PARAM));
		addParam(createParamCentered<TinyButton>(Vec(rndX, y0 + yStep * 3.f), module, FM16Seq::RANDOMIZE_PITCHES_PARAM));
		addParam(createParamCentered<TinyButton>(Vec(rndX, y0 + yStep * 4.f), module, FM16Seq::RANDOMIZE_INDEXES_PARAM));
		addParam(createParamCentered<TinyButton>(Vec(rndX, y0 + yStep * 5.f), module, FM16Seq::RANDOMIZE_LEVELS_PARAM));
		addParam(createParamCentered<TinyButton>(Vec(rndX, y0 + yStep * 6.f), module, FM16Seq::RANDOMIZE_FEEDBACK_PARAM));

		// Integer ratios mode switch next to randomize ratios button
		addParam(createParamCentered<JwHorizontalSwitch>(Vec(rndX + 20.f, y0 + yStep * 0.f), module, FM16Seq::INTEGER_RATIOS_PARAM));

		addParam(createParamCentered<TinyButton>(Vec(initX, y0 + yStep * 0.f), module, FM16Seq::INITIALIZE_RATIOS_PARAM));
		addParam(createParamCentered<TinyButton>(Vec(initX, y0 + yStep * 1.f), module, FM16Seq::INITIALIZE_DIVISIONS_PARAM));
		addParam(createParamCentered<TinyButton>(Vec(initX, y0 + yStep * 2.f), module, FM16Seq::INITIALIZE_ENVELOPES_PARAM));
		addParam(createParamCentered<TinyButton>(Vec(initX, y0 + yStep * 3.f), module, FM16Seq::INITIALIZE_PITCHES_PARAM));
		addParam(createParamCentered<TinyButton>(Vec(initX, y0 + yStep * 4.f), module, FM16Seq::INITIALIZE_INDEXES_PARAM));
		addParam(createParamCentered<TinyButton>(Vec(initX, y0 + yStep * 5.f), module, FM16Seq::INITIALIZE_LEVELS_PARAM));
		addParam(createParamCentered<TinyButton>(Vec(initX, y0 + yStep * 6.f), module, FM16Seq::INITIALIZE_FEEDBACK_PARAM));
	}
};

Model* modelFM16Seq = createModel<FM16Seq, FM16SeqWidget>("FM16Seq");
