/*******************************************************************************************************************
Copyright (c) 2023 Cycling '74

The code that Max generates automatically and that end users are capable of
exporting and using, and any associated documentation files (the “Software”)
is a work of authorship for which Cycling '74 is the author and owner for
copyright purposes.

This Software is dual-licensed either under the terms of the Cycling '74
License for Max-Generated Code for Export, or alternatively under the terms
of the General Public License (GPL) Version 3. You may use the Software
according to either of these licenses as it is most appropriate for your
project on a case-by-case basis (proprietary or not).

A) Cycling '74 License for Max-Generated Code for Export

A license is hereby granted, free of charge, to any person obtaining a copy
of the Software (“Licensee”) to use, copy, modify, merge, publish, and
distribute copies of the Software, and to permit persons to whom the Software
is furnished to do so, subject to the following conditions:

The Software is licensed to Licensee for all uses that do not include the sale,
sublicensing, or commercial distribution of software that incorporates this
source code. This means that the Licensee is free to use this software for
educational, research, and prototyping purposes, to create musical or other
creative works with software that incorporates this source code, or any other
use that does not constitute selling software that makes use of this source
code. Commercial distribution also includes the packaging of free software with
other paid software, hardware, or software-provided commercial services.

For entities with UNDER $200k in annual revenue or funding, a license is hereby
granted, free of charge, for the sale, sublicensing, or commercial distribution
of software that incorporates this source code, for as long as the entity's
annual revenue remains below $200k annual revenue or funding.

For entities with OVER $200k in annual revenue or funding interested in the
sale, sublicensing, or commercial distribution of software that incorporates
this source code, please send inquiries to licensing@cycling74.com.

The above copyright notice and this license shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

Please see
https://support.cycling74.com/hc/en-us/articles/10730637742483-RNBO-Export-Licensing-FAQ
for additional information

B) General Public License Version 3 (GPLv3)
Details of the GPLv3 license can be found at: https://www.gnu.org/licenses/gpl-3.0.html
*******************************************************************************************************************/

#ifdef RNBO_LIB_PREFIX
#define STR_IMPL(A) #A
#define STR(A) STR_IMPL(A)
#define RNBO_LIB_INCLUDE(X) STR(RNBO_LIB_PREFIX/X)
#else
#define RNBO_LIB_INCLUDE(X) #X
#endif // RNBO_LIB_PREFIX
#ifdef RNBO_INJECTPLATFORM
#define RNBO_USECUSTOMPLATFORM
#include RNBO_INJECTPLATFORM
#endif // RNBO_INJECTPLATFORM

#include "rnbo/common/RNBO_Common.h"
#include RNBO_LIB_INCLUDE(RNBO_AudioSignal.h)

namespace RNBO {


#define trunc(x) ((Int)(x))
#define autoref auto&

#if defined(__GNUC__) || defined(__clang__)
    #define RNBO_RESTRICT __restrict__
#elif defined(_MSC_VER)
    #define RNBO_RESTRICT __restrict
#endif

#define FIXEDSIZEARRAYINIT(...) { }

template <class ENGINE = INTERNALENGINE> class rnbomatic : public PatcherInterfaceImpl {

friend class EngineCore;
friend class Engine;
friend class MinimalEngine<>;
public:

rnbomatic()
: _internalEngine(this)
{
}

~rnbomatic()
{
    deallocateSignals();
}

Index getNumMidiInputPorts() const {
    return 0;
}

void processMidiEvent(MillisecondTime , int , ConstByteArray , Index ) {}

Index getNumMidiOutputPorts() const {
    return 0;
}

void process(
    const SampleValue * const* inputs,
    Index numInputs,
    SampleValue * const* outputs,
    Index numOutputs,
    Index n
) {
    RNBO_UNUSED(numInputs);
    RNBO_UNUSED(inputs);
    this->vs = n;
    this->updateTime(this->getEngine()->getCurrentTime(), (ENGINE*)nullptr, true);
    SampleValue * out1 = (numOutputs >= 1 && outputs[0] ? outputs[0] : this->dummyBuffer);
    this->ip_01_perform(this->signals[0], n);
    this->dspexpr_01_perform(this->signals[0], this->dspexpr_01_in2, this->signals[1], n);
    this->phasor_01_perform(this->phasor_01_freq, this->signals[0], n);
    this->dspexpr_03_perform(this->signals[0], this->dspexpr_03_in2, this->signals[2], n);
    this->dspexpr_04_perform(this->signals[2], this->dspexpr_04_in2, this->signals[3], n);

    this->cycle_tilde_01_perform(
        this->cycle_tilde_01_frequency,
        this->signals[3],
        this->signals[2],
        this->dummyBuffer,
        n
    );

    this->delta_tilde_02_perform(this->signals[0], this->signals[3], n);
    this->dspexpr_15_perform(this->signals[3], this->dspexpr_15_in2, this->signals[4], n);
    this->delta_tilde_01_perform(this->signals[0], this->signals[3], n);
    this->dspexpr_12_perform(this->signals[3], this->dspexpr_12_in2, this->signals[5], n);
    this->triangle_tilde_01_perform(this->signals[0], this->triangle_tilde_01_duty, this->signals[3], n);
    this->dspexpr_10_perform(this->signals[0], this->dspexpr_10_in2, this->signals[6], n);
    this->dspexpr_09_perform(this->signals[6], this->dspexpr_09_in2, this->signals[7], n);
    this->dspexpr_08_perform(this->signals[7], this->dspexpr_08_in2, this->signals[6], n);

    this->scale_tilde_02_perform(
        this->signals[0],
        this->scale_tilde_02_lowin,
        this->scale_tilde_02_hiin,
        this->scale_tilde_02_lowout,
        this->scale_tilde_02_highout,
        this->scale_tilde_02_pow,
        this->signals[7],
        n
    );

    this->scale_tilde_01_perform(
        this->signals[0],
        this->scale_tilde_01_lowin,
        this->scale_tilde_01_hiin,
        this->scale_tilde_01_lowout,
        this->scale_tilde_01_highout,
        this->scale_tilde_01_pow,
        this->signals[8],
        n
    );

    this->noise_tilde_01_perform(this->signals[0], n);
    this->noise_tilde_02_perform(this->signals[9], n);
    this->latch_tilde_02_perform(this->signals[9], this->signals[4], this->signals[10], n);
    this->ip_02_perform(this->signals[4], n);
    this->dspexpr_14_perform(this->signals[10], this->signals[4], this->signals[9], n);
    this->dspexpr_02_perform(this->signals[2], this->signals[9], this->signals[4], n);
    this->dspexpr_05_perform(this->signals[8], this->signals[9], this->signals[2], n);
    this->dspexpr_06_perform(this->signals[7], this->signals[9], this->signals[8], n);
    this->dspexpr_07_perform(this->signals[6], this->signals[9], this->signals[7], n);
    this->dspexpr_11_perform(this->signals[3], this->signals[9], this->signals[6], n);
    this->dspexpr_13_perform(this->signals[5], this->signals[9], this->signals[3], n);
    this->latch_tilde_01_perform(this->signals[0], this->signals[3], this->signals[9], n);

    this->selector_01_perform(
        this->signals[1],
        this->signals[4],
        this->signals[2],
        this->signals[8],
        this->signals[7],
        this->signals[6],
        this->signals[9],
        out1,
        n
    );

    this->stackprotect_perform(n);
    this->globaltransport_advance();
    this->advanceTime((ENGINE*)nullptr);
    this->audioProcessSampleCount += this->vs;
}

void prepareToProcess(number sampleRate, Index maxBlockSize, bool force) {
    RNBO_ASSERT(this->_isInitialized);

    if (this->maxvs < maxBlockSize || !this->didAllocateSignals) {
        Index i;

        for (i = 0; i < 11; i++) {
            this->signals[i] = resizeSignal(this->signals[i], this->maxvs, maxBlockSize);
        }

        this->ip_01_sigbuf = resizeSignal(this->ip_01_sigbuf, this->maxvs, maxBlockSize);
        this->phasor_01_sigbuf = resizeSignal(this->phasor_01_sigbuf, this->maxvs, maxBlockSize);
        this->ip_02_sigbuf = resizeSignal(this->ip_02_sigbuf, this->maxvs, maxBlockSize);
        this->globaltransport_tempo = resizeSignal(this->globaltransport_tempo, this->maxvs, maxBlockSize);
        this->globaltransport_state = resizeSignal(this->globaltransport_state, this->maxvs, maxBlockSize);
        this->zeroBuffer = resizeSignal(this->zeroBuffer, this->maxvs, maxBlockSize);
        this->dummyBuffer = resizeSignal(this->dummyBuffer, this->maxvs, maxBlockSize);
        this->didAllocateSignals = true;
    }

    const bool sampleRateChanged = sampleRate != this->sr;
    const bool maxvsChanged = maxBlockSize != this->maxvs;
    const bool forceDSPSetup = sampleRateChanged || maxvsChanged || force;

    if (sampleRateChanged || maxvsChanged) {
        this->vs = maxBlockSize;
        this->maxvs = maxBlockSize;
        this->sr = sampleRate;
        this->invsr = 1 / sampleRate;
    }

    this->ip_01_dspsetup(forceDSPSetup);
    this->phasor_01_dspsetup(forceDSPSetup);
    this->cycle_tilde_01_dspsetup(forceDSPSetup);
    this->delta_tilde_02_dspsetup(forceDSPSetup);
    this->delta_tilde_01_dspsetup(forceDSPSetup);
    this->noise_tilde_01_dspsetup(forceDSPSetup);
    this->noise_tilde_02_dspsetup(forceDSPSetup);
    this->latch_tilde_02_dspsetup(forceDSPSetup);
    this->ip_02_dspsetup(forceDSPSetup);
    this->latch_tilde_01_dspsetup(forceDSPSetup);
    this->globaltransport_dspsetup(forceDSPSetup);

    if (sampleRateChanged)
        this->onSampleRateChanged(sampleRate);
}

number msToSamps(MillisecondTime ms, number sampleRate) {
    return ms * sampleRate * 0.001;
}

MillisecondTime sampsToMs(SampleIndex samps) {
    return samps * (this->invsr * 1000);
}

Index getNumInputChannels() const {
    return 0;
}

Index getNumOutputChannels() const {
    return 1;
}

DataRef* getDataRef(DataRefIndex index)  {
    switch (index) {
    case 0:
        {
        return addressOf(this->RNBODefaultSinus);
        break;
        }
    default:
        {
        return nullptr;
        }
    }
}

DataRefIndex getNumDataRefs() const {
    return 1;
}

void processDataViewUpdate(DataRefIndex index, MillisecondTime time) {
    this->updateTime(time, (ENGINE*)nullptr);

    if (index == 0) {
        this->cycle_tilde_01_buffer = reInitDataView(this->cycle_tilde_01_buffer, this->RNBODefaultSinus);
        this->cycle_tilde_01_bufferUpdated();
    }
}

void initialize() {
    RNBO_ASSERT(!this->_isInitialized);

    this->RNBODefaultSinus = initDataRef(
        this->RNBODefaultSinus,
        this->dataRefStrings->name0,
        true,
        this->dataRefStrings->file0,
        this->dataRefStrings->tag0
    );

    this->assign_defaults();
    this->applyState();
    this->RNBODefaultSinus->setIndex(0);
    this->cycle_tilde_01_buffer = new SampleBuffer(this->RNBODefaultSinus);
    this->initializeObjects();
    this->allocateDataRefs();
    this->startup();
    this->_isInitialized = true;
}

void processTempoEvent(MillisecondTime time, Tempo tempo) {
    this->updateTime(time, (ENGINE*)nullptr);

    if (this->globaltransport_setTempo(this->_currentTime, tempo, false))
        {}
}

void processTransportEvent(MillisecondTime time, TransportState state) {
    this->updateTime(time, (ENGINE*)nullptr);

    if (this->globaltransport_setState(this->_currentTime, state, false))
        {}
}

void processBeatTimeEvent(MillisecondTime time, BeatTime beattime) {
    this->updateTime(time, (ENGINE*)nullptr);

    if (this->globaltransport_setBeatTime(this->_currentTime, beattime, false)) {
        this->phasor_01_onBeatTimeChanged(beattime);
    }
}

void processTimeSignatureEvent(MillisecondTime time, Int numerator, Int denominator) {
    this->updateTime(time, (ENGINE*)nullptr);

    if (this->globaltransport_setTimeSignature(this->_currentTime, numerator, denominator, false))
        {}
}

void processBBUEvent(MillisecondTime time, number bars, number beats, number units) {
    this->updateTime(time, (ENGINE*)nullptr);

    if (this->globaltransport_setBBU(this->_currentTime, bars, beats, units, false))
        {}
}

void getPreset(PatcherStateInterface& preset) {
    this->updateTime(this->getEngine()->getCurrentTime(), (ENGINE*)nullptr);
    preset["__presetid"] = "rnbo";
    this->param_01_getPresetValue(getSubState(preset, "type"));
    this->param_02_getPresetValue(getSubState(preset, "frequency"));
    this->param_03_getPresetValue(getSubState(preset, "chance"));
}

void setPreset(MillisecondTime time, PatcherStateInterface& preset) {
    this->updateTime(time, (ENGINE*)nullptr);
    this->param_01_setPresetValue(getSubState(preset, "type"));
    this->param_02_setPresetValue(getSubState(preset, "frequency"));
    this->param_03_setPresetValue(getSubState(preset, "chance"));
}

void setParameterValue(ParameterIndex index, ParameterValue v, MillisecondTime time) {
    this->updateTime(time, (ENGINE*)nullptr);

    switch (index) {
    case 0:
        {
        this->param_01_value_set(v);
        break;
        }
    case 1:
        {
        this->param_02_value_set(v);
        break;
        }
    case 2:
        {
        this->param_03_value_set(v);
        break;
        }
    }
}

void processParameterEvent(ParameterIndex index, ParameterValue value, MillisecondTime time) {
    this->setParameterValue(index, value, time);
}

void processParameterBangEvent(ParameterIndex index, MillisecondTime time) {
    this->setParameterValue(index, this->getParameterValue(index), time);
}

void processNormalizedParameterEvent(ParameterIndex index, ParameterValue value, MillisecondTime time) {
    this->setParameterValueNormalized(index, value, time);
}

ParameterValue getParameterValue(ParameterIndex index)  {
    switch (index) {
    case 0:
        {
        return this->param_01_value;
        }
    case 1:
        {
        return this->param_02_value;
        }
    case 2:
        {
        return this->param_03_value;
        }
    default:
        {
        return 0;
        }
    }
}

ParameterIndex getNumSignalInParameters() const {
    return 0;
}

ParameterIndex getNumSignalOutParameters() const {
    return 0;
}

ParameterIndex getNumParameters() const {
    return 3;
}

ConstCharPointer getParameterName(ParameterIndex index) const {
    switch (index) {
    case 0:
        {
        return "type";
        }
    case 1:
        {
        return "frequency";
        }
    case 2:
        {
        return "chance";
        }
    default:
        {
        return "bogus";
        }
    }
}

ConstCharPointer getParameterId(ParameterIndex index) const {
    switch (index) {
    case 0:
        {
        return "type";
        }
    case 1:
        {
        return "frequency";
        }
    case 2:
        {
        return "chance";
        }
    default:
        {
        return "bogus";
        }
    }
}

void getParameterInfo(ParameterIndex index, ParameterInfo * info) const {
    {
        switch (index) {
        case 0:
            {
            info->type = ParameterTypeNumber;
            info->initialValue = 0;
            info->min = 0;
            info->max = 5;
            info->exponent = 1;
            info->steps = 6;
            static const char * eVal0[] = {"sine", "rampup", "rampdown", "square", "triangle", "random"};
            info->enumValues = eVal0;
            info->debug = false;
            info->saveable = true;
            info->transmittable = true;
            info->initialized = true;
            info->visible = true;
            info->displayName = "";
            info->unit = "";
            info->ioType = IOTypeUndefined;
            info->signalIndex = INVALID_INDEX;
            break;
            }
        case 1:
            {
            info->type = ParameterTypeNumber;
            info->initialValue = 0.1;
            info->min = 0.1;
            info->max = 40;
            info->exponent = 1;
            info->steps = 0;
            info->debug = false;
            info->saveable = true;
            info->transmittable = true;
            info->initialized = true;
            info->visible = true;
            info->displayName = "";
            info->unit = "";
            info->ioType = IOTypeUndefined;
            info->signalIndex = INVALID_INDEX;
            break;
            }
        case 2:
            {
            info->type = ParameterTypeNumber;
            info->initialValue = 1;
            info->min = 0;
            info->max = 1;
            info->exponent = 1;
            info->steps = 0;
            info->debug = false;
            info->saveable = true;
            info->transmittable = true;
            info->initialized = true;
            info->visible = true;
            info->displayName = "";
            info->unit = "";
            info->ioType = IOTypeUndefined;
            info->signalIndex = INVALID_INDEX;
            break;
            }
        }
    }
}

ParameterValue applyStepsToNormalizedParameterValue(ParameterValue normalizedValue, int steps) const {
    if (steps == 1) {
        if (normalizedValue > 0) {
            normalizedValue = 1.;
        }
    } else {
        ParameterValue oneStep = (number)1. / (steps - 1);
        ParameterValue numberOfSteps = rnbo_fround(normalizedValue / oneStep * 1 / (number)1) * (number)1;
        normalizedValue = numberOfSteps * oneStep;
    }

    return normalizedValue;
}

ParameterValue convertToNormalizedParameterValue(ParameterIndex index, ParameterValue value) const {
    switch (index) {
    case 2:
        {
        {
            value = (value < 0 ? 0 : (value > 1 ? 1 : value));
            ParameterValue normalizedValue = (value - 0) / (1 - 0);
            return normalizedValue;
        }
        }
    case 0:
        {
        {
            value = (value < 0 ? 0 : (value > 5 ? 5 : value));
            ParameterValue normalizedValue = (value - 0) / (5 - 0);

            {
                normalizedValue = this->applyStepsToNormalizedParameterValue(normalizedValue, 6);
            }

            return normalizedValue;
        }
        }
    case 1:
        {
        {
            value = (value < 0.1 ? 0.1 : (value > 40 ? 40 : value));
            ParameterValue normalizedValue = (value - 0.1) / (40 - 0.1);
            return normalizedValue;
        }
        }
    default:
        {
        return value;
        }
    }
}

ParameterValue convertFromNormalizedParameterValue(ParameterIndex index, ParameterValue value) const {
    value = (value < 0 ? 0 : (value > 1 ? 1 : value));

    switch (index) {
    case 2:
        {
        {
            {
                return 0 + value * (1 - 0);
            }
        }
        }
    case 0:
        {
        {
            {
                value = this->applyStepsToNormalizedParameterValue(value, 6);
            }

            {
                return 0 + value * (5 - 0);
            }
        }
        }
    case 1:
        {
        {
            {
                return 0.1 + value * (40 - 0.1);
            }
        }
        }
    default:
        {
        return value;
        }
    }
}

ParameterValue constrainParameterValue(ParameterIndex index, ParameterValue value) const {
    switch (index) {
    case 0:
        {
        return this->param_01_value_constrain(value);
        }
    case 1:
        {
        return this->param_02_value_constrain(value);
        }
    case 2:
        {
        return this->param_03_value_constrain(value);
        }
    default:
        {
        return value;
        }
    }
}

void processNumMessage(MessageTag , MessageTag , MillisecondTime , number ) {}

void processListMessage(MessageTag , MessageTag , MillisecondTime , const list& ) {}

void processBangMessage(MessageTag , MessageTag , MillisecondTime ) {}

MessageTagInfo resolveTag(MessageTag tag) const {
    switch (tag) {

    }

    return "";
}

MessageIndex getNumMessages() const {
    return 0;
}

const MessageInfo& getMessageInfo(MessageIndex index) const {
    switch (index) {

    }

    return NullMessageInfo;
}

protected:

		
void advanceTime(EXTERNALENGINE*) {}
void advanceTime(INTERNALENGINE*) {
	_internalEngine.advanceTime(sampstoms(this->vs));
}

void processInternalEvents(MillisecondTime time) {
	_internalEngine.processEventsUntil(time);
}

void updateTime(MillisecondTime time, INTERNALENGINE*, bool inProcess = false) {
	if (time == TimeNow) time = getPatcherTime();
	processInternalEvents(inProcess ? time + sampsToMs(this->vs) : time);
	updateTime(time, (EXTERNALENGINE*)nullptr);
}

rnbomatic* operator->() {
    return this;
}
const rnbomatic* operator->() const {
    return this;
}
rnbomatic* getTopLevelPatcher() {
    return this;
}

void cancelClockEvents()
{
}

template<typename LISTTYPE = list> void listquicksort(LISTTYPE& arr, LISTTYPE& sortindices, Int l, Int h, bool ascending) {
    if (l < h) {
        Int p = (Int)(this->listpartition(arr, sortindices, l, h, ascending));
        this->listquicksort(arr, sortindices, l, p - 1, ascending);
        this->listquicksort(arr, sortindices, p + 1, h, ascending);
    }
}

template<typename LISTTYPE = list> Int listpartition(LISTTYPE& arr, LISTTYPE& sortindices, Int l, Int h, bool ascending) {
    number x = arr[(Index)h];
    Int i = (Int)(l - 1);

    for (Int j = (Int)(l); j <= h - 1; j++) {
        bool asc = (bool)((bool)(ascending) && arr[(Index)j] <= x);
        bool desc = (bool)((bool)(!(bool)(ascending)) && arr[(Index)j] >= x);

        if ((bool)(asc) || (bool)(desc)) {
            i++;
            this->listswapelements(arr, i, j);
            this->listswapelements(sortindices, i, j);
        }
    }

    i++;
    this->listswapelements(arr, i, h);
    this->listswapelements(sortindices, i, h);
    return i;
}

template<typename LISTTYPE = list> void listswapelements(LISTTYPE& arr, Int a, Int b) {
    auto tmp = arr[(Index)a];
    arr[(Index)a] = arr[(Index)b];
    arr[(Index)b] = tmp;
}

inline number safemod(number f, number m) {
    if (m != 0) {
        Int f_trunc = (Int)(trunc(f));
        Int m_trunc = (Int)(trunc(m));

        if (f == f_trunc && m == m_trunc) {
            f = f_trunc % m_trunc;
        } else {
            if (m < 0) {
                m = -m;
            }

            if (f >= m) {
                if (f >= m * 2.0) {
                    number d = f / m;
                    Int i = (Int)(trunc(d));
                    d = d - i;
                    f = d * m;
                } else {
                    f -= m;
                }
            } else if (f <= -m) {
                if (f <= -m * 2.0) {
                    number d = f / m;
                    Int i = (Int)(trunc(d));
                    d = d - i;
                    f = d * m;
                } else {
                    f += m;
                }
            }
        }
    } else {
        f = 0.0;
    }

    return f;
}

inline number safediv(number num, number denom) {
    return (denom == 0.0 ? 0.0 : num / denom);
}

number safepow(number base, number exponent) {
    return fixnan(rnbo_pow(base, exponent));
}

number wrap(number x, number low, number high) {
    number lo;
    number hi;

    if (low == high)
        return low;

    if (low > high) {
        hi = low;
        lo = high;
    } else {
        lo = low;
        hi = high;
    }

    number range = hi - lo;

    if (x >= lo && x < hi)
        return x;

    if (range <= 0.000000001)
        return lo;

    Int numWraps = (Int)(trunc((x - lo) / range));
    numWraps = numWraps - ((x < lo ? 1 : 0));
    number result = x - range * numWraps;

    if (result >= hi)
        return result - range;
    else
        return result;
}

Index voice() {
    return this->_voiceIndex;
}

number random(number low, number high) {
    number range = high - low;
    return globalrandom() * range + low;
}

number mstosamps(MillisecondTime ms) {
    return ms * this->sr * 0.001;
}

number maximum(number x, number y) {
    return (x < y ? y : x);
}

MillisecondTime sampstoms(number samps) {
    return samps * 1000 / this->sr;
}

void param_01_value_set(number v) {
    v = this->param_01_value_constrain(v);
    this->param_01_value = v;
    this->sendParameter(0, false);

    if (this->param_01_value != this->param_01_lastValue) {
        {
            this->getEngine()->presetTouched();
        }

        this->param_01_lastValue = this->param_01_value;
    }

    this->ip_01_value_set(v);
}

void param_02_value_set(number v) {
    v = this->param_02_value_constrain(v);
    this->param_02_value = v;
    this->sendParameter(1, false);

    if (this->param_02_value != this->param_02_lastValue) {
        {
            this->getEngine()->presetTouched();
        }

        this->param_02_lastValue = this->param_02_value;
    }

    this->phasor_01_freq_set(v);
}

void param_03_value_set(number v) {
    v = this->param_03_value_constrain(v);
    this->param_03_value = v;
    this->sendParameter(2, false);

    if (this->param_03_value != this->param_03_lastValue) {
        {
            this->getEngine()->presetTouched();
        }

        this->param_03_lastValue = this->param_03_value;
    }

    this->ip_02_value_set(v);
}

MillisecondTime getPatcherTime() const {
    return this->_currentTime;
}

void deallocateSignals() {
    Index i;

    for (i = 0; i < 11; i++) {
        this->signals[i] = freeSignal(this->signals[i]);
    }

    this->ip_01_sigbuf = freeSignal(this->ip_01_sigbuf);
    this->phasor_01_sigbuf = freeSignal(this->phasor_01_sigbuf);
    this->ip_02_sigbuf = freeSignal(this->ip_02_sigbuf);
    this->globaltransport_tempo = freeSignal(this->globaltransport_tempo);
    this->globaltransport_state = freeSignal(this->globaltransport_state);
    this->zeroBuffer = freeSignal(this->zeroBuffer);
    this->dummyBuffer = freeSignal(this->dummyBuffer);
}

Index getMaxBlockSize() const {
    return this->maxvs;
}

number getSampleRate() const {
    return this->sr;
}

bool hasFixedVectorSize() const {
    return false;
}

void setProbingTarget(MessageTag ) {}

void fillRNBODefaultSinus(DataRef& ref) {
    SampleBuffer buffer(ref);
    number bufsize = buffer->getSize();

    for (Index i = 0; i < bufsize; i++) {
        buffer[i] = rnbo_cos(i * 3.14159265358979323846 * 2. / bufsize);
    }
}

void fillDataRef(DataRefIndex index, DataRef& ref) {
    switch (index) {
    case 0:
        {
        this->fillRNBODefaultSinus(ref);
        break;
        }
    }
}

void allocateDataRefs() {
    this->cycle_tilde_01_buffer->requestSize(16384, 1);
    this->cycle_tilde_01_buffer->setSampleRate(this->sr);
    this->cycle_tilde_01_buffer = this->cycle_tilde_01_buffer->allocateIfNeeded();

    if (this->RNBODefaultSinus->hasRequestedSize()) {
        if (this->RNBODefaultSinus->wantsFill())
            this->fillRNBODefaultSinus(this->RNBODefaultSinus);

        this->getEngine()->sendDataRefUpdated(0);
    }
}

void initializeObjects() {
    this->ip_01_init();
    this->latch_tilde_01_init();
    this->noise_tilde_01_nz_init();
    this->latch_tilde_02_init();
    this->noise_tilde_02_nz_init();
    this->ip_02_init();
}

Index getIsMuted()  {
    return this->isMuted;
}

void setIsMuted(Index v)  {
    this->isMuted = v;
}

void onSampleRateChanged(double ) {}

void extractState(PatcherStateInterface& ) {}

void applyState() {}

void processClockEvent(MillisecondTime , ClockId , bool , ParameterValue ) {}

void processOutletAtCurrentTime(EngineLink* , OutletIndex , ParameterValue ) {}

void processOutletEvent(
    EngineLink* sender,
    OutletIndex index,
    ParameterValue value,
    MillisecondTime time
) {
    this->updateTime(time, (ENGINE*)nullptr);
    this->processOutletAtCurrentTime(sender, index, value);
}

void sendOutlet(OutletIndex index, ParameterValue value) {
    this->getEngine()->sendOutlet(this, index, value);
}

void startup() {
    this->updateTime(this->getEngine()->getCurrentTime(), (ENGINE*)nullptr);

    {
        this->scheduleParamInit(0, 0);
    }

    {
        this->scheduleParamInit(1, 0);
    }

    {
        this->scheduleParamInit(2, 0);
    }

    this->processParamInitEvents();
}

number param_01_value_constrain(number v) const {
    v = (v > 5 ? 5 : (v < 0 ? 0 : v));

    {
        number oneStep = (number)5 / (number)5;
        number oneStepInv = (oneStep != 0 ? (number)1 / oneStep : 0);
        number numberOfSteps = rnbo_fround((v - 0) * oneStepInv * 1 / (number)1) * 1;
        v = numberOfSteps * oneStep + 0;
    }

    return v;
}

void ip_01_value_set(number v) {
    this->ip_01_value = v;
    this->ip_01_fillSigBuf();
    this->ip_01_lastValue = v;
}

number param_02_value_constrain(number v) const {
    v = (v > 40 ? 40 : (v < 0.1 ? 0.1 : v));
    return v;
}

void phasor_01_freq_set(number v) {
    this->phasor_01_freq = v;
}

number param_03_value_constrain(number v) const {
    v = (v > 1 ? 1 : (v < 0 ? 0 : v));
    return v;
}

void ip_02_value_set(number v) {
    this->ip_02_value = v;
    this->ip_02_fillSigBuf();
    this->ip_02_lastValue = v;
}

void ip_01_perform(SampleValue * out, Index n) {
    auto __ip_01_lastValue = this->ip_01_lastValue;
    auto __ip_01_lastIndex = this->ip_01_lastIndex;

    for (Index i = 0; i < n; i++) {
        out[(Index)i] = ((SampleIndex)(i) >= __ip_01_lastIndex ? __ip_01_lastValue : this->ip_01_sigbuf[(Index)i]);
    }

    __ip_01_lastIndex = 0;
    this->ip_01_lastIndex = __ip_01_lastIndex;
}

void dspexpr_01_perform(const Sample * in1, number in2, SampleValue * out1, Index n) {
    RNBO_UNUSED(in2);
    Index i;

    for (i = 0; i < (Index)n; i++) {
        out1[(Index)i] = in1[(Index)i] + 1;//#map:_###_obj_###_:1
    }
}

void phasor_01_perform(number freq, SampleValue * out, Index n) {
    for (Index i = 0; i < n; i++) {
        out[(Index)i] = this->phasor_01_ph_next(freq, -1);
        this->phasor_01_sigbuf[(Index)i] = -1;
    }
}

void dspexpr_03_perform(const Sample * in1, number in2, SampleValue * out1, Index n) {
    RNBO_UNUSED(in2);
    Index i;

    for (i = 0; i < (Index)n; i++) {
        out1[(Index)i] = in1[(Index)i] + 0.75;//#map:_###_obj_###_:1
    }
}

void dspexpr_04_perform(const Sample * in1, number in2, SampleValue * out1, Index n) {
    RNBO_UNUSED(in2);
    Index i;

    for (i = 0; i < (Index)n; i++) {
        out1[(Index)i] = this->safemod(in1[(Index)i], 1);//#map:_###_obj_###_:1
    }
}

void cycle_tilde_01_perform(
    number frequency,
    const Sample * phase_offset,
    SampleValue * out1,
    SampleValue * out2,
    Index n
) {
    RNBO_UNUSED(frequency);
    auto __cycle_tilde_01_f2i = this->cycle_tilde_01_f2i;
    auto __cycle_tilde_01_buffer = this->cycle_tilde_01_buffer;
    auto __cycle_tilde_01_phasei = this->cycle_tilde_01_phasei;
    Index i;

    for (i = 0; i < (Index)n; i++) {
        {
            UInt32 uint_phase;

            if (phase_offset[(Index)i] != 0 || 0 == 2) {
                uint_phase = uint32_add(
                    uint32_trunc(phase_offset[(Index)i] * 4294967296.0),
                    __cycle_tilde_01_phasei
                );
            } else {
                uint_phase = __cycle_tilde_01_phasei;
            }

            UInt32 idx = (UInt32)(uint32_rshift(uint_phase, 18));
            number frac = ((BinOpInt)((BinOpInt)uint_phase & (BinOpInt)262143)) * 3.81471181759574e-6;
            number y0 = __cycle_tilde_01_buffer[(Index)idx];
            number y1 = __cycle_tilde_01_buffer[(Index)((BinOpInt)(idx + 1) & (BinOpInt)16383)];
            number y = y0 + frac * (y1 - y0);

            {
                UInt32 pincr = (UInt32)(uint32_trunc(0 * __cycle_tilde_01_f2i));
                __cycle_tilde_01_phasei = uint32_add(__cycle_tilde_01_phasei, pincr);
            }

            out1[(Index)i] = y;
            out2[(Index)i] = uint_phase * 0.232830643653869629e-9;
            continue;
        }
    }

    this->cycle_tilde_01_phasei = __cycle_tilde_01_phasei;
}

void delta_tilde_02_perform(const Sample * x, SampleValue * out1, Index n) {
    auto __delta_tilde_02_prev = this->delta_tilde_02_prev;
    Index i;

    for (i = 0; i < (Index)n; i++) {
        number temp = (number)(x[(Index)i] - __delta_tilde_02_prev);
        __delta_tilde_02_prev = x[(Index)i];
        out1[(Index)i] = temp;
    }

    this->delta_tilde_02_prev = __delta_tilde_02_prev;
}

void dspexpr_15_perform(const Sample * in1, number in2, SampleValue * out1, Index n) {
    RNBO_UNUSED(in2);
    Index i;

    for (i = 0; i < (Index)n; i++) {
        out1[(Index)i] = in1[(Index)i] < 0;//#map:_###_obj_###_:1
    }
}

void delta_tilde_01_perform(const Sample * x, SampleValue * out1, Index n) {
    auto __delta_tilde_01_prev = this->delta_tilde_01_prev;
    Index i;

    for (i = 0; i < (Index)n; i++) {
        number temp = (number)(x[(Index)i] - __delta_tilde_01_prev);
        __delta_tilde_01_prev = x[(Index)i];
        out1[(Index)i] = temp;
    }

    this->delta_tilde_01_prev = __delta_tilde_01_prev;
}

void dspexpr_12_perform(const Sample * in1, number in2, SampleValue * out1, Index n) {
    RNBO_UNUSED(in2);
    Index i;

    for (i = 0; i < (Index)n; i++) {
        out1[(Index)i] = in1[(Index)i] < 0;//#map:_###_obj_###_:1
    }
}

void triangle_tilde_01_perform(const Sample * phase, number duty, SampleValue * out1, Index n) {
    RNBO_UNUSED(duty);
    Index i;

    for (i = 0; i < (Index)n; i++) {
        number p1 = 0.5;
        auto wrappedPhase = this->wrap(phase[(Index)i], 0., 1.);
        p1 = (p1 > 1. ? 1. : (p1 < 0. ? 0. : p1));

        if (wrappedPhase < p1) {
            out1[(Index)i] = wrappedPhase / p1;
            continue;
        } else {
            out1[(Index)i] = (p1 == 1. ? wrappedPhase : 1. - (wrappedPhase - p1) / (1. - p1));
            continue;
        }
    }
}

void dspexpr_10_perform(const Sample * in1, number in2, SampleValue * out1, Index n) {
    RNBO_UNUSED(in2);
    Index i;

    for (i = 0; i < (Index)n; i++) {
        out1[(Index)i] = in1[(Index)i] < 0.5;//#map:_###_obj_###_:1
    }
}

void dspexpr_09_perform(const Sample * in1, number in2, SampleValue * out1, Index n) {
    RNBO_UNUSED(in2);
    Index i;

    for (i = 0; i < (Index)n; i++) {
        out1[(Index)i] = in1[(Index)i] * 2;//#map:_###_obj_###_:1
    }
}

void dspexpr_08_perform(const Sample * in1, number in2, SampleValue * out1, Index n) {
    RNBO_UNUSED(in2);
    Index i;

    for (i = 0; i < (Index)n; i++) {
        out1[(Index)i] = in1[(Index)i] - 1;//#map:_###_obj_###_:1
    }
}

void scale_tilde_02_perform(
    const Sample * x,
    number lowin,
    number hiin,
    number lowout,
    number highout,
    number pow,
    SampleValue * out1,
    Index n
) {
    RNBO_UNUSED(pow);
    RNBO_UNUSED(highout);
    RNBO_UNUSED(lowout);
    RNBO_UNUSED(hiin);
    RNBO_UNUSED(lowin);
    auto inscale = this->safediv(1., 1 - 0);
    number outdiff = -1 - 1;
    Index i;

    for (i = 0; i < (Index)n; i++) {
        number value = (x[(Index)i] - 0) * inscale;
        value = value * outdiff + 1;
        out1[(Index)i] = value;
    }
}

void scale_tilde_01_perform(
    const Sample * x,
    number lowin,
    number hiin,
    number lowout,
    number highout,
    number pow,
    SampleValue * out1,
    Index n
) {
    RNBO_UNUSED(pow);
    RNBO_UNUSED(highout);
    RNBO_UNUSED(lowout);
    RNBO_UNUSED(hiin);
    RNBO_UNUSED(lowin);
    auto inscale = this->safediv(1., 1 - 0);
    number outdiff = 1 - -1;
    Index i;

    for (i = 0; i < (Index)n; i++) {
        number value = (x[(Index)i] - 0) * inscale;
        value = value * outdiff + -1;
        out1[(Index)i] = value;
    }
}

void noise_tilde_01_perform(SampleValue * out, Index n) {
    for (Index i = 0; i < n; i++) {
        out[(Index)i] = this->noise_tilde_01_nz_next();
    }
}

void noise_tilde_02_perform(SampleValue * out, Index n) {
    for (Index i = 0; i < n; i++) {
        out[(Index)i] = this->noise_tilde_02_nz_next();
    }
}

void latch_tilde_02_perform(const Sample * x, const Sample * control, SampleValue * out1, Index n) {
    auto __latch_tilde_02_value = this->latch_tilde_02_value;
    Index i;

    for (i = 0; i < (Index)n; i++) {
        if (control[(Index)i] != 0.) {
            __latch_tilde_02_value = x[(Index)i];
        }

        out1[(Index)i] = __latch_tilde_02_value;
    }

    this->latch_tilde_02_value = __latch_tilde_02_value;
}

void ip_02_perform(SampleValue * out, Index n) {
    auto __ip_02_lastValue = this->ip_02_lastValue;
    auto __ip_02_lastIndex = this->ip_02_lastIndex;

    for (Index i = 0; i < n; i++) {
        out[(Index)i] = ((SampleIndex)(i) >= __ip_02_lastIndex ? __ip_02_lastValue : this->ip_02_sigbuf[(Index)i]);
    }

    __ip_02_lastIndex = 0;
    this->ip_02_lastIndex = __ip_02_lastIndex;
}

void dspexpr_14_perform(const Sample * in1, const Sample * in2, SampleValue * out1, Index n) {
    Index i;

    for (i = 0; i < (Index)n; i++) {
        out1[(Index)i] = in1[(Index)i] < in2[(Index)i];//#map:_###_obj_###_:1
    }
}

void dspexpr_02_perform(const Sample * in1, const Sample * in2, SampleValue * out1, Index n) {
    Index i;

    for (i = 0; i < (Index)n; i++) {
        out1[(Index)i] = in1[(Index)i] * in2[(Index)i];//#map:_###_obj_###_:1
    }
}

void dspexpr_05_perform(const Sample * in1, const Sample * in2, SampleValue * out1, Index n) {
    Index i;

    for (i = 0; i < (Index)n; i++) {
        out1[(Index)i] = in1[(Index)i] * in2[(Index)i];//#map:_###_obj_###_:1
    }
}

void dspexpr_06_perform(const Sample * in1, const Sample * in2, SampleValue * out1, Index n) {
    Index i;

    for (i = 0; i < (Index)n; i++) {
        out1[(Index)i] = in1[(Index)i] * in2[(Index)i];//#map:_###_obj_###_:1
    }
}

void dspexpr_07_perform(const Sample * in1, const Sample * in2, SampleValue * out1, Index n) {
    Index i;

    for (i = 0; i < (Index)n; i++) {
        out1[(Index)i] = in1[(Index)i] * in2[(Index)i];//#map:_###_obj_###_:1
    }
}

void dspexpr_11_perform(const Sample * in1, const Sample * in2, SampleValue * out1, Index n) {
    Index i;

    for (i = 0; i < (Index)n; i++) {
        out1[(Index)i] = in1[(Index)i] * in2[(Index)i];//#map:_###_obj_###_:1
    }
}

void dspexpr_13_perform(const Sample * in1, const Sample * in2, SampleValue * out1, Index n) {
    Index i;

    for (i = 0; i < (Index)n; i++) {
        out1[(Index)i] = in1[(Index)i] * in2[(Index)i];//#map:_###_obj_###_:1
    }
}

void latch_tilde_01_perform(const Sample * x, const Sample * control, SampleValue * out1, Index n) {
    auto __latch_tilde_01_value = this->latch_tilde_01_value;
    Index i;

    for (i = 0; i < (Index)n; i++) {
        if (control[(Index)i] != 0.) {
            __latch_tilde_01_value = x[(Index)i];
        }

        out1[(Index)i] = __latch_tilde_01_value;
    }

    this->latch_tilde_01_value = __latch_tilde_01_value;
}

void selector_01_perform(
    const Sample * onoff,
    const SampleValue * in1,
    const SampleValue * in2,
    const SampleValue * in3,
    const SampleValue * in4,
    const SampleValue * in5,
    const SampleValue * in6,
    SampleValue * out,
    Index n
) {
    Index i;

    for (i = 0; i < (Index)n; i++) {
        if (onoff[(Index)i] >= 1 && onoff[(Index)i] < 2)
            out[(Index)i] = in1[(Index)i];
        else if (onoff[(Index)i] >= 2 && onoff[(Index)i] < 3)
            out[(Index)i] = in2[(Index)i];
        else if (onoff[(Index)i] >= 3 && onoff[(Index)i] < 4)
            out[(Index)i] = in3[(Index)i];
        else if (onoff[(Index)i] >= 4 && onoff[(Index)i] < 5)
            out[(Index)i] = in4[(Index)i];
        else if (onoff[(Index)i] >= 5 && onoff[(Index)i] < 6)
            out[(Index)i] = in5[(Index)i];
        else if (onoff[(Index)i] >= 6 && onoff[(Index)i] < 7)
            out[(Index)i] = in6[(Index)i];
        else
            out[(Index)i] = 0;
    }
}

void stackprotect_perform(Index n) {
    RNBO_UNUSED(n);
    auto __stackprotect_count = this->stackprotect_count;
    __stackprotect_count = 0;
    this->stackprotect_count = __stackprotect_count;
}

void ip_01_init() {
    this->ip_01_lastValue = this->ip_01_value;
}

void ip_01_fillSigBuf() {
    if ((bool)(this->ip_01_sigbuf)) {
        SampleIndex k = (SampleIndex)(this->sampleOffsetIntoNextAudioBuffer);

        if (k >= (SampleIndex)(this->vs))
            k = (SampleIndex)(this->vs) - 1;

        for (SampleIndex i = (SampleIndex)(this->ip_01_lastIndex); i < k; i++) {
            if (this->ip_01_resetCount > 0) {
                this->ip_01_sigbuf[(Index)i] = 1;
                this->ip_01_resetCount--;
            } else {
                this->ip_01_sigbuf[(Index)i] = this->ip_01_lastValue;
            }
        }

        this->ip_01_lastIndex = k;
    }
}

void ip_01_dspsetup(bool force) {
    if ((bool)(this->ip_01_setupDone) && (bool)(!(bool)(force)))
        return;

    this->ip_01_lastIndex = 0;
    this->ip_01_setupDone = true;
}

void param_01_getPresetValue(PatcherStateInterface& preset) {
    preset["value"] = this->param_01_value;
}

void param_01_setPresetValue(PatcherStateInterface& preset) {
    if ((bool)(stateIsEmpty(preset)))
        return;

    this->param_01_value_set(preset["value"]);
}

void phasor_01_onBeatTimeChanged(number beattime) {
    RNBO_UNUSED(beattime);
    this->phasor_01_recalcInc = true;
    this->phasor_01_recalcPhase = true;
}

number phasor_01_ph_next(number freq, number reset) {
    RNBO_UNUSED(reset);
    number pincr = freq * this->phasor_01_ph_conv;

    if (this->phasor_01_ph_currentPhase < 0.)
        this->phasor_01_ph_currentPhase = 1. + this->phasor_01_ph_currentPhase;

    if (this->phasor_01_ph_currentPhase > 1.)
        this->phasor_01_ph_currentPhase = this->phasor_01_ph_currentPhase - 1.;

    number tmp = this->phasor_01_ph_currentPhase;
    this->phasor_01_ph_currentPhase += pincr;
    return tmp;
}

void phasor_01_ph_reset() {
    this->phasor_01_ph_currentPhase = 0;
}

void phasor_01_ph_dspsetup() {
    this->phasor_01_ph_conv = (number)1 / this->sr;
}

void phasor_01_dspsetup(bool force) {
    if ((bool)(this->phasor_01_setupDone) && (bool)(!(bool)(force)))
        return;

    this->phasor_01_setupDone = true;
    this->phasor_01_ph_dspsetup();
}

void param_02_getPresetValue(PatcherStateInterface& preset) {
    preset["value"] = this->param_02_value;
}

void param_02_setPresetValue(PatcherStateInterface& preset) {
    if ((bool)(stateIsEmpty(preset)))
        return;

    this->param_02_value_set(preset["value"]);
}

number cycle_tilde_01_ph_next(number freq, number reset) {
    {
        {
            if (reset >= 0.)
                this->cycle_tilde_01_ph_currentPhase = reset;
        }
    }

    number pincr = freq * this->cycle_tilde_01_ph_conv;

    if (this->cycle_tilde_01_ph_currentPhase < 0.)
        this->cycle_tilde_01_ph_currentPhase = 1. + this->cycle_tilde_01_ph_currentPhase;

    if (this->cycle_tilde_01_ph_currentPhase > 1.)
        this->cycle_tilde_01_ph_currentPhase = this->cycle_tilde_01_ph_currentPhase - 1.;

    number tmp = this->cycle_tilde_01_ph_currentPhase;
    this->cycle_tilde_01_ph_currentPhase += pincr;
    return tmp;
}

void cycle_tilde_01_ph_reset() {
    this->cycle_tilde_01_ph_currentPhase = 0;
}

void cycle_tilde_01_ph_dspsetup() {
    this->cycle_tilde_01_ph_conv = (number)1 / this->sr;
}

void cycle_tilde_01_dspsetup(bool force) {
    if ((bool)(this->cycle_tilde_01_setupDone) && (bool)(!(bool)(force)))
        return;

    this->cycle_tilde_01_phasei = 0;
    this->cycle_tilde_01_f2i = (number)4294967296 / this->sr;
    this->cycle_tilde_01_wrap = (Int)(this->cycle_tilde_01_buffer->getSize()) - 1;
    this->cycle_tilde_01_setupDone = true;
    this->cycle_tilde_01_ph_dspsetup();
}

void cycle_tilde_01_bufferUpdated() {
    this->cycle_tilde_01_wrap = (Int)(this->cycle_tilde_01_buffer->getSize()) - 1;
}

void latch_tilde_01_init() {
    this->latch_tilde_01_reset();
}

void latch_tilde_01_reset() {
    this->latch_tilde_01_value = 0;
}

void latch_tilde_01_dspsetup(bool force) {
    if ((bool)(this->latch_tilde_01_setupDone) && (bool)(!(bool)(force)))
        return;

    this->latch_tilde_01_reset();
    this->latch_tilde_01_setupDone = true;
}

void noise_tilde_01_nz_reset() {
    xoshiro_reset(
        systemticks() + this->voice() + this->random(0, 10000),
        this->noise_tilde_01_nz_state
    );
}

void noise_tilde_01_nz_init() {
    this->noise_tilde_01_nz_reset();
}

void noise_tilde_01_nz_seed(number v) {
    xoshiro_reset(v, this->noise_tilde_01_nz_state);
}

number noise_tilde_01_nz_next() {
    return xoshiro_next(this->noise_tilde_01_nz_state);
}

void noise_tilde_01_dspsetup(bool force) {
    if ((bool)(this->noise_tilde_01_setupDone) && (bool)(!(bool)(force)))
        return;

    this->noise_tilde_01_setupDone = true;
}

void delta_tilde_01_reset() {
    this->delta_tilde_01_prev = 0;
}

void delta_tilde_01_dspsetup(bool force) {
    if ((bool)(this->delta_tilde_01_setupDone) && (bool)(!(bool)(force)))
        return;

    this->delta_tilde_01_reset();
    this->delta_tilde_01_setupDone = true;
}

void latch_tilde_02_init() {
    this->latch_tilde_02_reset();
}

void latch_tilde_02_reset() {
    this->latch_tilde_02_value = 0;
}

void latch_tilde_02_dspsetup(bool force) {
    if ((bool)(this->latch_tilde_02_setupDone) && (bool)(!(bool)(force)))
        return;

    this->latch_tilde_02_reset();
    this->latch_tilde_02_setupDone = true;
}

void noise_tilde_02_nz_reset() {
    xoshiro_reset(
        systemticks() + this->voice() + this->random(0, 10000),
        this->noise_tilde_02_nz_state
    );
}

void noise_tilde_02_nz_init() {
    this->noise_tilde_02_nz_reset();
}

void noise_tilde_02_nz_seed(number v) {
    xoshiro_reset(v, this->noise_tilde_02_nz_state);
}

number noise_tilde_02_nz_next() {
    return xoshiro_next(this->noise_tilde_02_nz_state);
}

void noise_tilde_02_dspsetup(bool force) {
    if ((bool)(this->noise_tilde_02_setupDone) && (bool)(!(bool)(force)))
        return;

    this->noise_tilde_02_setupDone = true;
}

void ip_02_init() {
    this->ip_02_lastValue = this->ip_02_value;
}

void ip_02_fillSigBuf() {
    if ((bool)(this->ip_02_sigbuf)) {
        SampleIndex k = (SampleIndex)(this->sampleOffsetIntoNextAudioBuffer);

        if (k >= (SampleIndex)(this->vs))
            k = (SampleIndex)(this->vs) - 1;

        for (SampleIndex i = (SampleIndex)(this->ip_02_lastIndex); i < k; i++) {
            if (this->ip_02_resetCount > 0) {
                this->ip_02_sigbuf[(Index)i] = 1;
                this->ip_02_resetCount--;
            } else {
                this->ip_02_sigbuf[(Index)i] = this->ip_02_lastValue;
            }
        }

        this->ip_02_lastIndex = k;
    }
}

void ip_02_dspsetup(bool force) {
    if ((bool)(this->ip_02_setupDone) && (bool)(!(bool)(force)))
        return;

    this->ip_02_lastIndex = 0;
    this->ip_02_setupDone = true;
}

void param_03_getPresetValue(PatcherStateInterface& preset) {
    preset["value"] = this->param_03_value;
}

void param_03_setPresetValue(PatcherStateInterface& preset) {
    if ((bool)(stateIsEmpty(preset)))
        return;

    this->param_03_value_set(preset["value"]);
}

void delta_tilde_02_reset() {
    this->delta_tilde_02_prev = 0;
}

void delta_tilde_02_dspsetup(bool force) {
    if ((bool)(this->delta_tilde_02_setupDone) && (bool)(!(bool)(force)))
        return;

    this->delta_tilde_02_reset();
    this->delta_tilde_02_setupDone = true;
}

number globaltransport_getSampleOffset(MillisecondTime time) {
    return this->mstosamps(this->maximum(0, time - this->getEngine()->getCurrentTime()));
}

number globaltransport_getTempoAtSample(SampleIndex sampleOffset) {
    return (sampleOffset >= 0 && sampleOffset < (SampleIndex)(this->vs) ? this->globaltransport_tempo[(Index)sampleOffset] : this->globaltransport_lastTempo);
}

number globaltransport_getStateAtSample(SampleIndex sampleOffset) {
    return (sampleOffset >= 0 && sampleOffset < (SampleIndex)(this->vs) ? this->globaltransport_state[(Index)sampleOffset] : this->globaltransport_lastState);
}

number globaltransport_getState(MillisecondTime time) {
    return this->globaltransport_getStateAtSample((SampleIndex)(this->globaltransport_getSampleOffset(time)));
}

number globaltransport_getTempo(MillisecondTime time) {
    return this->globaltransport_getTempoAtSample((SampleIndex)(this->globaltransport_getSampleOffset(time)));
}

number globaltransport_getBeatTime(MillisecondTime time) {
    number i = 2;

    while (i < this->globaltransport_beatTimeChanges->length && this->globaltransport_beatTimeChanges[(Index)(i + 1)] <= time) {
        i += 2;
    }

    i -= 2;
    number beatTimeBase = this->globaltransport_beatTimeChanges[(Index)i];

    if (this->globaltransport_getState(time) == 0)
        return beatTimeBase;

    number beatTimeBaseMsTime = this->globaltransport_beatTimeChanges[(Index)(i + 1)];
    number diff = time - beatTimeBaseMsTime;
    number diffInBeats = diff * this->globaltransport_getTempo(time) * 0.008 / (number)480;
    return beatTimeBase + diffInBeats;
}

bool globaltransport_setTempo(MillisecondTime time, number tempo, bool notify) {
    if ((bool)(notify)) {
        this->processTempoEvent(time, tempo);
        this->globaltransport_notify = true;
    } else {
        Index offset = (Index)(this->globaltransport_getSampleOffset(time));

        if (this->globaltransport_getTempoAtSample((SampleIndex)(offset)) != tempo) {
            this->globaltransport_beatTimeChanges->push(this->globaltransport_getBeatTime(time));
            this->globaltransport_beatTimeChanges->push(time);
            fillSignal(this->globaltransport_tempo, this->vs, tempo, offset);
            this->globaltransport_lastTempo = tempo;
            this->globaltransport_tempoNeedsReset = true;
            return true;
        }
    }

    return false;
}

bool globaltransport_setState(MillisecondTime time, number state, bool notify) {
    if ((bool)(notify)) {
        this->processTransportEvent(time, TransportState(state));
        this->globaltransport_notify = true;
    } else {
        SampleIndex offset = (SampleIndex)(this->globaltransport_getSampleOffset(time));

        if (this->globaltransport_getStateAtSample(offset) != state) {
            this->globaltransport_beatTimeChanges->push(this->globaltransport_getBeatTime(time));
            this->globaltransport_beatTimeChanges->push(time);
            fillSignal(this->globaltransport_state, this->vs, state, (Index)(offset));
            this->globaltransport_lastState = TransportState(state);
            this->globaltransport_stateNeedsReset = true;
            return true;
        }
    }

    return false;
}

bool globaltransport_setBeatTime(MillisecondTime time, number beattime, bool notify) {
    if ((bool)(notify)) {
        this->processBeatTimeEvent(time, beattime);
        this->globaltransport_notify = true;
        return false;
    } else {
        bool beatTimeHasChanged = false;

        if (rnbo_abs(beattime - this->globaltransport_getBeatTime(time)) > 0.001) {
            beatTimeHasChanged = true;
        }

        this->globaltransport_beatTimeChanges->push(beattime);
        this->globaltransport_beatTimeChanges->push(time);
        return beatTimeHasChanged;
    }
}

number globaltransport_getBeatTimeAtSample(SampleIndex sampleOffset) {
    auto msOffset = this->sampstoms(sampleOffset);
    return this->globaltransport_getBeatTime(this->getEngine()->getCurrentTime() + msOffset);
}

array<number, 2> globaltransport_getTimeSignature(MillisecondTime time) {
    number i = 3;

    while (i < this->globaltransport_timeSignatureChanges->length && this->globaltransport_timeSignatureChanges[(Index)(i + 2)] <= time) {
        i += 3;
    }

    i -= 3;

    return {
        this->globaltransport_timeSignatureChanges[(Index)i],
        this->globaltransport_timeSignatureChanges[(Index)(i + 1)]
    };
}

array<number, 2> globaltransport_getTimeSignatureAtSample(SampleIndex sampleOffset) {
    MillisecondTime msOffset = (MillisecondTime)(this->sampstoms(sampleOffset));
    return this->globaltransport_getTimeSignature(this->getEngine()->getCurrentTime() + msOffset);
}

void globaltransport_setBBUBase(
    MillisecondTime time,
    number numerator,
    number denominator,
    number bars,
    number beats,
    number units
) {
    number beatsInQuarterNotes = this->globaltransport_getBeatTime(time);
    bars--;
    beats--;
    number beatsIncCurrenttDenom = beatsInQuarterNotes * (denominator * 0.25);
    number beatLength = (number)4 / denominator;
    number beatLengthInUnits = beatLength * 480;

    while (units > beatLengthInUnits) {
        units -= beatLengthInUnits;
        beats++;
    }

    number targetBeatTime = bars * numerator + beats + units / beatLengthInUnits;
    this->globaltransport_bbuBase = targetBeatTime - beatsIncCurrenttDenom;
}

array<number, 3> globaltransport_getBBU(MillisecondTime time) {
    array<number, 2> currentSig = this->globaltransport_getTimeSignature(time);
    number numerator = currentSig[0];
    number denominator = currentSig[1];
    number beatsInQuarterNotes = this->globaltransport_getBeatTime(time);
    number beatsIncCurrenttDenom = beatsInQuarterNotes * (denominator * 0.25);
    number beatLength = (number)4 / denominator;
    number beatLengthInUnits = beatLength * 480;
    number targetBeatTime = beatsIncCurrenttDenom + this->globaltransport_bbuBase;
    number currentBars = 0;
    number currentBeats = 0;
    number currentUnits = 0;

    if (targetBeatTime >= 0) {
        currentBars = trunc(targetBeatTime / numerator);
        targetBeatTime -= currentBars * numerator;
        currentBeats = trunc(targetBeatTime);
        targetBeatTime -= currentBeats;
        currentUnits = targetBeatTime * beatLengthInUnits;
    } else {
        currentBars = trunc(targetBeatTime / numerator);
        targetBeatTime -= currentBars * numerator;

        if (targetBeatTime != 0) {
            currentBars -= 1;
            currentBeats = trunc(targetBeatTime);
            targetBeatTime -= currentBeats;
            currentBeats = numerator + currentBeats;
            currentUnits = targetBeatTime * beatLengthInUnits;

            if (currentUnits != 0) {
                currentUnits = beatLengthInUnits + currentUnits;
                currentBeats -= 1;
            }
        }
    }

    return {currentBars + 1, currentBeats + 1, currentUnits};
}

bool globaltransport_setTimeSignature(MillisecondTime time, number numerator, number denominator, bool notify) {
    if ((bool)(notify)) {
        this->processTimeSignatureEvent(time, (Int)(numerator), (Int)(denominator));
        this->globaltransport_notify = true;
    } else {
        array<number, 2> currentSig = this->globaltransport_getTimeSignature(time);

        if (currentSig[0] != numerator || currentSig[1] != denominator) {
            array<number, 3> bbu = this->globaltransport_getBBU(time);
            this->globaltransport_setBBUBase(time, numerator, denominator, bbu[0], bbu[1], bbu[2]);
            this->globaltransport_timeSignatureChanges->push(numerator);
            this->globaltransport_timeSignatureChanges->push(denominator);
            this->globaltransport_timeSignatureChanges->push(time);
            return true;
        }
    }

    return false;
}

array<number, 3> globaltransport_getBBUAtSample(SampleIndex sampleOffset) {
    auto msOffset = this->sampstoms(sampleOffset);
    return this->globaltransport_getBBU(this->getEngine()->getCurrentTime() + msOffset);
}

bool globaltransport_setBBU(MillisecondTime time, number bars, number beats, number units, bool notify) {
    RNBO_UNUSED(notify);
    array<number, 2> sig = this->globaltransport_getTimeSignature(time);
    number numerator = sig[0];
    number denominator = sig[1];
    this->globaltransport_setBBUBase(time, numerator, denominator, bars, beats, units);
    return true;
}

void globaltransport_advance() {
    if ((bool)(this->globaltransport_tempoNeedsReset)) {
        fillSignal(this->globaltransport_tempo, this->vs, this->globaltransport_lastTempo);
        this->globaltransport_tempoNeedsReset = false;

        if ((bool)(this->globaltransport_notify)) {
            this->getEngine()->sendTempoEvent(this->globaltransport_lastTempo);
        }
    }

    if ((bool)(this->globaltransport_stateNeedsReset)) {
        fillSignal(this->globaltransport_state, this->vs, this->globaltransport_lastState);
        this->globaltransport_stateNeedsReset = false;

        if ((bool)(this->globaltransport_notify)) {
            this->getEngine()->sendTransportEvent(TransportState(this->globaltransport_lastState));
        }
    }

    if (this->globaltransport_beatTimeChanges->length > 2) {
        this->globaltransport_beatTimeChanges[0] = this->globaltransport_beatTimeChanges[(Index)(this->globaltransport_beatTimeChanges->length - 2)];
        this->globaltransport_beatTimeChanges[1] = this->globaltransport_beatTimeChanges[(Index)(this->globaltransport_beatTimeChanges->length - 1)];
        this->globaltransport_beatTimeChanges->length = 2;

        if ((bool)(this->globaltransport_notify)) {
            this->getEngine()->sendBeatTimeEvent(this->globaltransport_beatTimeChanges[0]);
        }
    }

    if (this->globaltransport_timeSignatureChanges->length > 3) {
        this->globaltransport_timeSignatureChanges[0] = this->globaltransport_timeSignatureChanges[(Index)(this->globaltransport_timeSignatureChanges->length - 3)];
        this->globaltransport_timeSignatureChanges[1] = this->globaltransport_timeSignatureChanges[(Index)(this->globaltransport_timeSignatureChanges->length - 2)];
        this->globaltransport_timeSignatureChanges[2] = this->globaltransport_timeSignatureChanges[(Index)(this->globaltransport_timeSignatureChanges->length - 1)];
        this->globaltransport_timeSignatureChanges->length = 3;

        if ((bool)(this->globaltransport_notify)) {
            this->getEngine()->sendTimeSignatureEvent(
                (Int)(this->globaltransport_timeSignatureChanges[0]),
                (Int)(this->globaltransport_timeSignatureChanges[1])
            );
        }
    }

    this->globaltransport_notify = false;
}

void globaltransport_dspsetup(bool force) {
    if ((bool)(this->globaltransport_setupDone) && (bool)(!(bool)(force)))
        return;

    fillSignal(this->globaltransport_tempo, this->vs, this->globaltransport_lastTempo);
    this->globaltransport_tempoNeedsReset = false;
    fillSignal(this->globaltransport_state, this->vs, this->globaltransport_lastState);
    this->globaltransport_stateNeedsReset = false;
    this->globaltransport_setupDone = true;
}

bool stackprotect_check() {
    this->stackprotect_count++;

    if (this->stackprotect_count > 128) {
        console->log("STACK OVERFLOW DETECTED - stopped processing branch !");
        return true;
    }

    return false;
}

Index getPatcherSerial() const {
    return 0;
}

void sendParameter(ParameterIndex index, bool ignoreValue) {
    this->getEngine()->notifyParameterValueChanged(index, (ignoreValue ? 0 : this->getParameterValue(index)), ignoreValue);
}

void scheduleParamInit(ParameterIndex index, Index order) {
    this->paramInitIndices->push(index);
    this->paramInitOrder->push(order);
}

void processParamInitEvents() {
    this->listquicksort(
        this->paramInitOrder,
        this->paramInitIndices,
        0,
        (int)(this->paramInitOrder->length - 1),
        true
    );

    for (Index i = 0; i < this->paramInitOrder->length; i++) {
        this->getEngine()->scheduleParameterBang(this->paramInitIndices[i], 0);
    }
}

void updateTime(MillisecondTime time, EXTERNALENGINE* engine, bool inProcess = false) {
    RNBO_UNUSED(inProcess);
    RNBO_UNUSED(engine);
    this->_currentTime = time;
    auto offset = rnbo_fround(this->msToSamps(time - this->getEngine()->getCurrentTime(), this->sr));

    if (offset >= (SampleIndex)(this->vs))
        offset = (SampleIndex)(this->vs) - 1;

    if (offset < 0)
        offset = 0;

    this->sampleOffsetIntoNextAudioBuffer = (Index)(offset);
}

void assign_defaults()
{
    ip_01_value = 0;
    ip_01_impulse = 0;
    dspexpr_01_in1 = 0;
    dspexpr_01_in2 = 1;
    param_01_value = 0;
    selector_01_onoff = 1;
    phasor_01_freq = 10;
    param_02_value = 0;
    dspexpr_02_in1 = 0;
    dspexpr_02_in2 = 0;
    cycle_tilde_01_frequency = 0;
    cycle_tilde_01_phase_offset = 0;
    dspexpr_03_in1 = 0;
    dspexpr_03_in2 = 0.75;
    dspexpr_04_in1 = 0;
    dspexpr_04_in2 = 1;
    scale_tilde_01_x = 0;
    scale_tilde_01_lowin = 0;
    scale_tilde_01_hiin = 1;
    scale_tilde_01_lowout = -1;
    scale_tilde_01_highout = 1;
    scale_tilde_01_pow = 1;
    dspexpr_05_in1 = 0;
    dspexpr_05_in2 = 0;
    scale_tilde_02_x = 0;
    scale_tilde_02_lowin = 0;
    scale_tilde_02_hiin = 1;
    scale_tilde_02_lowout = 1;
    scale_tilde_02_highout = -1;
    scale_tilde_02_pow = 1;
    dspexpr_06_in1 = 0;
    dspexpr_06_in2 = 0;
    dspexpr_07_in1 = 0;
    dspexpr_07_in2 = 0;
    dspexpr_08_in1 = 0;
    dspexpr_08_in2 = 1;
    dspexpr_09_in1 = 0;
    dspexpr_09_in2 = 2;
    dspexpr_10_in1 = 0;
    dspexpr_10_in2 = 0.5;
    triangle_tilde_01_phase = 0;
    triangle_tilde_01_duty = 0.5;
    dspexpr_11_in1 = 0;
    dspexpr_11_in2 = 0;
    latch_tilde_01_x = 0;
    latch_tilde_01_control = 0;
    noise_tilde_01_seed = 0;
    dspexpr_12_in1 = 0;
    dspexpr_12_in2 = 0;
    delta_tilde_01_x = 0;
    dspexpr_13_in1 = 0;
    dspexpr_13_in2 = 0;
    dspexpr_14_in1 = 0;
    dspexpr_14_in2 = 1;
    latch_tilde_02_x = 0;
    latch_tilde_02_control = 0;
    noise_tilde_02_seed = 0;
    ip_02_value = 0;
    ip_02_impulse = 0;
    param_03_value = 1;
    delta_tilde_02_x = 0;
    dspexpr_15_in1 = 0;
    dspexpr_15_in2 = 0;
    _currentTime = 0;
    audioProcessSampleCount = 0;
    sampleOffsetIntoNextAudioBuffer = 0;
    zeroBuffer = nullptr;
    dummyBuffer = nullptr;
    signals[0] = nullptr;
    signals[1] = nullptr;
    signals[2] = nullptr;
    signals[3] = nullptr;
    signals[4] = nullptr;
    signals[5] = nullptr;
    signals[6] = nullptr;
    signals[7] = nullptr;
    signals[8] = nullptr;
    signals[9] = nullptr;
    signals[10] = nullptr;
    didAllocateSignals = 0;
    vs = 0;
    maxvs = 0;
    sr = 44100;
    invsr = 0.000022675736961451248;
    ip_01_lastIndex = 0;
    ip_01_lastValue = 0;
    ip_01_resetCount = 0;
    ip_01_sigbuf = nullptr;
    ip_01_setupDone = false;
    param_01_lastValue = 0;
    phasor_01_sigbuf = nullptr;
    phasor_01_lastLockedPhase = 0;
    phasor_01_lastQuantum = 0;
    phasor_01_lastTempo = 0;
    phasor_01_nextJumpInSamples = 0;
    phasor_01_inc = 0;
    phasor_01_recalcInc = true;
    phasor_01_recalcPhase = true;
    phasor_01_ph_currentPhase = 0;
    phasor_01_ph_conv = 0;
    phasor_01_setupDone = false;
    param_02_lastValue = 0;
    cycle_tilde_01_wrap = 0;
    cycle_tilde_01_ph_currentPhase = 0;
    cycle_tilde_01_ph_conv = 0;
    cycle_tilde_01_setupDone = false;
    latch_tilde_01_value = 0;
    latch_tilde_01_setupDone = false;
    noise_tilde_01_setupDone = false;
    delta_tilde_01_prev = 0;
    delta_tilde_01_setupDone = false;
    latch_tilde_02_value = 0;
    latch_tilde_02_setupDone = false;
    noise_tilde_02_setupDone = false;
    ip_02_lastIndex = 0;
    ip_02_lastValue = 0;
    ip_02_resetCount = 0;
    ip_02_sigbuf = nullptr;
    ip_02_setupDone = false;
    param_03_lastValue = 0;
    delta_tilde_02_prev = 0;
    delta_tilde_02_setupDone = false;
    globaltransport_tempo = nullptr;
    globaltransport_tempoNeedsReset = false;
    globaltransport_lastTempo = 120;
    globaltransport_state = nullptr;
    globaltransport_stateNeedsReset = false;
    globaltransport_lastState = 0;
    globaltransport_beatTimeChanges = { 0, 0 };
    globaltransport_timeSignatureChanges = { 4, 4, 0 };
    globaltransport_notify = false;
    globaltransport_bbuBase = 0;
    globaltransport_setupDone = false;
    stackprotect_count = 0;
    _voiceIndex = 0;
    _noteNumber = 0;
    isMuted = 1;
}

    // data ref strings
    struct DataRefStrings {
    	static constexpr auto& name0 = "RNBODefaultSinus";
    	static constexpr auto& file0 = "";
    	static constexpr auto& tag0 = "buffer~";
    	DataRefStrings* operator->() { return this; }
    	const DataRefStrings* operator->() const { return this; }
    };

    DataRefStrings dataRefStrings;

// member variables

    number ip_01_value;
    number ip_01_impulse;
    number dspexpr_01_in1;
    number dspexpr_01_in2;
    number param_01_value;
    number selector_01_onoff;
    number phasor_01_freq;
    number param_02_value;
    number dspexpr_02_in1;
    number dspexpr_02_in2;
    number cycle_tilde_01_frequency;
    number cycle_tilde_01_phase_offset;
    number dspexpr_03_in1;
    number dspexpr_03_in2;
    number dspexpr_04_in1;
    number dspexpr_04_in2;
    number scale_tilde_01_x;
    number scale_tilde_01_lowin;
    number scale_tilde_01_hiin;
    number scale_tilde_01_lowout;
    number scale_tilde_01_highout;
    number scale_tilde_01_pow;
    number dspexpr_05_in1;
    number dspexpr_05_in2;
    number scale_tilde_02_x;
    number scale_tilde_02_lowin;
    number scale_tilde_02_hiin;
    number scale_tilde_02_lowout;
    number scale_tilde_02_highout;
    number scale_tilde_02_pow;
    number dspexpr_06_in1;
    number dspexpr_06_in2;
    number dspexpr_07_in1;
    number dspexpr_07_in2;
    number dspexpr_08_in1;
    number dspexpr_08_in2;
    number dspexpr_09_in1;
    number dspexpr_09_in2;
    number dspexpr_10_in1;
    number dspexpr_10_in2;
    number triangle_tilde_01_phase;
    number triangle_tilde_01_duty;
    number dspexpr_11_in1;
    number dspexpr_11_in2;
    number latch_tilde_01_x;
    number latch_tilde_01_control;
    number noise_tilde_01_seed;
    number dspexpr_12_in1;
    number dspexpr_12_in2;
    number delta_tilde_01_x;
    number dspexpr_13_in1;
    number dspexpr_13_in2;
    number dspexpr_14_in1;
    number dspexpr_14_in2;
    number latch_tilde_02_x;
    number latch_tilde_02_control;
    number noise_tilde_02_seed;
    number ip_02_value;
    number ip_02_impulse;
    number param_03_value;
    number delta_tilde_02_x;
    number dspexpr_15_in1;
    number dspexpr_15_in2;
    MillisecondTime _currentTime;
    ENGINE _internalEngine;
    UInt64 audioProcessSampleCount;
    Index sampleOffsetIntoNextAudioBuffer;
    signal zeroBuffer;
    signal dummyBuffer;
    SampleValue * signals[11];
    bool didAllocateSignals;
    Index vs;
    Index maxvs;
    number sr;
    number invsr;
    SampleIndex ip_01_lastIndex;
    number ip_01_lastValue;
    SampleIndex ip_01_resetCount;
    signal ip_01_sigbuf;
    bool ip_01_setupDone;
    number param_01_lastValue;
    signal phasor_01_sigbuf;
    number phasor_01_lastLockedPhase;
    number phasor_01_lastQuantum;
    number phasor_01_lastTempo;
    number phasor_01_nextJumpInSamples;
    number phasor_01_inc;
    bool phasor_01_recalcInc;
    bool phasor_01_recalcPhase;
    number phasor_01_ph_currentPhase;
    number phasor_01_ph_conv;
    bool phasor_01_setupDone;
    number param_02_lastValue;
    SampleBufferRef cycle_tilde_01_buffer;
    Int cycle_tilde_01_wrap;
    UInt32 cycle_tilde_01_phasei;
    SampleValue cycle_tilde_01_f2i;
    number cycle_tilde_01_ph_currentPhase;
    number cycle_tilde_01_ph_conv;
    bool cycle_tilde_01_setupDone;
    number latch_tilde_01_value;
    bool latch_tilde_01_setupDone;
    UInt noise_tilde_01_nz_state[4] = { };
    bool noise_tilde_01_setupDone;
    number delta_tilde_01_prev;
    bool delta_tilde_01_setupDone;
    number latch_tilde_02_value;
    bool latch_tilde_02_setupDone;
    UInt noise_tilde_02_nz_state[4] = { };
    bool noise_tilde_02_setupDone;
    SampleIndex ip_02_lastIndex;
    number ip_02_lastValue;
    SampleIndex ip_02_resetCount;
    signal ip_02_sigbuf;
    bool ip_02_setupDone;
    number param_03_lastValue;
    number delta_tilde_02_prev;
    bool delta_tilde_02_setupDone;
    signal globaltransport_tempo;
    bool globaltransport_tempoNeedsReset;
    number globaltransport_lastTempo;
    signal globaltransport_state;
    bool globaltransport_stateNeedsReset;
    number globaltransport_lastState;
    list globaltransport_beatTimeChanges;
    list globaltransport_timeSignatureChanges;
    bool globaltransport_notify;
    number globaltransport_bbuBase;
    bool globaltransport_setupDone;
    number stackprotect_count;
    DataRef RNBODefaultSinus;
    Index _voiceIndex;
    Int _noteNumber;
    Index isMuted;
    indexlist paramInitIndices;
    indexlist paramInitOrder;
    bool _isInitialized = false;
};

static PatcherInterface* creaternbomatic()
{
    return new rnbomatic<EXTERNALENGINE>();
}

#ifndef RNBO_NO_PATCHERFACTORY
extern "C" PatcherFactoryFunctionPtr GetPatcherFactoryFunction()
#else
extern "C" PatcherFactoryFunctionPtr rnbomaticFactoryFunction()
#endif
{
    return creaternbomatic;
}

#ifndef RNBO_NO_PATCHERFACTORY
extern "C" void SetLogger(Logger* logger)
#else
void rnbomaticSetLogger(Logger* logger)
#endif
{
    console = logger;
}

} // end RNBO namespace

