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

#include RNBO_LIB_INCLUDE(RNBO_Common.h)
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
    this->vs = n;
    this->updateTime(this->getEngine()->getCurrentTime(), (ENGINE*)nullptr, true);
    SampleValue * out1 = (numOutputs >= 1 && outputs[0] ? outputs[0] : this->dummyBuffer);
    SampleValue * out2 = (numOutputs >= 2 && outputs[1] ? outputs[1] : this->dummyBuffer);
    const SampleValue * in1 = (numInputs >= 1 && inputs[0] ? inputs[0] : this->zeroBuffer);
    const SampleValue * in2 = (numInputs >= 2 && inputs[1] ? inputs[1] : this->zeroBuffer);
    this->phasor_01_perform(this->phasor_01_freq, this->signals[0], n);
    this->beatstosamps_tilde_01_perform(this->beatstosamps_tilde_01_beattime, this->signals[1], n);

    this->gen_01_perform(
        in1,
        this->signals[0],
        this->signals[1],
        this->gen_01_in4,
        this->gen_01_in5,
        out1,
        n
    );

    this->gen_02_perform(
        in2,
        this->signals[0],
        this->signals[1],
        this->gen_02_in4,
        this->gen_02_in5,
        out2,
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

        for (i = 0; i < 2; i++) {
            this->signals[i] = resizeSignal(this->signals[i], this->maxvs, maxBlockSize);
        }

        this->phasor_01_sigbuf = resizeSignal(this->phasor_01_sigbuf, this->maxvs, maxBlockSize);
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

    this->gen_01_dspsetup(forceDSPSetup);
    this->gen_02_dspsetup(forceDSPSetup);
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
    return 2;
}

Index getNumOutputChannels() const {
    return 2;
}

DataRef* getDataRef(DataRefIndex index)  {
    switch (index) {
    case 0:
        {
        return addressOf(this->repeat);
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
        this->gen_01_repeat = reInitDataView(this->gen_01_repeat, this->repeat);
        this->gen_01_repeat_exprdata_buffer = reInitDataView(this->gen_01_repeat_exprdata_buffer, this->repeat);
        this->gen_02_repeat = reInitDataView(this->gen_02_repeat, this->repeat);
        this->gen_02_repeat_exprdata_buffer = reInitDataView(this->gen_02_repeat_exprdata_buffer, this->repeat);
    }
}

void initialize() {
    RNBO_ASSERT(!this->_isInitialized);

    this->repeat = initDataRef(
        this->repeat,
        this->dataRefStrings->name0,
        true,
        this->dataRefStrings->file0,
        this->dataRefStrings->tag0
    );

    this->assign_defaults();
    this->applyState();
    this->repeat->setIndex(0);
    this->gen_01_repeat = new SampleBuffer(this->repeat);
    this->gen_01_repeat_exprdata_buffer = new Float64Buffer(this->repeat);
    this->gen_02_repeat = new SampleBuffer(this->repeat);
    this->gen_02_repeat_exprdata_buffer = new Float64Buffer(this->repeat);
    this->initializeObjects();
    this->allocateDataRefs();
    this->startup();
    this->_isInitialized = true;
}

void processTempoEvent(MillisecondTime time, Tempo tempo) {
    this->updateTime(time, (ENGINE*)nullptr);

    if (this->globaltransport_setTempo(this->_currentTime, tempo, false)) {
        this->transport_01_onTempoChanged(tempo);
        this->timevalue_01_onTempoChanged(tempo);
        this->timevalue_02_onTempoChanged(tempo);
    }
}

void processTransportEvent(MillisecondTime time, TransportState state) {
    this->updateTime(time, (ENGINE*)nullptr);

    if (this->globaltransport_setState(this->_currentTime, state, false)) {
        this->transport_01_onTransportChanged(state);
        this->metro_01_onTransportChanged(state);
    }
}

void processBeatTimeEvent(MillisecondTime time, BeatTime beattime) {
    this->updateTime(time, (ENGINE*)nullptr);

    if (this->globaltransport_setBeatTime(this->_currentTime, beattime, false)) {
        this->phasor_01_onBeatTimeChanged(beattime);
        this->metro_01_onBeatTimeChanged(beattime);
    }
}

void processTimeSignatureEvent(MillisecondTime time, Int numerator, Int denominator) {
    this->updateTime(time, (ENGINE*)nullptr);

    if (this->globaltransport_setTimeSignature(this->_currentTime, numerator, denominator, false)) {
        this->transport_01_onTimeSignatureChanged(numerator, denominator);
        this->timevalue_01_onTimeSignatureChanged(numerator, denominator);
        this->timevalue_02_onTimeSignatureChanged(numerator, denominator);
    }
}

void processBBUEvent(MillisecondTime time, number bars, number beats, number units) {
    this->updateTime(time, (ENGINE*)nullptr);

    if (this->globaltransport_setBBU(this->_currentTime, bars, beats, units, false))
        {}
}

void getPreset(PatcherStateInterface& preset) {
    this->updateTime(this->getEngine()->getCurrentTime(), (ENGINE*)nullptr);
    preset["__presetid"] = "rnbo";
    this->param_01_getPresetValue(getSubState(preset, "repeatlength"));
    this->param_02_getPresetValue(getSubState(preset, "offset"));
}

void setPreset(MillisecondTime time, PatcherStateInterface& preset) {
    this->updateTime(time, (ENGINE*)nullptr);
    this->param_01_setPresetValue(getSubState(preset, "repeatlength"));
    this->param_02_setPresetValue(getSubState(preset, "offset"));
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
    return 2;
}

ConstCharPointer getParameterName(ParameterIndex index) const {
    switch (index) {
    case 0:
        {
        return "repeatlength";
        }
    case 1:
        {
        return "offset";
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
        return "repeatlength";
        }
    case 1:
        {
        return "offset";
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
        case 1:
            {
            info->type = ParameterTypeNumber;
            info->initialValue = 0;
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
    case 0:
    case 1:
        {
        {
            value = (value < 0 ? 0 : (value > 1 ? 1 : value));
            ParameterValue normalizedValue = (value - 0) / (1 - 0);
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
    case 0:
    case 1:
        {
        {
            {
                return 0 + value * (1 - 0);
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
    default:
        {
        return value;
        }
    }
}

void processNumMessage(MessageTag , MessageTag , MillisecondTime , number ) {}

void processListMessage(MessageTag , MessageTag , MillisecondTime , const list& ) {}

void processBangMessage(MessageTag tag, MessageTag objectId, MillisecondTime time) {
    this->updateTime(time, (ENGINE*)nullptr);

    switch (tag) {
    case TAG("startupbang"):
        {
        if (TAG("loadbang_obj-14") == objectId)
            this->loadbang_01_startupbang_bang();

        break;
        }
    }
}

MessageTagInfo resolveTag(MessageTag tag) const {
    switch (tag) {
    case TAG("startupbang"):
        {
        return "startupbang";
        }
    case TAG("loadbang_obj-14"):
        {
        return "loadbang_obj-14";
        }
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
    getEngine()->flushClockEvents(this, -871642103, false);
    getEngine()->flushClockEvents(this, 1935387534, false);
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

template<typename BUFFERTYPE> number dim(BUFFERTYPE& buffer) {
    return buffer->getSize();
}

template<typename BUFFERTYPE> number channels(BUFFERTYPE& buffer) {
    return buffer->getChannels();
}

inline number intnum(const number value) {
    return trunc(value);
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

template<typename BUFFERTYPE> void poke_default(
    BUFFERTYPE& buffer,
    SampleValue value,
    SampleValue sampleIndex,
    Int channel,
    number overdub
) {
    number bufferSize = buffer->getSize();
    const Index bufferChannels = (const Index)(buffer->getChannels());

    if (bufferSize > 0 && (5 != 5 || (sampleIndex >= 0 && sampleIndex < bufferSize)) && (5 != 5 || (channel >= 0 && channel < bufferChannels))) {
        if (overdub != 0) {
            number currentValue = buffer->getSample(channel, sampleIndex);

            {
                value = value + currentValue * overdub;
            }
        }

        buffer->setSample(channel, sampleIndex, value);
        buffer->setTouched(true);
    }
}

inline number linearinterp(number frac, number x, number y) {
    return x + (y - x) * frac;
}

inline number cubicinterp(number a, number w, number x, number y, number z) {
    number a1 = 1. + a;
    number aa = a * a1;
    number b = 1. - a;
    number b1 = 2. - a;
    number bb = b * b1;
    number fw = -.1666667 * bb * a;
    number fx = .5 * bb * a1;
    number fy = .5 * aa * b1;
    number fz = -.1666667 * aa * b;
    return w * fw + x * fx + y * fy + z * fz;
}

inline number fastcubicinterp(number a, number w, number x, number y, number z) {
    number a2 = a * a;
    number f0 = z - y - w + x;
    number f1 = w - x - f0;
    number f2 = y - w;
    number f3 = x;
    return f0 * a * a2 + f1 * a2 + f2 * a + f3;
}

inline number splineinterp(number a, number w, number x, number y, number z) {
    number a2 = a * a;
    number f0 = -0.5 * w + 1.5 * x - 1.5 * y + 0.5 * z;
    number f1 = w - 2.5 * x + 2 * y - 0.5 * z;
    number f2 = -0.5 * w + 0.5 * y;
    return f0 * a * a2 + f1 * a2 + f2 * a + x;
}

inline number spline6interp(number a, number y0, number y1, number y2, number y3, number y4, number y5) {
    number ym2py2 = y0 + y4;
    number ym1py1 = y1 + y3;
    number y2mym2 = y4 - y0;
    number y1mym1 = y3 - y1;
    number sixthym1py1 = (number)1 / (number)6.0 * ym1py1;
    number c0 = (number)1 / (number)120.0 * ym2py2 + (number)13 / (number)60.0 * ym1py1 + (number)11 / (number)20.0 * y2;
    number c1 = (number)1 / (number)24.0 * y2mym2 + (number)5 / (number)12.0 * y1mym1;
    number c2 = (number)1 / (number)12.0 * ym2py2 + sixthym1py1 - (number)1 / (number)2.0 * y2;
    number c3 = (number)1 / (number)12.0 * y2mym2 - (number)1 / (number)6.0 * y1mym1;
    number c4 = (number)1 / (number)24.0 * ym2py2 - sixthym1py1 + (number)1 / (number)4.0 * y2;
    number c5 = (number)1 / (number)120.0 * (y5 - y0) + (number)1 / (number)24.0 * (y1 - y4) + (number)1 / (number)12.0 * (y3 - y2);
    return ((((c5 * a + c4) * a + c3) * a + c2) * a + c1) * a + c0;
}

inline number cosT8(number r) {
    number t84 = 56.0;
    number t83 = 1680.0;
    number t82 = 20160.0;
    number t81 = 2.4801587302e-05;
    number t73 = 42.0;
    number t72 = 840.0;
    number t71 = 1.9841269841e-04;

    if (r < 0.785398163397448309615660845819875721 && r > -0.785398163397448309615660845819875721) {
        number rr = r * r;
        return 1.0 - rr * t81 * (t82 - rr * (t83 - rr * (t84 - rr)));
    } else if (r > 0.0) {
        r -= 1.57079632679489661923132169163975144;
        number rr = r * r;
        return -r * (1.0 - t71 * rr * (t72 - rr * (t73 - rr)));
    } else {
        r += 1.57079632679489661923132169163975144;
        number rr = r * r;
        return r * (1.0 - t71 * rr * (t72 - rr * (t73 - rr)));
    }
}

inline number cosineinterp(number frac, number x, number y) {
    number a2 = (1.0 - this->cosT8(frac * 3.14159265358979323846)) / (number)2.0;
    return x * (1.0 - a2) + y * a2;
}

template<typename BUFFERTYPE> array<SampleValue, 1 + 1> wave_default(
    BUFFERTYPE& buffer,
    SampleValue phase,
    SampleValue start,
    SampleValue end,
    Int channelOffset
) {
    number bufferSize = buffer->getSize();
    const Index bufferChannels = (const Index)(buffer->getChannels());
    constexpr int ___N4 = 1 + 1;
    array<SampleValue, ___N4> out = FIXEDSIZEARRAYINIT(1 + 1);

    if (start < 0)
        start = 0;

    if (end > bufferSize)
        end = bufferSize;

    if (end - start <= 0) {
        start = 0;
        end = bufferSize;
    }

    number sampleIndex;

    {
        SampleValue bufferphasetoindex_result;

        {
            auto __end = end;
            auto __start = start;
            auto __phase = phase;
            number size;

            {
                size = __end - __start;
            }

            {
                {
                    {
                        bufferphasetoindex_result = __start + __phase * size;
                    }
                }
            }
        }

        sampleIndex = bufferphasetoindex_result;
    }

    if (bufferSize == 0 || (3 == 5 && (sampleIndex < start || sampleIndex >= end))) {
        return out;
    } else {
        {
            SampleValue bufferbindindex_result;

            {
                {
                    {
                        bufferbindindex_result = this->wrap(sampleIndex, start, end);
                    }
                }
            }

            sampleIndex = bufferbindindex_result;
        }

        for (Index channel = 0; channel < 1; channel++) {
            Index channelIndex = (Index)(channel + channelOffset);

            {
                Index bufferbindchannel_result;

                {
                    {
                        {
                            bufferbindchannel_result = (bufferChannels == 0 ? 0 : channelIndex % bufferChannels);
                        }
                    }
                }

                channelIndex = bufferbindchannel_result;
            }

            SampleValue bufferreadsample_result;

            {
                auto& __buffer = buffer;

                if (sampleIndex < 0.0) {
                    bufferreadsample_result = 0.0;
                }

                SampleIndex truncIndex = (SampleIndex)(trunc(sampleIndex));

                {
                    number frac = sampleIndex - truncIndex;
                    number wrap = end - 1;

                    {
                        SampleIndex index1 = (SampleIndex)(truncIndex);

                        if (index1 < 0)
                            index1 = wrap;

                        SampleIndex index2 = (SampleIndex)(index1 + 1);

                        if (index2 > wrap)
                            index2 = start;

                        bufferreadsample_result = this->linearinterp(
                            frac,
                            __buffer->getSample(channelIndex, index1),
                            __buffer->getSample(channelIndex, index2)
                        );
                    }
                }
            }

            out[(Index)channel] = bufferreadsample_result;
        }

        out[1] = sampleIndex - start;
        return out;
    }
}

inline number safediv(number num, number denom) {
    return (denom == 0.0 ? 0.0 : num / denom);
}

number tempo() {
    return this->getTopLevelPatcher()->globaltransport_getTempo(this->_currentTime);
}

number hztobeats(number hz) {
    return this->safediv(this->tempo() * 8, hz * 480.);
}

number transportatsample(SampleIndex sampleOffset) {
    return this->getTopLevelPatcher()->globaltransport_getStateAtSample(sampleOffset);
}

number beattimeatsample(number offset) {
    return this->getTopLevelPatcher()->globaltransport_getBeatTimeAtSample(offset);
}

number beattime() {
    return this->getTopLevelPatcher()->globaltransport_getBeatTime(this->_currentTime);
}

array<number, 2> timesignature() {
    return this->getTopLevelPatcher()->globaltransport_getTimeSignature(this->_currentTime);
}

number beatstoticks(number beattime) {
    return beattime * 480;
}

number transport() {
    return this->getTopLevelPatcher()->globaltransport_getState(this->_currentTime);
}

number tickstohz(number ticks) {
    return (number)1 / (ticks / (number)480 * this->safediv(60, this->tempo()));
}

number tickstoms(number ticks) {
    return ticks / (number)480 * this->safediv(60, this->tempo()) * 1000;
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

    this->gen_02_in4_set(v);
    this->gen_01_in4_set(v);
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

    this->gen_02_in5_set(v);
    this->gen_01_in5_set(v);
}

MillisecondTime getPatcherTime() const {
    return this->_currentTime;
}

void loadbang_01_startupbang_bang() {
    this->loadbang_01_output_bang();
}

void metro_01_tick_bang() {
    this->metro_01_tickout_bang();
    this->getEngine()->flushClockEvents(this, 1935387534, false);;

    if ((bool)(this->metro_01_on)) {
        this->metro_01_last = this->_currentTime;

        {
            this->metro_01_next = this->metro_01_last + this->metro_01_interval;
            this->getEngine()->scheduleClockEvent(this, 1935387534, this->metro_01_interval + this->_currentTime);;
        }
    }
}

void deallocateSignals() {
    Index i;

    for (i = 0; i < 2; i++) {
        this->signals[i] = freeSignal(this->signals[i]);
    }

    this->phasor_01_sigbuf = freeSignal(this->phasor_01_sigbuf);
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

void fillDataRef(DataRefIndex , DataRef& ) {}

void zeroDataRef(DataRef& ref) {
    ref->setZero();
}

void allocateDataRefs() {
    this->gen_01_repeat = this->gen_01_repeat->allocateIfNeeded();
    this->gen_01_repeat_exprdata_buffer = this->gen_01_repeat_exprdata_buffer->allocateIfNeeded();
    this->gen_02_repeat = this->gen_02_repeat->allocateIfNeeded();
    this->gen_02_repeat_exprdata_buffer = this->gen_02_repeat_exprdata_buffer->allocateIfNeeded();

    if (this->repeat->hasRequestedSize()) {
        if (this->repeat->wantsFill())
            this->zeroDataRef(this->repeat);

        this->getEngine()->sendDataRefUpdated(0);
    }
}

void initializeObjects() {
    this->gen_01_repeat_exprdata_init();
    this->gen_01_latch_10_init();
    this->gen_01_counter_14_init();
    this->gen_01_latch_39_init();
    this->gen_01_latch_45_init();
    this->gen_02_repeat_exprdata_init();
    this->gen_02_latch_10_init();
    this->gen_02_counter_14_init();
    this->gen_02_latch_39_init();
    this->gen_02_latch_45_init();
}

Index getIsMuted()  {
    return this->isMuted;
}

void setIsMuted(Index v)  {
    this->isMuted = v;
}

void onSampleRateChanged(double samplerate) {
    this->timevalue_01_onSampleRateChanged(samplerate);
    this->timevalue_02_onSampleRateChanged(samplerate);
}

void extractState(PatcherStateInterface& ) {}

void applyState() {}

void processClockEvent(MillisecondTime time, ClockId index, bool hasValue, ParameterValue value) {
    RNBO_UNUSED(value);
    RNBO_UNUSED(hasValue);
    this->updateTime(time, (ENGINE*)nullptr);

    switch (index) {
    case -871642103:
        {
        this->loadbang_01_startupbang_bang();
        break;
        }
    case 1935387534:
        {
        this->metro_01_tick_bang();
        break;
        }
    }
}

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
    this->getEngine()->scheduleClockEvent(this, -871642103, 0 + this->_currentTime);;

    if ((bool)(this->metro_01_on))
        this->metro_01_on_set(1);

    this->timevalue_01_sendValue();
    this->timevalue_02_sendValue();

    {
        this->scheduleParamInit(0, 0);
    }

    {
        this->scheduleParamInit(1, 0);
    }

    this->processParamInitEvents();
}

number param_01_value_constrain(number v) const {
    v = (v > 1 ? 1 : (v < 0 ? 0 : v));
    return v;
}

void gen_02_in4_set(number v) {
    this->gen_02_in4 = v;
}

void gen_01_in4_set(number v) {
    this->gen_01_in4 = v;
}

number param_02_value_constrain(number v) const {
    v = (v > 1 ? 1 : (v < 0 ? 0 : v));
    return v;
}

void gen_02_in5_set(number v) {
    this->gen_02_in5 = v;
}

void gen_01_in5_set(number v) {
    this->gen_01_in5 = v;
}

void transport_01_ticks_set(number v) {
    this->transport_01_ticks = v;
}

void transport_01_state_set(number ) {}

template<typename LISTTYPE> void transport_01_outtimesig_set(const LISTTYPE& ) {}

void transport_01_outtempo_set(number ) {}

void transport_01_resolution_set(number ) {}

void transport_01_units_set(number ) {}

void transport_01_beats_set(number ) {}

void transport_01_bars_set(number ) {}

void transport_01_input_bang_bang() {
    auto currbeattime = this->beattime();
    auto currtimesig = this->timesignature();
    auto currticks = this->beatstoticks(currbeattime);
    number beatmult = currbeattime * (currtimesig[1] / (number)4);
    this->transport_01_ticks_set(currticks);
    this->transport_01_state_set(this->transport());
    this->transport_01_outtimesig_set(listbase<number, 2>{currtimesig[0], currtimesig[1]});
    this->transport_01_outtempo_set(this->tempo());
    this->transport_01_resolution_set(480.);
    array<number, 3> bbu = this->getTopLevelPatcher()->globaltransport_getBBU(this->_currentTime);
    this->transport_01_units_set(bbu[2]);
    this->transport_01_beats_set(bbu[1]);
    this->transport_01_bars_set(bbu[0]);
}

void loadbang_01_output_bang() {
    this->transport_01_input_bang_bang();
}

void metro_01_tickout_bang() {
    this->transport_01_input_bang_bang();
}

void metro_01_on_set(number v) {
    this->metro_01_on = v;
    this->getEngine()->flushClockEvents(this, 1935387534, false);;

    if ((bool)(v)) {
        {
            this->getEngine()->scheduleClockEvent(this, 1935387534, 0 + this->_currentTime);;
        }
    }
}

void phasor_01_freq_set(number v) {
    this->phasor_01_freq = v;
}

void timevalue_01_out_set(number v) {
    this->phasor_01_freq_set(v);
}

void metro_01_interval_set(number v) {
    this->metro_01_interval_setter(v);
    v = this->metro_01_interval;
}

void timevalue_02_out_set(number v) {
    this->metro_01_interval_set(v);
}

void phasor_01_perform(number freq, SampleValue * out, Index n) {
    auto __phasor_01_lastTempo = this->phasor_01_lastTempo;
    auto __phasor_01_lastQuantum = this->phasor_01_lastQuantum;
    auto quantum = this->hztobeats(freq);

    if (quantum != __phasor_01_lastQuantum) {
        this->phasor_01_recalcInc = true;
        __phasor_01_lastQuantum = quantum;
    }

    auto tempo = this->tempo();

    if (tempo != __phasor_01_lastTempo) {
        this->phasor_01_recalcInc = true;
        __phasor_01_lastTempo = tempo;
    }

    if (quantum > 0 && tempo > 0) {
        for (Index i = 0; i < n; i++) {
            if ((bool)(this->transportatsample(i))) {
                if ((bool)(this->phasor_01_recalcInc)) {
                    this->phasor_01_recalc(quantum, this->beattimeatsample(i), tempo);
                }

                if (this->phasor_01_nextJumpInSamples <= 0) {
                    this->phasor_01_lastLockedPhase = 0;
                    this->phasor_01_recalcInc = true;
                } else {
                    this->phasor_01_lastLockedPhase += this->phasor_01_inc;
                    this->phasor_01_nextJumpInSamples--;
                }
            } else {
                this->phasor_01_recalcInc = true;
                this->phasor_01_recalcPhase = true;
            }

            out[(Index)i] = this->phasor_01_lastLockedPhase;
        }
    } else {
        for (Index i = 0; i < n; i++) {
            out[(Index)i] = 0;
        }

        this->phasor_01_recalcInc = true;
    }

    this->phasor_01_lastQuantum = __phasor_01_lastQuantum;
    this->phasor_01_lastTempo = __phasor_01_lastTempo;
}

void beatstosamps_tilde_01_perform(number beattime, SampleValue * out1, Index n) {
    RNBO_UNUSED(beattime);
    Index i;

    for (i = 0; i < (Index)n; i++) {
        out1[(Index)i] = this->safediv(4 * 60 * this->sr, this->tempo());
    }
}

void gen_01_perform(
    const Sample * in1,
    const Sample * in2,
    const Sample * in3,
    number in4,
    number in5,
    SampleValue * out1,
    Index n
) {
    auto int_3_2 = this->intnum(1);
    number clamp_4_3 = (in4 > 1 ? 1 : (in4 < 0 ? 0 : in4));
    number gt_5_4 = clamp_4_3 > 0;
    auto int_17_20 = this->intnum(1);
    number div_18_21 = int_17_20 / (number)8;
    auto int_19_22 = this->intnum(1);
    number div_20_23 = int_19_22 / (number)16;
    auto int_21_24 = this->intnum(8);
    auto int_22_25 = this->intnum(6);
    auto int_23_26 = this->intnum(4);
    auto int_24_27 = this->intnum(3);
    auto int_25_28 = this->intnum(2);
    auto int_26_29 = this->intnum(1);
    number clamp_27_30 = (in5 > 1 ? 1 : (in5 < 0 ? 0 : in5));
    number mul_28_31 = clamp_27_30 * 8;
    number mul_31_34 = clamp_4_3 * 6;
    Index i;

    for (i = 0; i < (Index)n; i++) {
        auto repeat_dim_1_0 = this->dim(this->gen_01_repeat);
        auto repeat_chans_2_1 = this->channels(this->gen_01_repeat);
        number mod_6_5 = this->safemod(in2[(Index)i], 0.125);
        number delta_7_7 = this->gen_01_delta_6_next(mod_6_5);
        number lt_8_8 = delta_7_7 < 0;
        number latch_9_11 = this->gen_01_latch_10_next(gt_5_4, lt_8_8);
        number add_10_12 = latch_9_11 + 1;
        number counter_11 = 0;
        number counter_12 = 0;
        number counter_13 = 0;
        array<number, 3> result_15 = this->gen_01_counter_14_next(int_3_2, latch_9_11, this->sr * 24);
        counter_13 = result_15[2];
        counter_12 = result_15[1];
        counter_11 = result_15[0];
        number mod_14_16 = this->safemod(counter_11, in3[(Index)i]);
        this->poke_default(this->gen_01_repeat, in1[(Index)i], mod_14_16, 0, 0);
        number delta_15_18 = this->gen_01_delta_17_next(mod_6_5);
        number lt_16_19 = delta_15_18 < 0;
        number round_29_32 = rnbo_fround(mul_28_31 * 1 / (number)1) * 1;
        number mul_30_33 = div_18_21 * round_29_32;
        number round_32_35 = rnbo_fround(mul_31_34 * 1 / (number)1) * 1;
        number add_33_36 = round_32_35 + 1;
        number selector_34_37 = (add_33_36 >= 6 ? int_26_29 : (add_33_36 >= 5 ? int_25_28 : (add_33_36 >= 4 ? int_24_27 : (add_33_36 >= 3 ? int_23_26 : (add_33_36 >= 2 ? int_22_25 : (add_33_36 >= 1 ? int_21_24 : 0))))));
        number mul_35_38 = div_20_23 * selector_34_37;
        number latch_36_40 = this->gen_01_latch_39_next(mul_35_38, lt_16_19);
        number mod_37_41 = this->safemod(in2[(Index)i], latch_36_40);
        number delta_38_43 = this->gen_01_delta_42_next(mod_37_41);
        number lt_39_44 = delta_38_43 < 0;
        number latch_40_46 = this->gen_01_latch_45_next(mul_30_33, lt_39_44);
        number add_41_47 = mod_37_41 + latch_40_46;
        number mod_42_48 = this->safemod(add_41_47, 1);
        number sample_repeat_43 = 0;
        number index_repeat_44 = 0;
        auto result_49 = this->wave_default(this->gen_01_repeat, mod_42_48, 0, in3[(Index)i], 0);
        index_repeat_44 = result_49[1];
        sample_repeat_43 = result_49[0];
        number selector_45_50 = (add_10_12 >= 2 ? sample_repeat_43 : (add_10_12 >= 1 ? in1[(Index)i] : 0));
        out1[(Index)i] = selector_45_50;
    }
}

void gen_02_perform(
    const Sample * in1,
    const Sample * in2,
    const Sample * in3,
    number in4,
    number in5,
    SampleValue * out1,
    Index n
) {
    auto int_3_2 = this->intnum(1);
    number clamp_4_3 = (in4 > 1 ? 1 : (in4 < 0 ? 0 : in4));
    number gt_5_4 = clamp_4_3 > 0;
    auto int_17_20 = this->intnum(1);
    number div_18_21 = int_17_20 / (number)8;
    auto int_19_22 = this->intnum(1);
    number div_20_23 = int_19_22 / (number)16;
    auto int_21_24 = this->intnum(8);
    auto int_22_25 = this->intnum(6);
    auto int_23_26 = this->intnum(4);
    auto int_24_27 = this->intnum(3);
    auto int_25_28 = this->intnum(2);
    auto int_26_29 = this->intnum(1);
    number clamp_27_30 = (in5 > 1 ? 1 : (in5 < 0 ? 0 : in5));
    number mul_28_31 = clamp_27_30 * 8;
    number mul_31_34 = clamp_4_3 * 6;
    Index i;

    for (i = 0; i < (Index)n; i++) {
        auto repeat_dim_1_0 = this->dim(this->gen_02_repeat);
        auto repeat_chans_2_1 = this->channels(this->gen_02_repeat);
        number mod_6_5 = this->safemod(in2[(Index)i], 0.125);
        number delta_7_7 = this->gen_02_delta_6_next(mod_6_5);
        number lt_8_8 = delta_7_7 < 0;
        number latch_9_11 = this->gen_02_latch_10_next(gt_5_4, lt_8_8);
        number add_10_12 = latch_9_11 + 1;
        number counter_11 = 0;
        number counter_12 = 0;
        number counter_13 = 0;
        array<number, 3> result_15 = this->gen_02_counter_14_next(int_3_2, latch_9_11, this->sr * 24);
        counter_13 = result_15[2];
        counter_12 = result_15[1];
        counter_11 = result_15[0];
        number mod_14_16 = this->safemod(counter_11, in3[(Index)i]);
        this->poke_default(this->gen_02_repeat, in1[(Index)i], mod_14_16, 0, 0);
        number delta_15_18 = this->gen_02_delta_17_next(mod_6_5);
        number lt_16_19 = delta_15_18 < 0;
        number round_29_32 = rnbo_fround(mul_28_31 * 1 / (number)1) * 1;
        number mul_30_33 = div_18_21 * round_29_32;
        number round_32_35 = rnbo_fround(mul_31_34 * 1 / (number)1) * 1;
        number add_33_36 = round_32_35 + 1;
        number selector_34_37 = (add_33_36 >= 6 ? int_26_29 : (add_33_36 >= 5 ? int_25_28 : (add_33_36 >= 4 ? int_24_27 : (add_33_36 >= 3 ? int_23_26 : (add_33_36 >= 2 ? int_22_25 : (add_33_36 >= 1 ? int_21_24 : 0))))));
        number mul_35_38 = div_20_23 * selector_34_37;
        number latch_36_40 = this->gen_02_latch_39_next(mul_35_38, lt_16_19);
        number mod_37_41 = this->safemod(in2[(Index)i], latch_36_40);
        number delta_38_43 = this->gen_02_delta_42_next(mod_37_41);
        number lt_39_44 = delta_38_43 < 0;
        number latch_40_46 = this->gen_02_latch_45_next(mul_30_33, lt_39_44);
        number add_41_47 = mod_37_41 + latch_40_46;
        number mod_42_48 = this->safemod(add_41_47, 1);
        number sample_repeat_43 = 0;
        number index_repeat_44 = 0;
        auto result_49 = this->wave_default(this->gen_02_repeat, mod_42_48, 0, in3[(Index)i], 0);
        index_repeat_44 = result_49[1];
        sample_repeat_43 = result_49[0];
        number selector_45_50 = (add_10_12 >= 2 ? sample_repeat_43 : (add_10_12 >= 1 ? in1[(Index)i] : 0));
        out1[(Index)i] = selector_45_50;
    }
}

void stackprotect_perform(Index n) {
    RNBO_UNUSED(n);
    auto __stackprotect_count = this->stackprotect_count;
    __stackprotect_count = 0;
    this->stackprotect_count = __stackprotect_count;
}

void metro_01_interval_setter(number v) {
    this->metro_01_interval = (v > 0 ? v : 0);
}

void gen_01_repeat_exprdata_init() {
    number sizeInSamples = this->gen_01_repeat_exprdata_evaluateSizeExpr(this->sr, this->vs);
    this->gen_01_repeat_exprdata_buffer->requestSize(sizeInSamples, 1);
}

void gen_01_repeat_exprdata_dspsetup() {
    number sizeInSamples = this->gen_01_repeat_exprdata_evaluateSizeExpr(this->sr, this->vs);
    this->gen_01_repeat_exprdata_buffer = this->gen_01_repeat_exprdata_buffer->setSize(sizeInSamples);
    updateDataRef(this, this->gen_01_repeat_exprdata_buffer);
}

number gen_01_repeat_exprdata_evaluateSizeExpr(number samplerate, number vectorsize) {
    RNBO_UNUSED(vectorsize);
    return samplerate * 24;
}

void gen_01_repeat_exprdata_reset() {}

number gen_01_delta_6_next(number x) {
    number temp = (number)(x - this->gen_01_delta_6_prev);
    this->gen_01_delta_6_prev = x;
    return temp;
}

void gen_01_delta_6_dspsetup() {
    this->gen_01_delta_6_reset();
}

void gen_01_delta_6_reset() {
    this->gen_01_delta_6_prev = 0;
}

number gen_01_latch_10_next(number x, number control) {
    if (control != 0.) {
        this->gen_01_latch_10_value = x;
    }

    return this->gen_01_latch_10_value;
}

void gen_01_latch_10_dspsetup() {
    this->gen_01_latch_10_reset();
}

void gen_01_latch_10_init() {
    this->gen_01_latch_10_reset();
}

void gen_01_latch_10_reset() {
    this->gen_01_latch_10_value = 0;
}

array<number, 3> gen_01_counter_14_next(number a, number reset, number limit) {
    number carry_flag = 0;

    if (reset != 0) {
        this->gen_01_counter_14_count = 0;
        this->gen_01_counter_14_carry = 0;
    } else {
        this->gen_01_counter_14_count += a;

        if (limit != 0) {
            if ((a > 0 && this->gen_01_counter_14_count >= limit) || (a < 0 && this->gen_01_counter_14_count <= limit)) {
                this->gen_01_counter_14_count = 0;
                this->gen_01_counter_14_carry += 1;
                carry_flag = 1;
            }
        }
    }

    return {this->gen_01_counter_14_count, carry_flag, this->gen_01_counter_14_carry};
}

void gen_01_counter_14_init() {
    this->gen_01_counter_14_count = 0;
}

void gen_01_counter_14_reset() {
    this->gen_01_counter_14_carry = 0;
    this->gen_01_counter_14_count = 0;
}

number gen_01_delta_17_next(number x) {
    number temp = (number)(x - this->gen_01_delta_17_prev);
    this->gen_01_delta_17_prev = x;
    return temp;
}

void gen_01_delta_17_dspsetup() {
    this->gen_01_delta_17_reset();
}

void gen_01_delta_17_reset() {
    this->gen_01_delta_17_prev = 0;
}

number gen_01_latch_39_next(number x, number control) {
    if (control != 0.) {
        this->gen_01_latch_39_value = x;
    }

    return this->gen_01_latch_39_value;
}

void gen_01_latch_39_dspsetup() {
    this->gen_01_latch_39_reset();
}

void gen_01_latch_39_init() {
    this->gen_01_latch_39_reset();
}

void gen_01_latch_39_reset() {
    this->gen_01_latch_39_value = 0;
}

number gen_01_delta_42_next(number x) {
    number temp = (number)(x - this->gen_01_delta_42_prev);
    this->gen_01_delta_42_prev = x;
    return temp;
}

void gen_01_delta_42_dspsetup() {
    this->gen_01_delta_42_reset();
}

void gen_01_delta_42_reset() {
    this->gen_01_delta_42_prev = 0;
}

number gen_01_latch_45_next(number x, number control) {
    if (control != 0.) {
        this->gen_01_latch_45_value = x;
    }

    return this->gen_01_latch_45_value;
}

void gen_01_latch_45_dspsetup() {
    this->gen_01_latch_45_reset();
}

void gen_01_latch_45_init() {
    this->gen_01_latch_45_reset();
}

void gen_01_latch_45_reset() {
    this->gen_01_latch_45_value = 0;
}

void gen_01_dspsetup(bool force) {
    if ((bool)(this->gen_01_setupDone) && (bool)(!(bool)(force)))
        return;

    this->gen_01_setupDone = true;
    this->gen_01_repeat_exprdata_dspsetup();
    this->gen_01_delta_6_dspsetup();
    this->gen_01_latch_10_dspsetup();
    this->gen_01_delta_17_dspsetup();
    this->gen_01_latch_39_dspsetup();
    this->gen_01_delta_42_dspsetup();
    this->gen_01_latch_45_dspsetup();
}

void phasor_01_onBeatTimeChanged(number beattime) {
    RNBO_UNUSED(beattime);
    this->phasor_01_recalcInc = true;
    this->phasor_01_recalcPhase = true;
}

void phasor_01_recalc(number quantum, number beattime, number tempo) {
    number samplerate = this->sr;
    number offset = fmod(beattime, quantum);
    number nextJump = quantum - offset;

    if ((bool)(this->phasor_01_recalcPhase)) {
        number oneoverquantum = (number)1 / quantum;
        this->phasor_01_lastLockedPhase = offset * oneoverquantum;
        this->phasor_01_recalcPhase = false;
    }

    this->phasor_01_nextJumpInSamples = rnbo_fround(nextJump * 60 * samplerate / tempo * 1 / (number)1) * 1;
    this->phasor_01_inc = (1 - this->phasor_01_lastLockedPhase) / (this->phasor_01_nextJumpInSamples - 1);
    this->phasor_01_recalcInc = false;
}

void transport_01_onTempoChanged(number state) {
    this->transport_01_outtempo_set(state);
}

void transport_01_onTransportChanged(number state) {
    this->transport_01_state_set(state);
}

void transport_01_onTimeSignatureChanged(number numerator, number denominator) {
    this->transport_01_outtimesig_set(listbase<number, 2>{numerator, denominator});
}

void metro_01_onTransportChanged(number ) {}

void metro_01_onBeatTimeChanged(number ) {}

void param_01_getPresetValue(PatcherStateInterface& preset) {
    preset["value"] = this->param_01_value;
}

void param_01_setPresetValue(PatcherStateInterface& preset) {
    if ((bool)(stateIsEmpty(preset)))
        return;

    this->param_01_value_set(preset["value"]);
}

void gen_02_repeat_exprdata_init() {
    number sizeInSamples = this->gen_02_repeat_exprdata_evaluateSizeExpr(this->sr, this->vs);
    this->gen_02_repeat_exprdata_buffer->requestSize(sizeInSamples, 1);
}

void gen_02_repeat_exprdata_dspsetup() {
    number sizeInSamples = this->gen_02_repeat_exprdata_evaluateSizeExpr(this->sr, this->vs);
    this->gen_02_repeat_exprdata_buffer = this->gen_02_repeat_exprdata_buffer->setSize(sizeInSamples);
    updateDataRef(this, this->gen_02_repeat_exprdata_buffer);
}

number gen_02_repeat_exprdata_evaluateSizeExpr(number samplerate, number vectorsize) {
    RNBO_UNUSED(vectorsize);
    return samplerate * 24;
}

void gen_02_repeat_exprdata_reset() {}

number gen_02_delta_6_next(number x) {
    number temp = (number)(x - this->gen_02_delta_6_prev);
    this->gen_02_delta_6_prev = x;
    return temp;
}

void gen_02_delta_6_dspsetup() {
    this->gen_02_delta_6_reset();
}

void gen_02_delta_6_reset() {
    this->gen_02_delta_6_prev = 0;
}

number gen_02_latch_10_next(number x, number control) {
    if (control != 0.) {
        this->gen_02_latch_10_value = x;
    }

    return this->gen_02_latch_10_value;
}

void gen_02_latch_10_dspsetup() {
    this->gen_02_latch_10_reset();
}

void gen_02_latch_10_init() {
    this->gen_02_latch_10_reset();
}

void gen_02_latch_10_reset() {
    this->gen_02_latch_10_value = 0;
}

array<number, 3> gen_02_counter_14_next(number a, number reset, number limit) {
    number carry_flag = 0;

    if (reset != 0) {
        this->gen_02_counter_14_count = 0;
        this->gen_02_counter_14_carry = 0;
    } else {
        this->gen_02_counter_14_count += a;

        if (limit != 0) {
            if ((a > 0 && this->gen_02_counter_14_count >= limit) || (a < 0 && this->gen_02_counter_14_count <= limit)) {
                this->gen_02_counter_14_count = 0;
                this->gen_02_counter_14_carry += 1;
                carry_flag = 1;
            }
        }
    }

    return {this->gen_02_counter_14_count, carry_flag, this->gen_02_counter_14_carry};
}

void gen_02_counter_14_init() {
    this->gen_02_counter_14_count = 0;
}

void gen_02_counter_14_reset() {
    this->gen_02_counter_14_carry = 0;
    this->gen_02_counter_14_count = 0;
}

number gen_02_delta_17_next(number x) {
    number temp = (number)(x - this->gen_02_delta_17_prev);
    this->gen_02_delta_17_prev = x;
    return temp;
}

void gen_02_delta_17_dspsetup() {
    this->gen_02_delta_17_reset();
}

void gen_02_delta_17_reset() {
    this->gen_02_delta_17_prev = 0;
}

number gen_02_latch_39_next(number x, number control) {
    if (control != 0.) {
        this->gen_02_latch_39_value = x;
    }

    return this->gen_02_latch_39_value;
}

void gen_02_latch_39_dspsetup() {
    this->gen_02_latch_39_reset();
}

void gen_02_latch_39_init() {
    this->gen_02_latch_39_reset();
}

void gen_02_latch_39_reset() {
    this->gen_02_latch_39_value = 0;
}

number gen_02_delta_42_next(number x) {
    number temp = (number)(x - this->gen_02_delta_42_prev);
    this->gen_02_delta_42_prev = x;
    return temp;
}

void gen_02_delta_42_dspsetup() {
    this->gen_02_delta_42_reset();
}

void gen_02_delta_42_reset() {
    this->gen_02_delta_42_prev = 0;
}

number gen_02_latch_45_next(number x, number control) {
    if (control != 0.) {
        this->gen_02_latch_45_value = x;
    }

    return this->gen_02_latch_45_value;
}

void gen_02_latch_45_dspsetup() {
    this->gen_02_latch_45_reset();
}

void gen_02_latch_45_init() {
    this->gen_02_latch_45_reset();
}

void gen_02_latch_45_reset() {
    this->gen_02_latch_45_value = 0;
}

void gen_02_dspsetup(bool force) {
    if ((bool)(this->gen_02_setupDone) && (bool)(!(bool)(force)))
        return;

    this->gen_02_setupDone = true;
    this->gen_02_repeat_exprdata_dspsetup();
    this->gen_02_delta_6_dspsetup();
    this->gen_02_latch_10_dspsetup();
    this->gen_02_delta_17_dspsetup();
    this->gen_02_latch_39_dspsetup();
    this->gen_02_delta_42_dspsetup();
    this->gen_02_latch_45_dspsetup();
}

void param_02_getPresetValue(PatcherStateInterface& preset) {
    preset["value"] = this->param_02_value;
}

void param_02_setPresetValue(PatcherStateInterface& preset) {
    if ((bool)(stateIsEmpty(preset)))
        return;

    this->param_02_value_set(preset["value"]);
}

void timevalue_01_sendValue() {
    {
        auto currentSig = this->timesignature();
        number ticksPerBeat = (number)1 / currentSig[1] * 4 * 480;
        number ticksPerBar = currentSig[0] * ticksPerBeat;
        list bbu = listbase<number, 3>{1, 0, 0};
        number tickValue = bbu[0] * ticksPerBar + bbu[1] * ticksPerBeat + bbu[2];

        {
            {
                {
                    {
                        this->timevalue_01_out_set(this->tickstohz(tickValue));
                    }
                }
            }
        }
    }
}

void timevalue_01_onTempoChanged(number tempo) {
    RNBO_UNUSED(tempo);

    {
        this->timevalue_01_sendValue();
    }
}

void timevalue_01_onSampleRateChanged(number ) {}

void timevalue_01_onTimeSignatureChanged(number numerator, number denominator) {
    {
        console->log("num: ", numerator, " den: ", denominator);
        this->timevalue_01_sendValue();
    }
}

void timevalue_02_sendValue() {
    {
        {
            {
                {
                    this->timevalue_02_out_set(this->tickstoms(480));
                }
            }
        }
    }
}

void timevalue_02_onTempoChanged(number tempo) {
    RNBO_UNUSED(tempo);

    {
        this->timevalue_02_sendValue();
    }
}

void timevalue_02_onSampleRateChanged(number ) {}

void timevalue_02_onTimeSignatureChanged(number , number ) {}

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
    gen_01_in1 = 0;
    gen_01_in2 = 0;
    gen_01_in3 = 0;
    gen_01_in4 = 0;
    gen_01_in5 = 0;
    phasor_01_freq = 0;
    beatstosamps_tilde_01_beattime = 4;
    transport_01_input_number = 0;
    transport_01_position = 0;
    transport_01_tempo = 120;
    transport_01_ticks = 0;
    metro_01_on = 1;
    metro_01_interval = 0;
    metro_01_interval_setter(metro_01_interval);
    param_01_value = 0;
    gen_02_in1 = 0;
    gen_02_in2 = 0;
    gen_02_in3 = 0;
    gen_02_in4 = 0;
    gen_02_in5 = 0;
    param_02_value = 0;
    _currentTime = 0;
    audioProcessSampleCount = 0;
    sampleOffsetIntoNextAudioBuffer = 0;
    zeroBuffer = nullptr;
    dummyBuffer = nullptr;
    signals[0] = nullptr;
    signals[1] = nullptr;
    didAllocateSignals = 0;
    vs = 0;
    maxvs = 0;
    sr = 44100;
    invsr = 0.000022675736961451248;
    gen_01_delta_6_prev = 0;
    gen_01_latch_10_value = 0;
    gen_01_counter_14_carry = 0;
    gen_01_counter_14_count = 0;
    gen_01_delta_17_prev = 0;
    gen_01_latch_39_value = 0;
    gen_01_delta_42_prev = 0;
    gen_01_latch_45_value = 0;
    gen_01_setupDone = false;
    phasor_01_sigbuf = nullptr;
    phasor_01_lastLockedPhase = 0;
    phasor_01_lastQuantum = 0;
    phasor_01_lastTempo = 0;
    phasor_01_nextJumpInSamples = 0;
    phasor_01_inc = 0;
    phasor_01_recalcInc = true;
    phasor_01_recalcPhase = true;
    metro_01_last = -1;
    metro_01_next = -1;
    param_01_lastValue = 0;
    gen_02_delta_6_prev = 0;
    gen_02_latch_10_value = 0;
    gen_02_counter_14_carry = 0;
    gen_02_counter_14_count = 0;
    gen_02_delta_17_prev = 0;
    gen_02_latch_39_value = 0;
    gen_02_delta_42_prev = 0;
    gen_02_latch_45_value = 0;
    gen_02_setupDone = false;
    param_02_lastValue = 0;
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
    	static constexpr auto& name0 = "repeat";
    	static constexpr auto& file0 = "";
    	static constexpr auto& tag0 = "buffer~";
    	DataRefStrings* operator->() { return this; }
    	const DataRefStrings* operator->() const { return this; }
    };

    DataRefStrings dataRefStrings;

// member variables

    number gen_01_in1;
    number gen_01_in2;
    number gen_01_in3;
    number gen_01_in4;
    number gen_01_in5;
    number phasor_01_freq;
    number beatstosamps_tilde_01_beattime;
    number transport_01_input_number;
    number transport_01_position;
    number transport_01_tempo;
    list transport_01_timesig;
    number transport_01_ticks;
    number metro_01_on;
    number metro_01_interval;
    number param_01_value;
    number gen_02_in1;
    number gen_02_in2;
    number gen_02_in3;
    number gen_02_in4;
    number gen_02_in5;
    number param_02_value;
    MillisecondTime _currentTime;
    ENGINE _internalEngine;
    UInt64 audioProcessSampleCount;
    Index sampleOffsetIntoNextAudioBuffer;
    signal zeroBuffer;
    signal dummyBuffer;
    SampleValue * signals[2];
    bool didAllocateSignals;
    Index vs;
    Index maxvs;
    number sr;
    number invsr;
    Float64BufferRef gen_01_repeat_exprdata_buffer;
    SampleBufferRef gen_01_repeat;
    number gen_01_delta_6_prev;
    number gen_01_latch_10_value;
    Int gen_01_counter_14_carry;
    number gen_01_counter_14_count;
    number gen_01_delta_17_prev;
    number gen_01_latch_39_value;
    number gen_01_delta_42_prev;
    number gen_01_latch_45_value;
    bool gen_01_setupDone;
    signal phasor_01_sigbuf;
    number phasor_01_lastLockedPhase;
    number phasor_01_lastQuantum;
    number phasor_01_lastTempo;
    number phasor_01_nextJumpInSamples;
    number phasor_01_inc;
    bool phasor_01_recalcInc;
    bool phasor_01_recalcPhase;
    MillisecondTime metro_01_last;
    MillisecondTime metro_01_next;
    number param_01_lastValue;
    Float64BufferRef gen_02_repeat_exprdata_buffer;
    SampleBufferRef gen_02_repeat;
    number gen_02_delta_6_prev;
    number gen_02_latch_10_value;
    Int gen_02_counter_14_carry;
    number gen_02_counter_14_count;
    number gen_02_delta_17_prev;
    number gen_02_latch_39_value;
    number gen_02_delta_42_prev;
    number gen_02_latch_45_value;
    bool gen_02_setupDone;
    number param_02_lastValue;
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
    DataRef repeat;
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

