#include <string.h>
#include <algorithm>
#include <math.h>
#include "JWModules.hpp"

struct RandomSound : Module {
	enum ParamIds {
		AMP_DECAY_PARAM,
		TRIGGER_BUTTON_PARAM,
		RANDOMIZE_BUTTON_PARAM,
		TRIG_RANDOMIZE_BUTTON_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		TRIGGER_INPUT,
		TRIGGER_RANDOMIZE,
		TRIG_RANDOMIZE_INPUT,
		DECAY_CV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		VOLT_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};
	dsp::SchmittTrigger inTrig;
	dsp::SchmittTrigger rndTrig;
	dsp::SchmittTrigger btnTrig;
	dsp::SchmittTrigger btnRndTrig;
	dsp::SchmittTrigger btnTrigRndTrig;
	dsp::SchmittTrigger trigRndInputTrig;
	bool edgesPrimed = false;
	// Polyphony support
	int channels = 1;
	// Per-channel synthesis state
	enum Wave { W_SINE, W_SQUARE, W_SAW, W_TRI };
	enum EnvStage { ENV_IDLE, ENV_ATTACK, ENV_DECAY };
	struct Voice {
		// Carrier
		float freq = 220.f;
		float phase = 0.f;
		float amp = 5.0f;
		float feedback = 0.2f;
		float prevOut = 0.f;
		Wave carrierWave = W_SINE;
		// Modulator
		float modFreq = 110.f;
		float modPhase = 0.f;
		float modAmp = 5.0f;
		float modFeedback = 0.1f;
		float modPrevOut = 0.f;
		float fmIndex = 0.1f;
		Wave modWave = W_SINE;
		// Routing
		bool useFM = true; // true: FM, false: additive mix
		// Envelopes
		EnvStage envStage = ENV_IDLE;
		float attackTime = 0.f;
		float decayTime = 0.5f;
		float envValue = 0.f;
		bool ampExp = false; // exponential amplitude envelope
		EnvStage pitchEnvStage = ENV_IDLE;
		float pitchEnvValue = 0.f;
		float pitchDecayTime = 0.2f;
		bool pitchExp = false; // exponential pitch envelope
		bool pitchUp = true;
		float pitchOctavesMax = 4.0f;
		EnvStage modEnvStage = ENV_IDLE;
		float modEnvValue = 0.f;
		float modDecayTime = 0.3f;
		bool modAmpExp = false; // exponential mod amplitude envelope
		EnvStage modPitchEnvStage = ENV_IDLE;
		float modPitchEnvValue = 0.f;
		float modPitchDecayTime = 0.25f;
		bool modPitchUp = true;
		float modPitchOctavesMax = 4.0f;
		bool modPitchExp = false; // exponential mod pitch envelope
	};
	Voice voices[16];

	RandomSound() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(AMP_DECAY_PARAM, 0.001f, 1.0f, 0.3f, "Amplitude Decay", "s");
		configButton(TRIGGER_BUTTON_PARAM, "Trigger Sound");
		configButton(RANDOMIZE_BUTTON_PARAM, "Randomize Sound");
		configButton(TRIG_RANDOMIZE_BUTTON_PARAM, "Trigger + Randomize");
		configInput(TRIGGER_INPUT, "Trigger Sound");
		configInput(TRIGGER_RANDOMIZE, "Trigger Randomize");
		configInput(TRIG_RANDOMIZE_INPUT, "Trigger + Randomize");
		configInput(DECAY_CV_INPUT, "Decay CV");
		configOutput(VOLT_OUTPUT, "Random Sound (polyphonic)");
		// Initialize voices' decay to current knob value so first randomization respects it
		float initDecay = clamp(params[AMP_DECAY_PARAM].getValue(), 0.001f, 1.0f);
		for (int i = 0; i < 16; i++) {
			voices[i].decayTime = initDecay;
			voices[i].feedback = random::uniform() * 0.9f; // random initial carrier feedback
		}
	}

	~RandomSound() {
	}

	void onRandomize() override {
		// Randomize all voices independently
		float baseDecay = clamp(params[AMP_DECAY_PARAM].getValue(), 0.001f, 1.0f);
		for (int i = 0; i < channels && i < 16; i++) {
			Voice &v = voices[i];
			// Carrier frequency: 20 Hz .. 2 kHz
			v.freq = 20.f + random::uniform() * (2000.f - 20.f);
			// Random per-voice carrier feedback scaled by global knob
			v.feedback = random::uniform() * 0.9f;
			v.attackTime = 0.f;
			// Set amplitude decay from knob so it is honored immediately after randomize
			v.decayTime  = baseDecay;
			v.pitchDecayTime = 0.001f + random::uniform() * (1.0f - 0.001f);
			v.pitchUp = random::uniform() < 0.5f;
			v.pitchOctavesMax = random::uniform() * 4.0f;
			int cw = int(random::uniform() * 4.0f); if (cw < 0) cw = 0; if (cw > 3) cw = 3;
			v.carrierWave = (Wave)cw;
			// Modulator frequency: 20 Hz .. 2 kHz
			v.modFreq = 20.f + random::uniform() * (2000.f - 20.f);
			v.modFeedback = random::uniform() * 0.9f;
			v.modDecayTime = 0.001f + random::uniform() * (1.0f - 0.001f);
			v.modPitchDecayTime = 0.001f + random::uniform() * (1.0f - 0.001f);
			v.modPitchUp = random::uniform() < 0.5f;
			v.modPitchOctavesMax = random::uniform() * 4.0f;
			v.fmIndex = 0.05f + random::uniform() * 0.45f;
			int mw = int(random::uniform() * 4.0f); if (mw < 0) mw = 0; if (mw > 3) mw = 3;
			v.modWave = (Wave)mw;
			v.useFM = random::uniform() < 0.5f;
			// Randomly choose exponential or linear envelopes
			v.ampExp = random::uniform() < 0.5f;
			v.pitchExp = random::uniform() < 0.5f;
			v.modAmpExp = random::uniform() < 0.5f;
			v.modPitchExp = random::uniform() < 0.5f;
		}
	}

	void onReset() override {
	}

	void onSampleRateChange() override {
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "channels", json_integer(channels));
		// Persist global parameters explicitly as well
		json_object_set_new(rootJ, "ampDecay", json_real(params[AMP_DECAY_PARAM].getValue()));
		// Save voices
		json_t *voicesJ = json_array();
		for (int i = 0; i < 16; i++) {
			json_t *vj = json_object();
			Voice &v = voices[i];
			json_object_set_new(vj, "freq", json_real(v.freq));
			json_object_set_new(vj, "phase", json_real(v.phase));
			json_object_set_new(vj, "amp", json_real(v.amp));
			json_object_set_new(vj, "feedback", json_real(v.feedback));
			json_object_set_new(vj, "prevOut", json_real(v.prevOut));
			json_object_set_new(vj, "carrierWave", json_integer((int)v.carrierWave));
			json_object_set_new(vj, "attackTime", json_real(v.attackTime));
			json_object_set_new(vj, "decayTime", json_real(v.decayTime));
			json_object_set_new(vj, "envValue", json_real(v.envValue));
			json_object_set_new(vj, "envStage", json_integer((int)v.envStage));
			json_object_set_new(vj, "pitchEnvValue", json_real(v.pitchEnvValue));
			json_object_set_new(vj, "pitchDecayTime", json_real(v.pitchDecayTime));
			json_object_set_new(vj, "pitchUp", json_integer(v.pitchUp ? 1 : 0));
			json_object_set_new(vj, "pitchEnvStage", json_integer((int)v.pitchEnvStage));
			json_object_set_new(vj, "modFreq", json_real(v.modFreq));
			json_object_set_new(vj, "modPhase", json_real(v.modPhase));
			json_object_set_new(vj, "modAmp", json_real(v.modAmp));
			json_object_set_new(vj, "modFeedback", json_real(v.modFeedback));
			json_object_set_new(vj, "modPrevOut", json_real(v.modPrevOut));
			json_object_set_new(vj, "modWave", json_integer((int)v.modWave));
			json_object_set_new(vj, "fmIndex", json_real(v.fmIndex));
			json_object_set_new(vj, "useFM", json_integer(v.useFM ? 1 : 0));
			json_object_set_new(vj, "ampExp", json_integer(v.ampExp ? 1 : 0));
			json_object_set_new(vj, "modEnvValue", json_real(v.modEnvValue));
			json_object_set_new(vj, "modDecayTime", json_real(v.modDecayTime));
			json_object_set_new(vj, "modEnvStage", json_integer((int)v.modEnvStage));
			json_object_set_new(vj, "modPitchEnvValue", json_real(v.modPitchEnvValue));
			json_object_set_new(vj, "modPitchDecayTime", json_real(v.modPitchDecayTime));
			json_object_set_new(vj, "modPitchUp", json_integer(v.modPitchUp ? 1 : 0));
			json_object_set_new(vj, "modPitchEnvStage", json_integer((int)v.modPitchEnvStage));
			json_object_set_new(vj, "pitchExp", json_integer(v.pitchExp ? 1 : 0));
			json_object_set_new(vj, "modAmpExp", json_integer(v.modAmpExp ? 1 : 0));
			json_object_set_new(vj, "modPitchExp", json_integer(v.modPitchExp ? 1 : 0));
			json_array_append_new(voicesJ, vj);
		}
		json_object_set_new(rootJ, "voices", voicesJ);
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		if (json_t *cj = json_object_get(rootJ, "channels")) channels = json_integer_value(cj);
		if (json_t *x = json_object_get(rootJ, "ampDecay")) params[AMP_DECAY_PARAM].setValue(clamp((float)json_real_value(x), 0.001f, 1.0f));
		if (json_t *voicesJ = json_object_get(rootJ, "voices")) {
			size_t i = 0;
			json_t *vj;
			json_array_foreach(voicesJ, i, vj) {
				if (i >= (size_t)16) break;
				Voice &v = voices[i];
				if (json_t *x = json_object_get(vj, "freq")) v.freq = json_real_value(x);
				if (json_t *x = json_object_get(vj, "phase")) v.phase = json_real_value(x);
				if (json_t *x = json_object_get(vj, "amp")) v.amp = json_real_value(x);
				if (json_t *x = json_object_get(vj, "feedback")) v.feedback = json_real_value(x);
				if (json_t *x = json_object_get(vj, "prevOut")) v.prevOut = json_real_value(x);
				if (json_t *x = json_object_get(vj, "carrierWave")) v.carrierWave = (Wave)json_integer_value(x);
				if (json_t *x = json_object_get(vj, "attackTime")) v.attackTime = json_real_value(x);
				if (json_t *x = json_object_get(vj, "decayTime")) v.decayTime = clamp(json_real_value(x), 0.001f, 1.0f);
				if (json_t *x = json_object_get(vj, "envValue")) v.envValue = json_real_value(x);
				if (json_t *x = json_object_get(vj, "envStage")) v.envStage = (EnvStage)json_integer_value(x);
				if (json_t *x = json_object_get(vj, "pitchEnvValue")) v.pitchEnvValue = json_real_value(x);
				if (json_t *x = json_object_get(vj, "pitchDecayTime")) v.pitchDecayTime = clamp(json_real_value(x), 0.001f, 1.0f);
				if (json_t *x = json_object_get(vj, "pitchUp")) v.pitchUp = json_integer_value(x) != 0;
				if (json_t *x = json_object_get(vj, "pitchEnvStage")) v.pitchEnvStage = (EnvStage)json_integer_value(x);
				if (json_t *x = json_object_get(vj, "modFreq")) v.modFreq = json_real_value(x);
				if (json_t *x = json_object_get(vj, "modPhase")) v.modPhase = json_real_value(x);
				if (json_t *x = json_object_get(vj, "modAmp")) v.modAmp = json_real_value(x);
				if (json_t *x = json_object_get(vj, "modFeedback")) v.modFeedback = json_real_value(x);
				if (json_t *x = json_object_get(vj, "modPrevOut")) v.modPrevOut = json_real_value(x);
				if (json_t *x = json_object_get(vj, "modWave")) v.modWave = (Wave)json_integer_value(x);
				if (json_t *x = json_object_get(vj, "fmIndex")) v.fmIndex = json_real_value(x);
				if (json_t *x = json_object_get(vj, "useFM")) v.useFM = json_integer_value(x) != 0;
				if (json_t *x = json_object_get(vj, "ampExp")) v.ampExp = json_integer_value(x) != 0;
				if (json_t *x = json_object_get(vj, "modEnvValue")) v.modEnvValue = json_real_value(x);
				if (json_t *x = json_object_get(vj, "modDecayTime")) v.modDecayTime = clamp(json_real_value(x), 0.001f, 1.0f);
				if (json_t *x = json_object_get(vj, "modEnvStage")) v.modEnvStage = (EnvStage)json_integer_value(x);
				if (json_t *x = json_object_get(vj, "modPitchEnvValue")) v.modPitchEnvValue = json_real_value(x);
				if (json_t *x = json_object_get(vj, "modPitchDecayTime")) v.modPitchDecayTime = clamp(json_real_value(x), 0.001f, 1.0f);
				if (json_t *x = json_object_get(vj, "modPitchUp")) v.modPitchUp = json_integer_value(x) != 0;
				if (json_t *x = json_object_get(vj, "modPitchEnvStage")) v.modPitchEnvStage = (EnvStage)json_integer_value(x);
				if (json_t *x = json_object_get(vj, "pitchExp")) v.pitchExp = json_integer_value(x) != 0;
				if (json_t *x = json_object_get(vj, "modAmpExp")) v.modAmpExp = json_integer_value(x) != 0;
				if (json_t *x = json_object_get(vj, "modPitchExp")) v.modPitchExp = json_integer_value(x) != 0;
			}
		}
		// Re-prime edge detectors after loading to avoid false first edges
		edgesPrimed = false;
	}

	void process(const ProcessArgs &args) override {
		// Prime edge detectors once to avoid treating constant-high as a rising edge on load
		bool doRandomize = false;
		bool doTrigger = false;
		if (!edgesPrimed) {
			// Consume initial states without acting
			rndTrig.process(inputs[TRIGGER_RANDOMIZE].getVoltage());
			inTrig.process(inputs[TRIGGER_INPUT].getVoltage());
			trigRndInputTrig.process(inputs[TRIG_RANDOMIZE_INPUT].getVoltage());
			edgesPrimed = true;
		} else {
			doRandomize = rndTrig.process(inputs[TRIGGER_RANDOMIZE].getVoltage());
			doTrigger = inTrig.process(inputs[TRIGGER_INPUT].getVoltage());
		}
		// Include buttons
		bool trigRnd = btnTrigRndTrig.process(params[TRIG_RANDOMIZE_BUTTON_PARAM].getValue() * 10.f) ||
		               trigRndInputTrig.process(inputs[TRIG_RANDOMIZE_INPUT].getVoltage());
		doRandomize = doRandomize || btnRndTrig.process(params[RANDOMIZE_BUTTON_PARAM].getValue() * 10.f) || trigRnd;
		doTrigger = doTrigger || btnTrig.process(params[TRIGGER_BUTTON_PARAM].getValue() * 10.f) || trigRnd;

		// Randomize on randomize trigger (input or button)
		if (doRandomize) {
			onRandomize();
		}

		// Trigger envelopes on trigger input or button (all voices)
		if (doTrigger) {
			float baseDecay = clamp(params[AMP_DECAY_PARAM].getValue(), 0.001f, 1.0f);
			// CV: 0-10V adds 0..1s
			if (inputs[DECAY_CV_INPUT].isConnected()) {
				float cv = inputs[DECAY_CV_INPUT].getVoltage();
				baseDecay = clamp(baseDecay + cv / 10.f, 0.001f, 1.0f);
			}
			for (int i = 0; i < channels && i < 16; i++) {
				Voice &v = voices[i];
				v.envStage = ENV_DECAY;
				v.envValue = 1.f;
				// Set amplitude envelope decay from the overall knob
				v.decayTime = baseDecay;
				v.pitchEnvStage = ENV_DECAY;
				v.pitchEnvValue = 1.f;
				v.modEnvStage = ENV_DECAY;
				v.modEnvValue = 1.f;
				v.modPitchEnvStage = ENV_DECAY;
				v.modPitchEnvValue = 1.f;
			}
		}
		float sr = args.sampleRate;
		outputs[VOLT_OUTPUT].setChannels(channels);
		for (int i = 0; i < channels && i < 16; i++) {
			Voice &vc = voices[i];
			// Advance envelopes
			if (vc.envStage == ENV_DECAY) {
				vc.decayTime = clamp(vc.decayTime, 0.001f, 1.0f);
				float dec = (vc.decayTime > 0.f) ? (1.f / (vc.decayTime * sr)) : 1.f;
				if (vc.ampExp) vc.envValue -= dec * vc.envValue; else vc.envValue -= dec;
				if (vc.envValue <= 0.f) { vc.envValue = 0.f; vc.envStage = ENV_IDLE; }
			}
			if (vc.pitchEnvStage == ENV_DECAY) {
				vc.pitchDecayTime = clamp(vc.pitchDecayTime, 0.001f, 1.0f);
				float pdec = (vc.pitchDecayTime > 0.f) ? (1.f / (vc.pitchDecayTime * sr)) : 1.f;
				if (vc.pitchExp) vc.pitchEnvValue -= pdec * vc.pitchEnvValue; else vc.pitchEnvValue -= pdec;
				if (vc.pitchEnvValue <= 0.f) { vc.pitchEnvValue = 0.f; vc.pitchEnvStage = ENV_IDLE; }
			}
			if (vc.modPitchEnvStage == ENV_DECAY) {
				vc.modPitchDecayTime = clamp(vc.modPitchDecayTime, 0.001f, 1.0f);
				float pdec = (vc.modPitchDecayTime > 0.f) ? (1.f / (vc.modPitchDecayTime * sr)) : 1.f;
				if (vc.modPitchExp) vc.modPitchEnvValue -= pdec * vc.modPitchEnvValue; else vc.modPitchEnvValue -= pdec;
				if (vc.modPitchEnvValue <= 0.f) { vc.modPitchEnvValue = 0.f; vc.modPitchEnvStage = ENV_IDLE; }
			}
			if (vc.modEnvStage == ENV_DECAY) {
				vc.modDecayTime = clamp(vc.modDecayTime, 0.001f, 1.0f);
				float mdec = (vc.modDecayTime > 0.f) ? (1.f / (vc.modDecayTime * sr)) : 1.f;
				if (vc.modAmpExp) vc.modEnvValue -= mdec * vc.modEnvValue; else vc.modEnvValue -= mdec;
				if (vc.modEnvValue <= 0.f) { vc.modEnvValue = 0.f; vc.modEnvStage = ENV_IDLE; }
			}
			// Pitch multipliers
			float octaves = vc.pitchOctavesMax * (vc.pitchUp ? vc.pitchEnvValue : -vc.pitchEnvValue);
			float pitchMul = powf(2.f, octaves);
			float modOct = vc.modPitchOctavesMax * (vc.modPitchUp ? vc.modPitchEnvValue : -vc.modPitchEnvValue);
			float modPitchMul = powf(2.f, modOct);
			// Modulator sample
			vc.modPhase += 2.f * M_PI * (vc.modFreq * modPitchMul) / sr;
			if (vc.modPhase > 2.f * M_PI) vc.modPhase -= 2.f * M_PI;
			float modBasic = 0.f;
			float normPhase = fmodf(vc.modPhase, 2.f * M_PI);
			if (normPhase < 0.f) normPhase += 2.f * M_PI;
			switch (vc.modWave) {
				case W_SINE:   modBasic = sinf(normPhase); break;
				case W_SQUARE: modBasic = (sinf(normPhase) >= 0.f) ? 1.f : -1.f; break;
				case W_SAW:    modBasic = (normPhase / (float)M_PI) - 1.f; break;
				case W_TRI: {
					float x = (normPhase / (float)M_PI);
					modBasic = (x < 1.f) ? (2.f * x - 1.f) : (-2.f * (x - 1.f) + 1.f);
					break;
				}
			}
			float modS = vc.modAmp * modBasic;
			modS = modS + vc.modFeedback * vc.modPrevOut;
			vc.modPrevOut = modS;
			modS *= vc.modEnvValue;
			// Carrier
			// Advance carrier with optional FM
			float phaseInc = 2.f * M_PI * (vc.freq * pitchMul) / sr;
			if (vc.useFM) phaseInc += vc.fmIndex * modS;
			vc.phase += phaseInc;
			if (vc.phase > 2.f * M_PI) vc.phase -= 2.f * M_PI;
			float carBasic = 0.f;
			float carPhaseNorm = fmodf(vc.phase, 2.f * M_PI);
			if (carPhaseNorm < 0.f) carPhaseNorm += 2.f * M_PI;
			switch (vc.carrierWave) {
				case W_SINE:   carBasic = sinf(carPhaseNorm); break;
				case W_SQUARE: carBasic = (sinf(carPhaseNorm) >= 0.f) ? 1.f : -1.f; break;
				case W_SAW:    carBasic = (carPhaseNorm / (float)M_PI) - 1.f; break;
				case W_TRI: {
					float x = (carPhaseNorm / (float)M_PI);
					carBasic = (x < 1.f) ? (2.f * x - 1.f) : (-2.f * (x - 1.f) + 1.f);
					break;
				}
			}
			float sCar = vc.amp * carBasic;
			sCar = sCar + vc.feedback * vc.prevOut;
			vc.prevOut = sCar;
			// Output routing: FM (carrier only) or additive (carrier + mod)
			float s = vc.useFM ? sCar : (sCar + modS);
			float outV = s * vc.envValue;
			if (outV > 10.f) outV = 10.f; else if (outV < -10.f) outV = -10.f;
			outputs[VOLT_OUTPUT].setVoltage(outV, i);
		}
	}

};

struct RandomSoundWidget : ModuleWidget { 
	RandomSoundWidget(RandomSound *module); 
	void appendContextMenu(Menu *menu) override;
};

RandomSoundWidget::RandomSoundWidget(RandomSound *module) {
	setModule(module);
	box.size = Vec(RACK_GRID_WIDTH*3, RACK_GRID_HEIGHT);

	setPanel(createPanel(
		asset::plugin(pluginInstance, "res/RandomSound.svg"), 
		asset::plugin(pluginInstance, "res/RandomSound.svg")
	));

	addChild(createWidget<Screw_J>(Vec(16, 2)));
	addChild(createWidget<Screw_W>(Vec(box.size.x-29, 365)));

	// Amplitude decay knob
	addParam(createParam<SmallWhiteKnob>(Vec(10, 70), module, RandomSound::AMP_DECAY_PARAM));
	addInput(createInput<TinyPJ301MPort>(Vec(15, 100), module, RandomSound::DECAY_CV_INPUT));
	// Buttons next to jacks for convenience
	addInput(createInput<TinyPJ301MPort>(Vec(5, 145), module, RandomSound::TRIGGER_RANDOMIZE));
	addParam(createParam<TinyButton>(Vec(25, 145), module, RandomSound::RANDOMIZE_BUTTON_PARAM));

	addInput(createInput<TinyPJ301MPort>(Vec(5, 192), module, RandomSound::TRIGGER_INPUT));
	addParam(createParam<TinyButton>(Vec(25, 192), module, RandomSound::TRIGGER_BUTTON_PARAM));
	// Combined trigger+randomize input and button
	addInput(createInput<TinyPJ301MPort>(Vec(5, 242), module, RandomSound::TRIG_RANDOMIZE_INPUT));
	addParam(createParam<TinyButton>(Vec(25, 242), module, RandomSound::TRIG_RANDOMIZE_BUTTON_PARAM));
	// Output
	addOutput(createOutput<TinyPJ301MPort>(Vec(15, 300), module, RandomSound::VOLT_OUTPUT));

}

	struct RSChannelValueItem : MenuItem {
		RandomSound *module;
		int channels;
		void onAction(const event::Action &e) override {
			module->channels = channels;
		}
	};

	void RandomSoundWidget::appendContextMenu(Menu *menu) {
		RandomSound *rs = dynamic_cast<RandomSound*>(module);
		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);
		struct RSChannelItem : MenuItem {
			RandomSound *module;
			Menu *createChildMenu() override {
				Menu *menu = new Menu;
				for (int channels = 1; channels <= 16; channels++) {
					RSChannelValueItem *item = new RSChannelValueItem;
					if (channels == 1) item->text = "Monophonic"; else item->text = string::f("%d", channels);
					item->rightText = CHECKMARK(module->channels == channels);
					item->module = module;
					item->channels = channels;
					menu->addChild(item);
				}
				return menu;
			}
		};
		RSChannelItem *channelItem = new RSChannelItem;
		channelItem->text = "Polyphony channels";
		channelItem->rightText = string::f("%d", rs->channels) + " " + RIGHT_ARROW;
		channelItem->module = rs;
		menu->addChild(channelItem);
	}

Model *modelRandomSound = createModel<RandomSound, RandomSoundWidget>("RandomSound");
