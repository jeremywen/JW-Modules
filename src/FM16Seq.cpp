#include "JWModules.hpp"
#include <cmath>

struct FM16Seq : Module, QuantizeUtils {
	static constexpr int STEPS = 16;
	static constexpr float TWO_PI = 6.28318530718f;

	enum ParamIds {
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
		RANDOMIZE_INDEXES_PARAM,
		RANDOMIZE_FEEDBACK_PARAM,
		RANDOMIZE_PITCHES_PARAM,
		RANDOMIZE_ENVELOPES_PARAM,
		RANDOMIZE_DIVISIONS_PARAM,
		RANDOMIZE_LEVELS_PARAM,
		INITIALIZE_RATIOS_PARAM,
		INITIALIZE_INDEXES_PARAM,
		INITIALIZE_FEEDBACK_PARAM,
		INITIALIZE_PITCHES_PARAM,
		INITIALIZE_ENVELOPES_PARAM,
		INITIALIZE_DIVISIONS_PARAM,
		INITIALIZE_LEVELS_PARAM,
		RANDOMIZE_STEP_LENGTHS_PARAM,
		INITIALIZE_STEP_LENGTHS_PARAM,
		EDIT_GATE_LENGTH_PARAM,
		INTEGER_RATIOS_PARAM,
		PLAY_MODE_KNOB_PARAM,
		MANUAL_STEP_TRIGGER_PARAM,
		RANDOMIZE_AMOUNT_RATIOS_PARAM,
		RANDOMIZE_AMOUNT_INDEXES_PARAM,
		RANDOMIZE_AMOUNT_FEEDBACK_PARAM,
		RANDOMIZE_AMOUNT_PITCHES_PARAM,
		RANDOMIZE_AMOUNT_ENVELOPES_PARAM,
		RANDOMIZE_AMOUNT_DIVISIONS_PARAM,
		RANDOMIZE_AMOUNT_LEVELS_PARAM,
		RANDOMIZE_AMOUNT_STEP_LENGTHS_PARAM,
		RANDOMIZE_CURRENT_STEP_ONLY_PARAM,
		RANDOMIZE_DIVISIONS_EXCLUDE_ZERO_PARAM,
		RANDOMIZE_ALL_TRIGGER_PARAM,
		NUM_PARAMS // Ensure this is the last entry
	};
	   enum InputIds {
		   CLOCK_INPUT,
		   RESET_INPUT,
		   LENGTH_INPUT,
		   PITCH_INPUT,
		   FM_INDEX_INPUT,
		   FEEDBACK_INPUT,
		   GATE_LENGTH_INPUT,
		   MOD_RATIO_INPUT,
		   CAR_RATIO_INPUT,
		   MODE_CV_INPUT,
		   MANUAL_STEP_GATE_INPUT,
		   RANDOMIZE_ALL_TRIGGER_INPUT,
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
		float gateLengthMs = 100.f; // Default 100 ms
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
	dsp::SchmittTrigger actionTrigger[16];
	dsp::SchmittTrigger manualStepTrigger;
	dsp::SchmittTrigger manualStepGateTrigger;
	dsp::SchmittTrigger randomizeAllButtonTrigger;
	dsp::SchmittTrigger randomizeAllInputTrigger;

	int stepIndex = 0;
	bool pendingReset = true;
	bool goingForward = true;
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
	bool pitchQuantizeEnabled = true;
	int pitchQuantizeRoot = QuantizeUtils::NOTE_C;
	int pitchQuantizeScale = QuantizeUtils::NONE;

	// Polyphonic FM engines (one per channel)
	struct FMEngine {
		float carrierPhase = 0.f;
		float modPhase = 0.f;
		float modFeedbackDelayedSample = 0.f;
		float stepElapsed = 0.f;
		ADSR carrierEnv;
		ADSR modEnv;
		float baseFreq = 261.6256f;
		float smoothedCarRatio = 1.f;
		float smoothedModRatio = 2.f;
		float smoothedFmIndex = 1.5f;
		float smoothedModFeedback = 0.f;
		float smoothedLevel = 0.8f;
		bool gateHeld = false;
	};
	FMEngine engines[STEPS];

	ADSR carrierEnv;
	ADSR modEnv;
	float clockElapsed = 0.f;
	float clockInterval = 0.5f;
	bool clockIntervalValid = false;

	float getRandomizeAmount(int amountParamId) {
		return clampfjw(params[amountParamId].getValue(), 0.f, 1.f);
	}

	// Returns a random value between min and min + (max-min)*amount
	float randomizeMaxPercent(float min, float max, float amount) {
		return min + random::uniform() * ((max - min) * amount);
	}

	float quantizeIntegerRatioModeValue(float value) {
		static const float allowed[] = {
			0.125f, 0.25f, 0.5f,
			1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f, 9.f, 10.f
		};
		float best = allowed[0];
		float bestDist = std::abs(value - best);
		for (size_t i = 1; i < sizeof(allowed) / sizeof(allowed[0]); i++) {
			float d = std::abs(value - allowed[i]);
			if (d < bestDist) {
				bestDist = d;
				best = allowed[i];
			}
		}
		return best;
	}

	bool randomizeCurrentStepOnly() {
		return params[RANDOMIZE_CURRENT_STEP_ONLY_PARAM].value > 0.5f;
	}

	bool randomizeDivisionsExcludeZero() {
		return params[RANDOMIZE_DIVISIONS_EXCLUDE_ZERO_PARAM].value > 0.5f;
	}

	float quantizePitchSemitones(float semitones) {
		float quantizedVolts = closestVoltageInScale(semitones / 12.f, pitchQuantizeRoot, pitchQuantizeScale);
		return quantizedVolts * 12.f;
	}

	void quantizeStoredPitches() {
		for (int i = 0; i < STEPS; i++) {
			stepData[i].pitch = quantizePitchSemitones(stepData[i].pitch);
		}
		loadEditorFromSelectedStep();
	}

	template <typename Fn>
	void forRandomizeTargets(Fn fn) {
		if (randomizeCurrentStepOnly()) {
			fn(selectedStep);
		}
		else {
			for (int i = 0; i < STEPS; i++) {
				fn(i);
			}
		}
	}

	FM16Seq() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(MASTER_LEVEL_PARAM, 0.f, 1.f, 0.7f, "Master level");
		configParam(SEQUENCE_LENGTH_PARAM, 1.f, 16.f, 16.f, "Sequence length");
		paramQuantities[SEQUENCE_LENGTH_PARAM]->snapEnabled = true;
		configParam(EDIT_ACTIVE_PARAM, 0.f, 4.f, 1.f, "Step division");
		paramQuantities[EDIT_ACTIVE_PARAM]->snapEnabled = true;
		configParam(EDIT_PITCH_PARAM, -24.f, 24.f, 0.f, "Pitch", " semitones");
		configParam(EDIT_CAR_RATIO_PARAM, 0.125f, 10.f, 1.f, "Carrier ratio");
		configParam(EDIT_MOD_RATIO_PARAM, 0.125f, 10.f, 2.f, "Mod ratio");
		configParam(EDIT_MOD_FEEDBACK_PARAM, 0.f, 1.f, 0.f, "Mod feedback");
		configParam(EDIT_FM_INDEX_PARAM, 0.f, 7.f, 1.5f, "FM index");
		configParam(EDIT_CAR_ATTACK_PARAM, 0.f, 1.f, 0.03f, "Carrier attack");
		configParam(EDIT_CAR_DECAY_PARAM, 0.f, 1.f, 0.2f, "Carrier decay");
		configParam(EDIT_CAR_SUSTAIN_PARAM, 0.f, 1.f, 0.8f, "Carrier sustain");
		configParam(EDIT_CAR_RELEASE_PARAM, 0.f, 1.f, 0.25f, "Carrier release");
		configParam(EDIT_MOD_ATTACK_PARAM, 0.f, 1.f, 0.02f, "Mod attack");
		configParam(EDIT_MOD_DECAY_PARAM, 0.f, 1.f, 0.18f, "Mod decay");
		configParam(EDIT_MOD_SUSTAIN_PARAM, 0.f, 1.f, 0.6f, "Mod sustain");
		configParam(EDIT_MOD_RELEASE_PARAM, 0.f, 1.f, 0.2f, "Mod release");
		configParam(EDIT_LEVEL_PARAM, 0.f, 1.f, 0.8f, "Step level");
		configParam(EDIT_GATE_LENGTH_PARAM, 1.f, 5000.f, 100.f, "Gate length", " ms");
		for (int i = 0; i < STEPS; i++) {
			configParam(STEP_SELECT_PARAM + i, 0.f, 1.f, 0.f, string::f("Select step %d", i + 1));
		}
		// Keep action param/config ordering aligned with widget layout rows.
		configParam(RANDOMIZE_RATIOS_PARAM, 0.f, 1.f, 0.f, "Randomize ratios");
		configParam(RANDOMIZE_INDEXES_PARAM, 0.f, 1.f, 0.f, "Randomize indexes");
		configParam(RANDOMIZE_FEEDBACK_PARAM, 0.f, 1.f, 0.f, "Randomize feedback");
		configParam(RANDOMIZE_PITCHES_PARAM, 0.f, 1.f, 0.f, "Randomize pitches");
		configParam(RANDOMIZE_ENVELOPES_PARAM, 0.f, 1.f, 0.f, "Randomize envelopes");
		configParam(RANDOMIZE_DIVISIONS_PARAM, 0.f, 1.f, 0.f, "Randomize step divisions");
		configParam(RANDOMIZE_LEVELS_PARAM, 0.f, 1.f, 0.f, "Randomize levels");
		configParam(INITIALIZE_RATIOS_PARAM, 0.f, 1.f, 0.f, "Initialize ratios");
		configParam(INITIALIZE_INDEXES_PARAM, 0.f, 1.f, 0.f, "Initialize indexes");
		configParam(INITIALIZE_FEEDBACK_PARAM, 0.f, 1.f, 0.f, "Initialize feedback");
		configParam(INITIALIZE_PITCHES_PARAM, 0.f, 1.f, 0.f, "Initialize pitches");
		configParam(INITIALIZE_ENVELOPES_PARAM, 0.f, 1.f, 0.f, "Initialize envelopes");
		configParam(INITIALIZE_DIVISIONS_PARAM, 0.f, 1.f, 0.f, "Initialize step divisions");
		configParam(INITIALIZE_LEVELS_PARAM, 0.f, 1.f, 0.f, "Initialize levels");
		configParam(RANDOMIZE_STEP_LENGTHS_PARAM, 0.f, 1.f, 0.f, "Randomize step lengths");
		configParam(INITIALIZE_STEP_LENGTHS_PARAM, 0.f, 1.f, 0.f, "Initialize step lengths");

		configParam(INTEGER_RATIOS_PARAM, 0.f, 1.f, 1.f, "Musical ratios mode");
		paramQuantities[INTEGER_RATIOS_PARAM]->snapEnabled = true;
		configParam<JwPlayModeQuantity>(PLAY_MODE_KNOB_PARAM, 0.f, (float)(NUM_PLAY_MODES - 1), 0.f, "Play mode");
		paramQuantities[PLAY_MODE_KNOB_PARAM]->snapEnabled = true;
		configParam(MANUAL_STEP_TRIGGER_PARAM, 0.f, 1.f, 0.f, "Trigger selected step");
		configParam(RANDOMIZE_AMOUNT_RATIOS_PARAM, 0.f, 1.f, 1.f, "Randomize ratios amount");
		configParam(RANDOMIZE_AMOUNT_INDEXES_PARAM, 0.f, 1.f, 1.f, "Randomize indexes amount");
		configParam(RANDOMIZE_AMOUNT_FEEDBACK_PARAM, 0.f, 1.f, 1.f, "Randomize feedback amount");
		configParam(RANDOMIZE_AMOUNT_PITCHES_PARAM, 0.f, 1.f, 1.f, "Randomize pitches amount");
		configParam(RANDOMIZE_AMOUNT_ENVELOPES_PARAM, 0.f, 1.f, 1.f, "Randomize envelopes amount");
		configParam(RANDOMIZE_AMOUNT_DIVISIONS_PARAM, 0.f, 1.f, 1.f, "Randomize step divisions amount");
		configParam(RANDOMIZE_AMOUNT_LEVELS_PARAM, 0.f, 1.f, 1.f, "Randomize levels amount");
		configParam(RANDOMIZE_AMOUNT_STEP_LENGTHS_PARAM, 0.f, 1.f, 1.f, "Randomize step lengths amount");
		configParam(RANDOMIZE_CURRENT_STEP_ONLY_PARAM, 0.f, 1.f, 0.f, "Current step only");
		paramQuantities[RANDOMIZE_CURRENT_STEP_ONLY_PARAM]->snapEnabled = true;
		configParam(RANDOMIZE_DIVISIONS_EXCLUDE_ZERO_PARAM, 0.f, 1.f, 0.f, "Exclude zero in random divisions");
		paramQuantities[RANDOMIZE_DIVISIONS_EXCLUDE_ZERO_PARAM]->snapEnabled = true;
		configParam(RANDOMIZE_ALL_TRIGGER_PARAM, 0.f, 1.f, 0.f, "Randomize all by amounts");

		configInput(CLOCK_INPUT, "Clock");
		configInput(RESET_INPUT, "Reset");
		configInput(LENGTH_INPUT, "Sequence length CV");
		configInput(PITCH_INPUT, "V/Oct pitch (16 poly channels)");
		configInput(FM_INDEX_INPUT, "FM index CV (16 poly channels)");
		configInput(FEEDBACK_INPUT, "Mod feedback CV (10V full scale, 16 poly channels)");
		configInput(GATE_LENGTH_INPUT, "Gate length CV (1V = 1000 ms, 16 poly channels)");
		configInput(MOD_RATIO_INPUT, "Mod ratio CV (16 poly channels)");
		configInput(CAR_RATIO_INPUT, "Carrier ratio CV (16 poly channels)");
		configInput(MODE_CV_INPUT, "Play mode CV (1V per mode)");
		configInput(MANUAL_STEP_GATE_INPUT, "Manual trigger gate (selected step)");
		configInput(RANDOMIZE_ALL_TRIGGER_INPUT, "Randomize all trigger");

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
			stepData[i].level = 0.8f;
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
		params[EDIT_GATE_LENGTH_PARAM].setValue(s.gateLengthMs);
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
		s.gateLengthMs = params[EDIT_GATE_LENGTH_PARAM].getValue();
	}

	float envKnobToSeconds(float knobValue) {
		// Square mapping gives better precision at short percussive envelope times.
		return 0.001f + (knobValue * knobValue) * 4.0f;
	}

	   int getPlayMode() {
		   float knob = params[PLAY_MODE_KNOB_PARAM].getValue();
		   float cv = (inputs[MODE_CV_INPUT].isConnected() ? inputs[MODE_CV_INPUT].getVoltage() : 0.f);
		   // 1V per mode step
		   float value = knob + cv;
		   return clampijw((int)std::round(value), 0, NUM_PLAY_MODES - 1);
	   }

	void onReset() override {
		sequenceLength = 16;
		goingForward = true;
			   int pm = getPlayMode();
			   stepIndex = (pm == PM_BWD_LOOP || pm == PM_BWD_FWD_LOOP) ? 0 : (sequenceLength - 1);
		latchedStep = 0;
		selectedStep = 0;
		pendingReset = true;
		gateHeld = false;
		carrierPhase = 0.f;
		modPhase = 0.f;
		clockElapsed = 0.f;
		clockInterval = 0.5f;
		clockIntervalValid = false;
		pitchQuantizeEnabled = true;
		pitchQuantizeRoot = QuantizeUtils::NOTE_C;
		pitchQuantizeScale = QuantizeUtils::NONE;
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
		float amount = (
			getRandomizeAmount(RANDOMIZE_AMOUNT_RATIOS_PARAM) +
			getRandomizeAmount(RANDOMIZE_AMOUNT_INDEXES_PARAM) +
			getRandomizeAmount(RANDOMIZE_AMOUNT_FEEDBACK_PARAM) +
			getRandomizeAmount(RANDOMIZE_AMOUNT_PITCHES_PARAM) +
			getRandomizeAmount(RANDOMIZE_AMOUNT_ENVELOPES_PARAM) +
			getRandomizeAmount(RANDOMIZE_AMOUNT_DIVISIONS_PARAM) +
			getRandomizeAmount(RANDOMIZE_AMOUNT_LEVELS_PARAM) +
			getRandomizeAmount(RANDOMIZE_AMOUNT_STEP_LENGTHS_PARAM)
		) / 8.f;
		for (int i = 0; i < STEPS; i++) {
		stepData[i].division = (int)std::floor(randomizeMaxPercent(0.f, 5.f, amount));
		stepData[i].pitch = randomizeMaxPercent(-24.f, 24.f, amount);
		stepData[i].carRatio = randomizeMaxPercent(0.125f, 10.f, amount);
		stepData[i].modRatio = randomizeMaxPercent(0.125f, 10.f, amount);
		stepData[i].modFeedback = randomizeMaxPercent(0.f, 1.f, amount);
		stepData[i].fmIndex = randomizeMaxPercent(0.f, 7.f, amount);
		stepData[i].carAttack = randomizeMaxPercent(0.f, 1.f, amount);
		stepData[i].carDecay = randomizeMaxPercent(0.f, 1.f, amount);
		stepData[i].carSustain = randomizeMaxPercent(0.f, 1.f, amount);
		stepData[i].carRelease = randomizeMaxPercent(0.f, 1.f, amount);
		stepData[i].modAttack = randomizeMaxPercent(0.f, 1.f, amount);
		stepData[i].modDecay = randomizeMaxPercent(0.f, 1.f, amount);
		stepData[i].modSustain = randomizeMaxPercent(0.f, 1.f, amount);
		stepData[i].modRelease = randomizeMaxPercent(0.f, 1.f, amount);
		stepData[i].level = randomizeMaxPercent(0.2f, 1.f, amount);
		stepData[i].gateLengthMs = randomizeMaxPercent(10.f, 5000.f, amount);
		}
		loadEditorFromSelectedStep();
	}

	void randomizeRatiosOnly() {
		float amount = getRandomizeAmount(RANDOMIZE_AMOUNT_RATIOS_PARAM);
		forRandomizeTargets([&](int i) {
			if (integerRatiosMode) {
				static const float allowed[] = {
					0.125f, 0.25f, 0.5f,
					1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f, 9.f, 10.f
				};
				int n = sizeof(allowed) / sizeof(allowed[0]);
				int maxIdx = std::max(1, (int)std::ceil(amount * n)) - 1;
				float targetCar = allowed[rack::random::u32() % (maxIdx + 1)];
				float targetMod = allowed[rack::random::u32() % (maxIdx + 1)];
				stepData[i].carRatio = targetCar;
				stepData[i].modRatio = targetMod;
			} else {
				stepData[i].carRatio = randomizeMaxPercent(0.125f, 10.f, amount);
				stepData[i].modRatio = randomizeMaxPercent(0.125f, 10.f, amount);
			}
		});
		loadEditorFromSelectedStep();
	}

	void initializeRatiosOnly() {
		forRandomizeTargets([&](int i) {
			stepData[i].carRatio = 1.f;
			stepData[i].modRatio = 2.f;
		});
		loadEditorFromSelectedStep();
	}

	void randomizeStepDivisionsOnly() {
		float amount = getRandomizeAmount(RANDOMIZE_AMOUNT_DIVISIONS_PARAM);
		bool excludeZero = randomizeDivisionsExcludeZero();
		forRandomizeTargets([&](int i) {
			int maxDiv = std::max(1, (int)std::ceil(amount * 5.f));
			if (excludeZero) {
				if (maxDiv <= 1) {
					stepData[i].division = 1;
				}
				else {
					stepData[i].division = 1 + (rack::random::u32() % (maxDiv - 1));
				}
			}
			else {
				stepData[i].division = rack::random::u32() % maxDiv;
			}
			stepHits[i] = 0;
		});
		loadEditorFromSelectedStep();
	}

	void initializeStepDivisionsOnly() {
		forRandomizeTargets([&](int i) {
			stepData[i].division = 1;
			stepHits[i] = 0;
		});
		loadEditorFromSelectedStep();
	}

	void randomizeEnvelopesOnly() {
		float amount = getRandomizeAmount(RANDOMIZE_AMOUNT_ENVELOPES_PARAM);
		forRandomizeTargets([&](int i) {
			stepData[i].carAttack = randomizeMaxPercent(0.f, 1.f, amount);
			stepData[i].carDecay = randomizeMaxPercent(0.f, 1.f, amount);
			stepData[i].carSustain = randomizeMaxPercent(0.f, 1.f, amount);
			stepData[i].carRelease = randomizeMaxPercent(0.f, 1.f, amount);
			stepData[i].modAttack = randomizeMaxPercent(0.f, 1.f, amount);
			stepData[i].modDecay = randomizeMaxPercent(0.f, 1.f, amount);
			stepData[i].modSustain = randomizeMaxPercent(0.f, 1.f, amount);
			stepData[i].modRelease = randomizeMaxPercent(0.f, 1.f, amount);
		});
		loadEditorFromSelectedStep();
	}

	void initializeEnvelopesOnly() {
		forRandomizeTargets([&](int i) {
			stepData[i].carAttack = 0.04f;
			stepData[i].carDecay = 0.16f;
			stepData[i].carSustain = 0.05f;
			stepData[i].carRelease = 0.12f;
			stepData[i].modAttack = 0.03f;
			stepData[i].modDecay = 0.12f;
			stepData[i].modSustain = 0.0f;
			stepData[i].modRelease = 0.1f;
		});
		loadEditorFromSelectedStep();
	}

	void randomizePitchesOnly() {
		float amount = getRandomizeAmount(RANDOMIZE_AMOUNT_PITCHES_PARAM);
		forRandomizeTargets([&](int i) {
			stepData[i].pitch = randomizeMaxPercent(-24.f, 24.f, amount);
		});
		loadEditorFromSelectedStep();
	}

	void initializePitchesOnly() {
		forRandomizeTargets([&](int i) {
			stepData[i].pitch = 0.f;
		});
		loadEditorFromSelectedStep();
	}

	void randomizeIndexesOnly() {
		float amount = getRandomizeAmount(RANDOMIZE_AMOUNT_INDEXES_PARAM);
		forRandomizeTargets([&](int i) {
			stepData[i].fmIndex = randomizeMaxPercent(0.f, 7.f, amount);
		});
		loadEditorFromSelectedStep();
	}

	void initializeIndexesOnly() {
		forRandomizeTargets([&](int i) {
			stepData[i].fmIndex = 1.5f;
		});
		loadEditorFromSelectedStep();
	}

	void randomizeLevelsOnly() {
		float amount = getRandomizeAmount(RANDOMIZE_AMOUNT_LEVELS_PARAM);
		forRandomizeTargets([&](int i) {
			stepData[i].level = randomizeMaxPercent(0.2f, 1.f, amount);
		});
		loadEditorFromSelectedStep();
	}

	void initializeLevelsOnly() {
		forRandomizeTargets([&](int i) {
			stepData[i].level = 0.8f;
		});
		loadEditorFromSelectedStep();
	}

	void randomizeStepLengthsOnly() {
		float amount = getRandomizeAmount(RANDOMIZE_AMOUNT_STEP_LENGTHS_PARAM);
		forRandomizeTargets([&](int i) {
			stepData[i].gateLengthMs = randomizeMaxPercent(1.f, 5000.f, amount);
		});
		loadEditorFromSelectedStep();
	}

	void initializeStepLengthsOnly() {
		forRandomizeTargets([&](int i) {
			stepData[i].gateLengthMs = 100.f;
		});
		loadEditorFromSelectedStep();
	}

	void randomizeFeedbackOnly() {
		float amount = getRandomizeAmount(RANDOMIZE_AMOUNT_FEEDBACK_PARAM);
		forRandomizeTargets([&](int i) {
			stepData[i].modFeedback = randomizeMaxPercent(0.f, 1.f, amount);
		});
		loadEditorFromSelectedStep();
	}

	void initializeFeedbackOnly() {
		forRandomizeTargets([&](int i) {
			stepData[i].modFeedback = 0.f;
		});
		loadEditorFromSelectedStep();
	}

	void randomizeAllByAmounts() {
		randomizeRatiosOnly();
		randomizeIndexesOnly();
		randomizeFeedbackOnly();
		randomizePitchesOnly();
		randomizeEnvelopesOnly();
		randomizeStepDivisionsOnly();
		randomizeLevelsOnly();
		randomizeStepLengthsOnly();
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "selectedStep", json_integer(selectedStep));
		json_object_set_new(rootJ, "sequenceLength", json_integer(sequenceLength));
		json_object_set_new(rootJ, "integerRatiosMode", json_boolean(integerRatiosMode));
		json_object_set_new(rootJ, "pitchQuantizeEnabled", json_boolean(pitchQuantizeEnabled));
		json_object_set_new(rootJ, "pitchQuantizeRoot", json_integer(pitchQuantizeRoot));
		json_object_set_new(rootJ, "pitchQuantizeScale", json_integer(pitchQuantizeScale));
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
						json_object_set_new(stepJ, "gateLengthMs", json_real(stepData[i].gateLengthMs));
			json_array_append_new(stepsJ, stepJ);
		}
		json_object_set_new(rootJ, "steps", stepsJ);
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* selectedStepJ = json_object_get(rootJ, "selectedStep");
		if (selectedStepJ) {
			   selectedStep = clampijw((int)json_integer_value(selectedStepJ), 0, sequenceLength - 1);
		}
		json_t* sequenceLengthJ = json_object_get(rootJ, "sequenceLength");
		if (sequenceLengthJ) {
			sequenceLength = clampijw((int)json_integer_value(sequenceLengthJ), 1, STEPS);
		}
		json_t* integerRatiosModeJ = json_object_get(rootJ, "integerRatiosMode");
		if (integerRatiosModeJ) {
			integerRatiosMode = json_is_true(integerRatiosModeJ);
		}
		json_t* pitchQuantizeEnabledJ = json_object_get(rootJ, "pitchQuantizeEnabled");
		if (pitchQuantizeEnabledJ) {
			pitchQuantizeEnabled = json_is_true(pitchQuantizeEnabledJ);
		}
		json_t* pitchQuantizeRootJ = json_object_get(rootJ, "pitchQuantizeRoot");
		if (pitchQuantizeRootJ) {
			pitchQuantizeRoot = clampijw((int)json_integer_value(pitchQuantizeRootJ), 0, QuantizeUtils::NUM_NOTES - 1);
		}
		json_t* pitchQuantizeScaleJ = json_object_get(rootJ, "pitchQuantizeScale");
		if (pitchQuantizeScaleJ) {
			pitchQuantizeScale = clampijw((int)json_integer_value(pitchQuantizeScaleJ), 0, QuantizeUtils::NUM_SCALES - 1);
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
				v = json_object_get(stepJ, "carRatio"); if (v) stepData[i].carRatio = clampfjw((float)json_number_value(v), 0.125f, 10.f);
				v = json_object_get(stepJ, "modRatio"); if (v) stepData[i].modRatio = clampfjw((float)json_number_value(v), 0.125f, 10.f);
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
							v = json_object_get(stepJ, "gateLengthMs"); if (v) stepData[i].gateLengthMs = (float)json_number_value(v);
			}
		}
		loadEditorFromSelectedStep();
	}

	void process(const ProcessArgs& args) override {
		bool stepChanged = false;

		// Check for step selection changes.
		// Editing should always allow selecting any of the 16 stored steps,
		// regardless of the current playback loop length.
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

		bool manualTriggerPressed = manualStepTrigger.process(params[MANUAL_STEP_TRIGGER_PARAM].getValue());
		bool manualGateTriggered = manualStepGateTrigger.process(inputs[MANUAL_STEP_GATE_INPUT].getVoltage());
		bool manualTriggerEvent = manualTriggerPressed || manualGateTriggered;
		if (manualTriggerEvent) {
			for (int i = 0; i < sequenceLength; i++) {
				if (engines[i].gateHeld) {
					engines[i].carrierEnv.gateOff();
					engines[i].modEnv.gateOff();
					engines[i].gateHeld = false;
				}
			}
			latchedStep = selectedStep;
		}

		// Update sequence length from knob + CV (1V per step)
		float lengthControl = params[SEQUENCE_LENGTH_PARAM].getValue() + inputs[LENGTH_INPUT].getVoltage();
		sequenceLength = (int) std::round(lengthControl);
		sequenceLength = clampijw(sequenceLength, 1, STEPS);

		// Update integer ratios mode from switch
		integerRatiosMode = params[INTEGER_RATIOS_PARAM].getValue() > 0.5f;

		if (actionTrigger[0].process(params[RANDOMIZE_RATIOS_PARAM].getValue())) randomizeRatiosOnly();
		if (actionTrigger[1].process(params[RANDOMIZE_INDEXES_PARAM].getValue())) randomizeIndexesOnly();
		if (actionTrigger[2].process(params[RANDOMIZE_FEEDBACK_PARAM].getValue())) randomizeFeedbackOnly();
		if (actionTrigger[3].process(params[RANDOMIZE_PITCHES_PARAM].getValue())) randomizePitchesOnly();
		if (actionTrigger[4].process(params[RANDOMIZE_ENVELOPES_PARAM].getValue())) randomizeEnvelopesOnly();
		if (actionTrigger[5].process(params[RANDOMIZE_DIVISIONS_PARAM].getValue())) randomizeStepDivisionsOnly();
		if (actionTrigger[6].process(params[RANDOMIZE_LEVELS_PARAM].getValue())) randomizeLevelsOnly();
		if (actionTrigger[7].process(params[INITIALIZE_RATIOS_PARAM].getValue())) initializeRatiosOnly();
		if (actionTrigger[8].process(params[INITIALIZE_INDEXES_PARAM].getValue())) initializeIndexesOnly();
		if (actionTrigger[9].process(params[INITIALIZE_FEEDBACK_PARAM].getValue())) initializeFeedbackOnly();
		if (actionTrigger[10].process(params[INITIALIZE_PITCHES_PARAM].getValue())) initializePitchesOnly();
		if (actionTrigger[11].process(params[INITIALIZE_ENVELOPES_PARAM].getValue())) initializeEnvelopesOnly();
		if (actionTrigger[12].process(params[INITIALIZE_DIVISIONS_PARAM].getValue())) initializeStepDivisionsOnly();
		if (actionTrigger[13].process(params[INITIALIZE_LEVELS_PARAM].getValue())) initializeLevelsOnly();
		if (actionTrigger[14].process(params[RANDOMIZE_STEP_LENGTHS_PARAM].getValue())) randomizeStepLengthsOnly();
		if (actionTrigger[15].process(params[INITIALIZE_STEP_LENGTHS_PARAM].getValue())) initializeStepLengthsOnly();

		bool randomizeAllPressed = randomizeAllButtonTrigger.process(params[RANDOMIZE_ALL_TRIGGER_PARAM].getValue());
		bool randomizeAllGateTriggered = randomizeAllInputTrigger.process(inputs[RANDOMIZE_ALL_TRIGGER_INPUT].getVoltage());
		if (randomizeAllPressed || randomizeAllGateTriggered) {
			randomizeAllByAmounts();
		}

		// Clock and sequence logic
		float clockValue = inputs[CLOCK_INPUT].getVoltage();
		float resetValue = inputs[RESET_INPUT].getVoltage();

		if (resetTrigger.process(resetValue)) {
			pendingReset = true;
		}

		bool stepTriggered = false;
		clockElapsed += args.sampleTime;
		if (clockTrigger.process(clockValue)) {
			if (clockIntervalValid) {
				clockInterval = std::max(clockElapsed, 0.001f);
			} else {
				clockIntervalValid = true;
			}
			clockElapsed = 0.f;

			if (pendingReset) {
				int pm = getPlayMode();
				if (pm == PM_BWD_LOOP || pm == PM_BWD_FWD_LOOP) {
					stepIndex = 0;
					goingForward = false;
				} else {
					stepIndex = sequenceLength - 1;
					goingForward = true;
				}
				pendingReset = false;
			}
			// Advance step index according to play mode
			{
				int pm = getPlayMode();
				if (pm == PM_FWD_LOOP) {
					stepIndex = (stepIndex + 1) % sequenceLength;
				} else if (pm == PM_BWD_LOOP) {
					stepIndex = stepIndex > 0 ? stepIndex - 1 : sequenceLength - 1;
				} else if (pm == PM_FWD_BWD_LOOP || pm == PM_BWD_FWD_LOOP) {
					if (goingForward) {
						if (stepIndex < sequenceLength - 1) {
							stepIndex++;
						} else {
							goingForward = false;
							stepIndex--;
						}
					} else {
						if (stepIndex > 0) {
							stepIndex--;
						} else {
							goingForward = true;
							stepIndex++;
						}
					}
					if (pm == PM_BWD_FWD_LOOP && !goingForward) {
						// handled above via goingForward state
					}
				} else if (pm == PM_RANDOM_POS) {
					stepIndex = (int)(random::uniform() * sequenceLength) % sequenceLength;
				}
			}

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

		// Poly channels are interpreted as per-step CV lanes.
		// Only one sequencer step plays at a time, so read CV from the
		// currently latched step channel and run a single active voice.
		int cvChannel = clampijw(latchedStep, 0, STEPS - 1);
		auto getStepCvVoltage = [&](int inputId) {
			int channels = inputs[inputId].getChannels();
			if (channels <= 1) {
				return inputs[inputId].getVoltage(0);
			}
			return inputs[inputId].getVoltage(cvChannel);
		};

		if (manualTriggerEvent) {
			stepTriggered = true;
		}

		// When step triggers, gate on all engines and reset gate timers
		if (stepTriggered) {
			FMEngine& engine = engines[0];
			engine.carrierEnv.gateOn();
			engine.modEnv.gateOn();
			engine.gateHeld = true;
			engine.stepElapsed = 0.f;
		}

		// Gate length logic: close gate after gateLengthMs for each channel
		{
			FMEngine& engine = engines[0];
			if (engine.gateHeld) {
				engine.stepElapsed += args.sampleTime * 1000.f; // ms
				float gateLengthCV = getStepCvVoltage(GATE_LENGTH_INPUT);
				float gateLen = clampfjw(stepData[latchedStep].gateLengthMs + gateLengthCV * 1000.f, 1.f, 5000.f);
				if (engine.stepElapsed >= gateLen) {
					engine.carrierEnv.gateOff();
					engine.modEnv.gateOff();
					engine.gateHeld = false;
				}
			}
		}

		// Process single active voice using step-indexed CV channel.
		float mixedOut = 0.f;
		{
			FMEngine& engine = engines[0];
			const StepData& s = stepData[latchedStep];

			float pitchVolts = getStepCvVoltage(PITCH_INPUT);
			float fmIndexCV = getStepCvVoltage(FM_INDEX_INPUT);
			float feedbackCV = getStepCvVoltage(FEEDBACK_INPUT);
			float modRatioCV = getStepCvVoltage(MOD_RATIO_INPUT);
			float carRatioCV = getStepCvVoltage(CAR_RATIO_INPUT);

			// Combine CV with stored step parameters
			float quantizedStepPitch = pitchQuantizeEnabled ? quantizePitchSemitones(s.pitch) : s.pitch;
			float baseFreqCV = pitchVolts + (quantizedStepPitch / 12.f);
			engine.baseFreq = 261.6256f * std::pow(2.f, baseFreqCV);

			float carRatio = s.carRatio + carRatioCV;
			float modRatio = s.modRatio + modRatioCV;
			carRatio = clampfjw(carRatio, 0.125f, 10.f);
			modRatio = clampfjw(modRatio, 0.125f, 10.f);
			float modFeedback = clampfjw(s.modFeedback + feedbackCV * 0.1f, 0.f, 1.f);

			float fmIndex = s.fmIndex + fmIndexCV;
			fmIndex = clampfjw(fmIndex, 0.f, 7.f);

			// Short parameter slew suppresses hard step-to-step discontinuities
			// without low-pass filtering the final audio output.
			const float paramSlewSec = 0.001f;
			float paramAlpha = clampfjw(args.sampleTime / paramSlewSec, 0.f, 1.f);
			engine.smoothedCarRatio += (carRatio - engine.smoothedCarRatio) * paramAlpha;
			engine.smoothedModRatio += (modRatio - engine.smoothedModRatio) * paramAlpha;
			engine.smoothedFmIndex += (fmIndex - engine.smoothedFmIndex) * paramAlpha;
			engine.smoothedModFeedback += (modFeedback - engine.smoothedModFeedback) * paramAlpha;
			engine.smoothedLevel += (s.level - engine.smoothedLevel) * paramAlpha;

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

			engine.modPhase += TWO_PI * (engine.baseFreq * engine.smoothedModRatio) / args.sampleRate;
			if (engine.modPhase > TWO_PI)
				engine.modPhase -= TWO_PI;
			float feedbackOffset = 0.f;
			if (engine.smoothedModFeedback > 0.001f) {
				// Shape and scale feedback to keep high settings musical instead of noisy.
				float feedbackAmount = engine.smoothedModFeedback * engine.smoothedModFeedback;
				float feedbackSample = std::tanh(engine.modFeedbackDelayedSample * 1.5f);
				feedbackOffset = feedbackAmount * feedbackSample * 2.0f;
			}
			float modSignal = std::sin(engine.modPhase + feedbackOffset) * modEnvValue;
			// Light damping on the feedback state to reduce high-frequency hash.
			engine.modFeedbackDelayedSample += (modSignal - engine.modFeedbackDelayedSample) * 0.2f;

			engine.carrierPhase += TWO_PI * (engine.baseFreq * engine.smoothedCarRatio) / args.sampleRate;
			if (engine.carrierPhase > TWO_PI)
				engine.carrierPhase -= TWO_PI;

			float carrierSignal = std::sin(engine.carrierPhase + (modSignal * engine.smoothedFmIndex * TWO_PI));
			float stepLevel = engine.smoothedLevel;
			float out = carrierSignal * carEnvValue * stepLevel * 5.0f;
			mixedOut = out;
		}

		float masterOut = mixedOut * params[MASTER_LEVEL_PARAM].getValue();
		float out = clampfjw(masterOut, -5.f, 5.f);
		outputs[AUDIO_OUTPUT].setVoltage(out);
		outputs[AUDIO_OUTPUT].setChannels(1);
	}
};

struct FM16SeqWidget : ModuleWidget {

	// Static clipboard for step copy/paste
	static FM16Seq::StepData stepClipboard;

	struct QuantizeEnabledMenuItem : MenuItem {
		FM16Seq* module;
		void onAction(const event::Action& e) override {
			if (!module) return;
			module->pitchQuantizeEnabled = !module->pitchQuantizeEnabled;
		}
		void step() override {
			rightText = CHECKMARK(module && module->pitchQuantizeEnabled);
			MenuItem::step();
		}
	};

	struct RootNoteValueItem : MenuItem {
		FM16Seq* module;
		int note;
		void onAction(const event::Action& e) override {
			if (!module) return;
			module->pitchQuantizeRoot = note;
		}
		void step() override {
			rightText = CHECKMARK(module && module->pitchQuantizeRoot == note);
			MenuItem::step();
		}
	};

	struct RootNoteItem : MenuItem {
		FM16Seq* module;
		Menu* createChildMenu() override {
			Menu* menu = new Menu;
			if (!module) {
				return menu;
			}
			for (int note = 0; note < QuantizeUtils::NUM_NOTES; note++) {
				RootNoteValueItem* item = new RootNoteValueItem;
				item->text = module->noteName(note);
				item->module = module;
				item->note = note;
				menu->addChild(item);
			}
			return menu;
		}
	};

	struct ScaleValueItem : MenuItem {
		FM16Seq* module;
		int scale;
		void onAction(const event::Action& e) override {
			if (!module) return;
			module->pitchQuantizeScale = scale;
		}
		void step() override {
			rightText = CHECKMARK(module && module->pitchQuantizeScale == scale);
			MenuItem::step();
		}
	};

	struct ScaleItem : MenuItem {
		FM16Seq* module;
		Menu* createChildMenu() override {
			Menu* menu = new Menu;
			if (!module) {
				return menu;
			}
			for (int scale = 0; scale < QuantizeUtils::NUM_SCALES; scale++) {
				ScaleValueItem* item = new ScaleValueItem;
				item->text = module->scaleName(scale);
				item->module = module;
				item->scale = scale;
				menu->addChild(item);
			}
			return menu;
		}
	};

   struct CopyStepMenuItem : MenuItem {
	   FM16Seq* module;
	   void onAction(const event::Action& e) override {
		   if (!module) return;
		   int src = module->selectedStep;
		   stepClipboard = module->stepData[src];
	   }
   };

   struct PasteStepMenuItem : MenuItem {
	   FM16Seq* module;
	   void onAction(const event::Action& e) override {
		   if (!module) return;
		   int dst = module->selectedStep;
		   module->stepData[dst] = stepClipboard;
		   module->loadEditorFromSelectedStep();
	   }
   };

   struct CopyStepToAllMenuItem : MenuItem {
	   FM16Seq* module;
	   void onAction(const event::Action& e) override {
		   if (!module) return;
		   int src = module->selectedStep;
		   FM16Seq::StepData srcData = module->stepData[src];
		   for (int i = 0; i < FM16Seq::STEPS; i++) {
			   if (i != src) module->stepData[i] = srcData;
		   }
		   module->loadEditorFromSelectedStep();
	   }
   };


   void appendContextMenu(Menu* menu) override {
	   ModuleWidget::appendContextMenu(menu);
	   FM16Seq* m = dynamic_cast<FM16Seq*>(module);
	   if (m) {
		   menu->addChild(new MenuSeparator());

		   CopyStepMenuItem* copyItem = new CopyStepMenuItem();
		   copyItem->text = "Copy step";
		   copyItem->module = m;
		   menu->addChild(copyItem);

		   PasteStepMenuItem* pasteItem = new PasteStepMenuItem();
		   pasteItem->text = "Paste step";
		   pasteItem->module = m;
		   menu->addChild(pasteItem);

		   CopyStepToAllMenuItem* item = new CopyStepToAllMenuItem();
		   item->text = "Copy current step to all steps";
		   item->module = m;
		   menu->addChild(item);

		   menu->addChild(new MenuSeparator());

		   QuantizeEnabledMenuItem* quantizeEnabledItem = new QuantizeEnabledMenuItem();
		   quantizeEnabledItem->text = "Enable pitch quantization";
		   quantizeEnabledItem->module = m;
		   menu->addChild(quantizeEnabledItem);

		   RootNoteItem* rootItem = new RootNoteItem();
		   rootItem->text = "Pitch quantize root note";
		   rootItem->rightText = m->noteName(m->pitchQuantizeRoot) + " " + RIGHT_ARROW;
		   rootItem->module = m;
		   menu->addChild(rootItem);

		   ScaleItem* scaleItem = new ScaleItem();
		   scaleItem->text = "Pitch quantize scale";
		   scaleItem->rightText = m->scaleName(m->pitchQuantizeScale) + " " + RIGHT_ARROW;
		   scaleItem->module = m;
		   menu->addChild(scaleItem);
	   }
   }

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
			float x = 30.f + (float)i * 32.f;
			float y = 73.f;
			addParam(createParamCentered<TinyButton>(Vec(x, y), module, FM16Seq::STEP_SELECT_PARAM + i));
			addChild(createLight<SmallLight<MyBlueValueLight>>(Vec(x - 6.f, y - 16.f), module, FM16Seq::STEP_PLAY_LIGHT + i));
			addChild(createLight<SmallLight<MyOrangeValueLight>>(Vec(x + 1.f, y - 16.f), module, FM16Seq::STEP_EDIT_LIGHT + i));
		}

		addParam(createParamCentered<JwSmallSnapKnob>(Vec(72.f, 140.f), module, FM16Seq::EDIT_ACTIVE_PARAM));
		addParam(createParamCentered<SmallWhiteKnob>(Vec(134.f, 140.f), module, FM16Seq::EDIT_PITCH_PARAM));
		addParam(createParamCentered<SmallWhiteKnob>(Vec(196.f, 140.f), module, FM16Seq::EDIT_FM_INDEX_PARAM));
		addParam(createParamCentered<SmallWhiteKnob>(Vec(258.f, 140.f), module, FM16Seq::EDIT_LEVEL_PARAM));

		addParam(createParamCentered<SmallWhiteKnob>(Vec(72.f, 194.f), module, FM16Seq::EDIT_CAR_RATIO_PARAM));
		addParam(createParamCentered<SmallWhiteKnob>(Vec(134.f, 194.f), module, FM16Seq::EDIT_MOD_RATIO_PARAM));
		addParam(createParamCentered<SmallWhiteKnob>(Vec(196.f, 194.f), module, FM16Seq::EDIT_MOD_FEEDBACK_PARAM));
		addParam(createParamCentered<SmallWhiteKnob>(Vec(258.f, 194.f), module, FM16Seq::EDIT_GATE_LENGTH_PARAM));

		addParam(createParamCentered<JwTinyGrayKnob>(Vec(84.f, 248.f), module, FM16Seq::EDIT_CAR_ATTACK_PARAM));
		addParam(createParamCentered<JwTinyGrayKnob>(Vec(124.f, 248.f), module, FM16Seq::EDIT_CAR_DECAY_PARAM));
		addParam(createParamCentered<JwTinyGrayKnob>(Vec(164.f, 248.f), module, FM16Seq::EDIT_CAR_SUSTAIN_PARAM));
		addParam(createParamCentered<JwTinyGrayKnob>(Vec(204.f, 248.f), module, FM16Seq::EDIT_CAR_RELEASE_PARAM));

		addParam(createParamCentered<JwTinyGrayKnob>(Vec(84.f, 288.f), module, FM16Seq::EDIT_MOD_ATTACK_PARAM));
		addParam(createParamCentered<JwTinyGrayKnob>(Vec(124.f, 288.f), module, FM16Seq::EDIT_MOD_DECAY_PARAM));
		addParam(createParamCentered<JwTinyGrayKnob>(Vec(164.f, 288.f), module, FM16Seq::EDIT_MOD_SUSTAIN_PARAM));
		addParam(createParamCentered<JwTinyGrayKnob>(Vec(204.f, 288.f), module, FM16Seq::EDIT_MOD_RELEASE_PARAM));
		addParam(createParamCentered<SmallButton>(Vec(252.f, 253.f), module, FM16Seq::MANUAL_STEP_TRIGGER_PARAM));
		addInput(createInputCentered<PJ301MPort>(Vec(252.f, 283.f), module, FM16Seq::MANUAL_STEP_GATE_INPUT));

		addParam(createParamCentered<JwPlayModeKnob>(Vec(342.f, 344.f), module, FM16Seq::PLAY_MODE_KNOB_PARAM));
		addInput(createInputCentered<PJ301MPort>(Vec(372.f, 344.f), module, FM16Seq::MODE_CV_INPUT));
		addParam(createParamCentered<JwSmallSnapKnob>(Vec(412.f, 344.f), module, FM16Seq::SEQUENCE_LENGTH_PARAM));
		addInput(createInputCentered<PJ301MPort>(Vec(442.f, 344.f), module, FM16Seq::LENGTH_INPUT));

		addInput(createInputCentered<PJ301MPort>(Vec(32.f, 344.f), module, FM16Seq::CLOCK_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(70.2857f, 344.f), module, FM16Seq::RESET_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(108.5714f, 344.f), module, FM16Seq::PITCH_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(146.8571f, 344.f), module, FM16Seq::FM_INDEX_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(185.1429f, 344.f), module, FM16Seq::MOD_RATIO_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(223.4286f, 344.f), module, FM16Seq::CAR_RATIO_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(261.7143f, 344.f), module, FM16Seq::FEEDBACK_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(300.f, 344.f), module, FM16Seq::GATE_LENGTH_INPUT));
        
        addParam(createParamCentered<SmallWhiteKnob>(Vec(485.f, 344.f), module, FM16Seq::MASTER_LEVEL_PARAM));
		addOutput(createOutputCentered<PJ301MPort>(Vec(520.f, 344.f), module, FM16Seq::AUDIO_OUTPUT));

		const float initX = 325.f;
		const float rndX = 360.f;
		const float y0 = 115.f;
		const float yStep = 24.f;

		addParam(createParamCentered<TinyButton>(Vec(rndX, y0 + yStep * 0.f), module, FM16Seq::RANDOMIZE_RATIOS_PARAM));
		addParam(createParamCentered<TinyButton>(Vec(rndX, y0 + yStep * 1.f), module, FM16Seq::RANDOMIZE_INDEXES_PARAM));
		addParam(createParamCentered<TinyButton>(Vec(rndX, y0 + yStep * 2.f), module, FM16Seq::RANDOMIZE_FEEDBACK_PARAM));
		addParam(createParamCentered<TinyButton>(Vec(rndX, y0 + yStep * 3.f), module, FM16Seq::RANDOMIZE_PITCHES_PARAM));
		addParam(createParamCentered<TinyButton>(Vec(rndX, y0 + yStep * 4.f), module, FM16Seq::RANDOMIZE_ENVELOPES_PARAM));
		addParam(createParamCentered<TinyButton>(Vec(rndX, y0 + yStep * 5.f), module, FM16Seq::RANDOMIZE_DIVISIONS_PARAM));
		addParam(createParamCentered<TinyButton>(Vec(rndX, y0 + yStep * 6.f), module, FM16Seq::RANDOMIZE_LEVELS_PARAM));
		addParam(createParamCentered<TinyButton>(Vec(rndX, y0 + yStep * 7.f), module, FM16Seq::RANDOMIZE_STEP_LENGTHS_PARAM));

		const float amtX = 440.f;
		addParam(createParamCentered<JwHorizontalVCVSlider>(Vec(amtX, y0 + yStep * 0.f), module, FM16Seq::RANDOMIZE_AMOUNT_RATIOS_PARAM));
		addParam(createParamCentered<JwHorizontalVCVSlider>(Vec(amtX, y0 + yStep * 1.f), module, FM16Seq::RANDOMIZE_AMOUNT_INDEXES_PARAM));
		addParam(createParamCentered<JwHorizontalVCVSlider>(Vec(amtX, y0 + yStep * 2.f), module, FM16Seq::RANDOMIZE_AMOUNT_FEEDBACK_PARAM));
		addParam(createParamCentered<JwHorizontalVCVSlider>(Vec(amtX, y0 + yStep * 3.f), module, FM16Seq::RANDOMIZE_AMOUNT_PITCHES_PARAM));
		addParam(createParamCentered<JwHorizontalVCVSlider>(Vec(amtX, y0 + yStep * 4.f), module, FM16Seq::RANDOMIZE_AMOUNT_ENVELOPES_PARAM));
		addParam(createParamCentered<JwHorizontalVCVSlider>(Vec(amtX, y0 + yStep * 5.f), module, FM16Seq::RANDOMIZE_AMOUNT_DIVISIONS_PARAM));
		addParam(createParamCentered<JwHorizontalVCVSlider>(Vec(amtX, y0 + yStep * 6.f), module, FM16Seq::RANDOMIZE_AMOUNT_LEVELS_PARAM));
		addParam(createParamCentered<JwHorizontalVCVSlider>(Vec(amtX, y0 + yStep * 7.f), module, FM16Seq::RANDOMIZE_AMOUNT_STEP_LENGTHS_PARAM));

		addParam(createParamCentered<TinyButton>(Vec(initX, y0 + yStep * 0.f), module, FM16Seq::INITIALIZE_RATIOS_PARAM));
		addParam(createParamCentered<TinyButton>(Vec(initX, y0 + yStep * 1.f), module, FM16Seq::INITIALIZE_INDEXES_PARAM));
		addParam(createParamCentered<TinyButton>(Vec(initX, y0 + yStep * 2.f), module, FM16Seq::INITIALIZE_FEEDBACK_PARAM));
		addParam(createParamCentered<TinyButton>(Vec(initX, y0 + yStep * 3.f), module, FM16Seq::INITIALIZE_PITCHES_PARAM));
		addParam(createParamCentered<TinyButton>(Vec(initX, y0 + yStep * 4.f), module, FM16Seq::INITIALIZE_ENVELOPES_PARAM));
		addParam(createParamCentered<TinyButton>(Vec(initX, y0 + yStep * 5.f), module, FM16Seq::INITIALIZE_DIVISIONS_PARAM));
		addParam(createParamCentered<TinyButton>(Vec(initX, y0 + yStep * 6.f), module, FM16Seq::INITIALIZE_LEVELS_PARAM));
		addParam(createParamCentered<TinyButton>(Vec(initX, y0 + yStep * 7.f), module, FM16Seq::INITIALIZE_STEP_LENGTHS_PARAM));

		// Integer ratios mode switch next to randomize ratios button
		addParam(createParamCentered<JwVerticalSwitch>(Vec(510, 115), module, FM16Seq::INTEGER_RATIOS_PARAM));
		addParam(createParamCentered<JwVerticalSwitch>(Vec(295, 204), module, FM16Seq::RANDOMIZE_CURRENT_STEP_ONLY_PARAM));
		addParam(createParamCentered<JwVerticalSwitch>(Vec(510, 235), module, FM16Seq::RANDOMIZE_DIVISIONS_EXCLUDE_ZERO_PARAM));
		addParam(createParamCentered<SmallButton>(Vec(295, 253), module, FM16Seq::RANDOMIZE_ALL_TRIGGER_PARAM));
		addInput(createInputCentered<PJ301MPort>(Vec(295, 283), module, FM16Seq::RANDOMIZE_ALL_TRIGGER_INPUT));
	}
};


// Define and initialize the static clipboard variable
FM16Seq::StepData FM16SeqWidget::stepClipboard = FM16Seq::StepData{};

Model* modelFM16Seq = createModel<FM16Seq, FM16SeqWidget>("FM16Seq");
