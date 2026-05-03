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
    const SampleValue * in1 = (numInputs >= 1 && inputs[0] ? inputs[0] : this->zeroBuffer);
    const SampleValue * in2 = (numInputs >= 2 && inputs[1] ? inputs[1] : this->zeroBuffer);
    const SampleValue * in3 = (numInputs >= 3 && inputs[2] ? inputs[2] : this->zeroBuffer);
    this->feedbackreader_01_perform(this->signals[0], n);
    this->dspexpr_01_perform(in1, this->signals[0], this->signals[1], n);

    this->cycle_tilde_01_perform(
        this->signals[1],
        this->cycle_tilde_01_phase_offset,
        this->signals[0],
        this->dummyBuffer,
        n
    );

    this->feedbackreader_02_perform(this->signals[1], n);
    this->dspexpr_03_perform(in2, this->signals[1], this->signals[2], n);

    this->cycle_tilde_02_perform(
        this->signals[2],
        this->cycle_tilde_02_phase_offset,
        this->signals[1],
        this->dummyBuffer,
        n
    );

    this->signaladder_01_perform(this->signals[0], this->signals[1], this->signals[2], n);
    this->feedbackwriter_03_perform(this->signals[2], n);
    this->feedbackreader_03_perform(this->signals[3], n);
    this->dspexpr_04_perform(in3, this->signals[3], this->signals[4], n);

    this->cycle_tilde_03_perform(
        this->signals[4],
        this->cycle_tilde_03_phase_offset,
        this->signals[3],
        this->dummyBuffer,
        n
    );

    this->signaladder_02_perform(this->signals[3], this->signals[1], this->signals[0], this->signals[4], n);
    this->dspexpr_02_perform(this->signals[4], this->dspexpr_02_in2, out1, n);
    this->signaladder_03_perform(this->signals[3], this->signals[1], this->signals[1], n);
    this->feedbackwriter_01_perform(this->signals[1], n);
    this->signaladder_04_perform(this->signals[3], this->signals[0], this->signals[0], n);
    this->feedbackwriter_02_perform(this->signals[0], n);
    this->stackprotect_perform(n);
    this->globaltransport_advance();
    this->advanceTime((ENGINE*)nullptr);
    this->audioProcessSampleCount += this->vs;
}

void prepareToProcess(number sampleRate, Index maxBlockSize, bool force) {
    RNBO_ASSERT(this->_isInitialized);

    if (this->maxvs < maxBlockSize || !this->didAllocateSignals) {
        Index i;

        for (i = 0; i < 5; i++) {
            this->signals[i] = resizeSignal(this->signals[i], this->maxvs, maxBlockSize);
        }

        this->feedbacktilde_01_feedbackbuffer = resizeSignal(this->feedbacktilde_01_feedbackbuffer, this->maxvs, maxBlockSize);
        this->feedbacktilde_02_feedbackbuffer = resizeSignal(this->feedbacktilde_02_feedbackbuffer, this->maxvs, maxBlockSize);
        this->feedbacktilde_03_feedbackbuffer = resizeSignal(this->feedbacktilde_03_feedbackbuffer, this->maxvs, maxBlockSize);
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

    this->cycle_tilde_01_dspsetup(forceDSPSetup);
    this->cycle_tilde_02_dspsetup(forceDSPSetup);
    this->cycle_tilde_03_dspsetup(forceDSPSetup);
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
    return 3;
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
        this->cycle_tilde_02_buffer = reInitDataView(this->cycle_tilde_02_buffer, this->RNBODefaultSinus);
        this->cycle_tilde_03_buffer = reInitDataView(this->cycle_tilde_03_buffer, this->RNBODefaultSinus);
        this->cycle_tilde_01_bufferUpdated();
        this->cycle_tilde_02_bufferUpdated();
        this->cycle_tilde_03_bufferUpdated();
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
    this->cycle_tilde_02_buffer = new SampleBuffer(this->RNBODefaultSinus);
    this->cycle_tilde_03_buffer = new SampleBuffer(this->RNBODefaultSinus);
    this->initializeObjects();
    this->allocateDataRefs();
    this->startup();
    this->_isInitialized = true;
}

void getPreset(PatcherStateInterface& preset) {
    this->updateTime(this->getEngine()->getCurrentTime(), (ENGINE*)nullptr);
    preset["__presetid"] = "rnbo";
}

void setPreset(MillisecondTime time, PatcherStateInterface& preset) {
    this->updateTime(time, (ENGINE*)nullptr);
}

void setParameterValue(ParameterIndex , ParameterValue , MillisecondTime ) {}

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
    return 0;
}

ConstCharPointer getParameterName(ParameterIndex index) const {
    switch (index) {
    default:
        {
        return "bogus";
        }
    }
}

ConstCharPointer getParameterId(ParameterIndex index) const {
    switch (index) {
    default:
        {
        return "bogus";
        }
    }
}

void getParameterInfo(ParameterIndex , ParameterInfo * ) const {}

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
    default:
        {
        return value;
        }
    }
}

ParameterValue convertFromNormalizedParameterValue(ParameterIndex index, ParameterValue value) const {
    value = (value < 0 ? 0 : (value > 1 ? 1 : value));

    switch (index) {
    default:
        {
        return value;
        }
    }
}

ParameterValue constrainParameterValue(ParameterIndex index, ParameterValue value) const {
    switch (index) {
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

number mstosamps(MillisecondTime ms) {
    return ms * this->sr * 0.001;
}

number maximum(number x, number y) {
    return (x < y ? y : x);
}

MillisecondTime sampstoms(number samps) {
    return samps * 1000 / this->sr;
}

MillisecondTime getPatcherTime() const {
    return this->_currentTime;
}

void deallocateSignals() {
    Index i;

    for (i = 0; i < 5; i++) {
        this->signals[i] = freeSignal(this->signals[i]);
    }

    this->feedbacktilde_01_feedbackbuffer = freeSignal(this->feedbacktilde_01_feedbackbuffer);
    this->feedbacktilde_02_feedbackbuffer = freeSignal(this->feedbacktilde_02_feedbackbuffer);
    this->feedbacktilde_03_feedbackbuffer = freeSignal(this->feedbacktilde_03_feedbackbuffer);
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
    this->cycle_tilde_02_buffer->requestSize(16384, 1);
    this->cycle_tilde_02_buffer->setSampleRate(this->sr);
    this->cycle_tilde_03_buffer->requestSize(16384, 1);
    this->cycle_tilde_03_buffer->setSampleRate(this->sr);
    this->cycle_tilde_01_buffer = this->cycle_tilde_01_buffer->allocateIfNeeded();
    this->cycle_tilde_02_buffer = this->cycle_tilde_02_buffer->allocateIfNeeded();
    this->cycle_tilde_03_buffer = this->cycle_tilde_03_buffer->allocateIfNeeded();

    if (this->RNBODefaultSinus->hasRequestedSize()) {
        if (this->RNBODefaultSinus->wantsFill())
            this->fillRNBODefaultSinus(this->RNBODefaultSinus);

        this->getEngine()->sendDataRefUpdated(0);
    }
}

void initializeObjects() {}

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
    this->processParamInitEvents();
}

void feedbackreader_01_perform(SampleValue * output, Index n) {
    auto& buffer = this->feedbacktilde_01_feedbackbuffer;

    for (Index i = 0; i < n; i++) {
        output[(Index)i] = buffer[(Index)i];
    }
}

void dspexpr_01_perform(const Sample * in1, const Sample * in2, SampleValue * out1, Index n) {
    Index i;

    for (i = 0; i < (Index)n; i++) {
        out1[(Index)i] = in1[(Index)i] * in2[(Index)i];//#map:_###_obj_###_:1
    }
}

void cycle_tilde_01_perform(
    const Sample * frequency,
    number phase_offset,
    SampleValue * out1,
    SampleValue * out2,
    Index n
) {
    RNBO_UNUSED(phase_offset);
    auto __cycle_tilde_01_f2i = this->cycle_tilde_01_f2i;
    auto __cycle_tilde_01_buffer = this->cycle_tilde_01_buffer;
    auto __cycle_tilde_01_phasei = this->cycle_tilde_01_phasei;
    Index i;

    for (i = 0; i < (Index)n; i++) {
        {
            UInt32 uint_phase;

            {
                {
                    uint_phase = __cycle_tilde_01_phasei;
                }
            }

            UInt32 idx = (UInt32)(uint32_rshift(uint_phase, 18));
            number frac = ((BinOpInt)((BinOpInt)uint_phase & (BinOpInt)262143)) * 3.81471181759574e-6;
            number y0 = __cycle_tilde_01_buffer[(Index)idx];
            number y1 = __cycle_tilde_01_buffer[(Index)((BinOpInt)(idx + 1) & (BinOpInt)16383)];
            number y = y0 + frac * (y1 - y0);

            {
                UInt32 pincr = (UInt32)(uint32_trunc(frequency[(Index)i] * __cycle_tilde_01_f2i));
                __cycle_tilde_01_phasei = uint32_add(__cycle_tilde_01_phasei, pincr);
            }

            out1[(Index)i] = y;
            out2[(Index)i] = uint_phase * 0.232830643653869629e-9;
            continue;
        }
    }

    this->cycle_tilde_01_phasei = __cycle_tilde_01_phasei;
}

void feedbackreader_02_perform(SampleValue * output, Index n) {
    auto& buffer = this->feedbacktilde_02_feedbackbuffer;

    for (Index i = 0; i < n; i++) {
        output[(Index)i] = buffer[(Index)i];
    }
}

void dspexpr_03_perform(const Sample * in1, const Sample * in2, SampleValue * out1, Index n) {
    Index i;

    for (i = 0; i < (Index)n; i++) {
        out1[(Index)i] = in1[(Index)i] * in2[(Index)i];//#map:_###_obj_###_:1
    }
}

void cycle_tilde_02_perform(
    const Sample * frequency,
    number phase_offset,
    SampleValue * out1,
    SampleValue * out2,
    Index n
) {
    RNBO_UNUSED(phase_offset);
    auto __cycle_tilde_02_f2i = this->cycle_tilde_02_f2i;
    auto __cycle_tilde_02_buffer = this->cycle_tilde_02_buffer;
    auto __cycle_tilde_02_phasei = this->cycle_tilde_02_phasei;
    Index i;

    for (i = 0; i < (Index)n; i++) {
        {
            UInt32 uint_phase;

            {
                {
                    uint_phase = __cycle_tilde_02_phasei;
                }
            }

            UInt32 idx = (UInt32)(uint32_rshift(uint_phase, 18));
            number frac = ((BinOpInt)((BinOpInt)uint_phase & (BinOpInt)262143)) * 3.81471181759574e-6;
            number y0 = __cycle_tilde_02_buffer[(Index)idx];
            number y1 = __cycle_tilde_02_buffer[(Index)((BinOpInt)(idx + 1) & (BinOpInt)16383)];
            number y = y0 + frac * (y1 - y0);

            {
                UInt32 pincr = (UInt32)(uint32_trunc(frequency[(Index)i] * __cycle_tilde_02_f2i));
                __cycle_tilde_02_phasei = uint32_add(__cycle_tilde_02_phasei, pincr);
            }

            out1[(Index)i] = y;
            out2[(Index)i] = uint_phase * 0.232830643653869629e-9;
            continue;
        }
    }

    this->cycle_tilde_02_phasei = __cycle_tilde_02_phasei;
}

void signaladder_01_perform(
    const SampleValue * in1,
    const SampleValue * in2,
    SampleValue * out,
    Index n
) {
    Index i;

    for (i = 0; i < (Index)n; i++) {
        out[(Index)i] = in1[(Index)i] + in2[(Index)i];
    }
}

void feedbackwriter_03_perform(const SampleValue * input, Index n) {
    auto& buffer = this->feedbacktilde_03_feedbackbuffer;

    for (Index i = 0; i < n; i++) {
        buffer[(Index)i] = input[(Index)i];
    }
}

void feedbackreader_03_perform(SampleValue * output, Index n) {
    auto& buffer = this->feedbacktilde_03_feedbackbuffer;

    for (Index i = 0; i < n; i++) {
        output[(Index)i] = buffer[(Index)i];
    }
}

void dspexpr_04_perform(const Sample * in1, const Sample * in2, SampleValue * out1, Index n) {
    Index i;

    for (i = 0; i < (Index)n; i++) {
        out1[(Index)i] = in1[(Index)i] * in2[(Index)i];//#map:_###_obj_###_:1
    }
}

void cycle_tilde_03_perform(
    const Sample * frequency,
    number phase_offset,
    SampleValue * out1,
    SampleValue * out2,
    Index n
) {
    RNBO_UNUSED(phase_offset);
    auto __cycle_tilde_03_f2i = this->cycle_tilde_03_f2i;
    auto __cycle_tilde_03_buffer = this->cycle_tilde_03_buffer;
    auto __cycle_tilde_03_phasei = this->cycle_tilde_03_phasei;
    Index i;

    for (i = 0; i < (Index)n; i++) {
        {
            UInt32 uint_phase;

            {
                {
                    uint_phase = __cycle_tilde_03_phasei;
                }
            }

            UInt32 idx = (UInt32)(uint32_rshift(uint_phase, 18));
            number frac = ((BinOpInt)((BinOpInt)uint_phase & (BinOpInt)262143)) * 3.81471181759574e-6;
            number y0 = __cycle_tilde_03_buffer[(Index)idx];
            number y1 = __cycle_tilde_03_buffer[(Index)((BinOpInt)(idx + 1) & (BinOpInt)16383)];
            number y = y0 + frac * (y1 - y0);

            {
                UInt32 pincr = (UInt32)(uint32_trunc(frequency[(Index)i] * __cycle_tilde_03_f2i));
                __cycle_tilde_03_phasei = uint32_add(__cycle_tilde_03_phasei, pincr);
            }

            out1[(Index)i] = y;
            out2[(Index)i] = uint_phase * 0.232830643653869629e-9;
            continue;
        }
    }

    this->cycle_tilde_03_phasei = __cycle_tilde_03_phasei;
}

void signaladder_02_perform(
    const SampleValue * in1,
    const SampleValue * in2,
    const SampleValue * in3,
    SampleValue * out,
    Index n
) {
    Index i;

    for (i = 0; i < (Index)n; i++) {
        out[(Index)i] = in1[(Index)i] + in2[(Index)i] + in3[(Index)i];
    }
}

void dspexpr_02_perform(const Sample * in1, number in2, SampleValue * out1, Index n) {
    RNBO_UNUSED(in2);
    Index i;

    for (i = 0; i < (Index)n; i++) {
        out1[(Index)i] = in1[(Index)i] * 0.5;//#map:_###_obj_###_:1
    }
}

void signaladder_03_perform(
    const SampleValue * in1,
    const SampleValue * in2,
    SampleValue * out,
    Index n
) {
    Index i;

    for (i = 0; i < (Index)n; i++) {
        out[(Index)i] = in1[(Index)i] + in2[(Index)i];
    }
}

void feedbackwriter_01_perform(const SampleValue * input, Index n) {
    auto& buffer = this->feedbacktilde_01_feedbackbuffer;

    for (Index i = 0; i < n; i++) {
        buffer[(Index)i] = input[(Index)i];
    }
}

void signaladder_04_perform(
    const SampleValue * in1,
    const SampleValue * in2,
    SampleValue * out,
    Index n
) {
    Index i;

    for (i = 0; i < (Index)n; i++) {
        out[(Index)i] = in1[(Index)i] + in2[(Index)i];
    }
}

void feedbackwriter_02_perform(const SampleValue * input, Index n) {
    auto& buffer = this->feedbacktilde_02_feedbackbuffer;

    for (Index i = 0; i < n; i++) {
        buffer[(Index)i] = input[(Index)i];
    }
}

void stackprotect_perform(Index n) {
    RNBO_UNUSED(n);
    auto __stackprotect_count = this->stackprotect_count;
    __stackprotect_count = 0;
    this->stackprotect_count = __stackprotect_count;
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

number cycle_tilde_02_ph_next(number freq, number reset) {
    {
        {
            if (reset >= 0.)
                this->cycle_tilde_02_ph_currentPhase = reset;
        }
    }

    number pincr = freq * this->cycle_tilde_02_ph_conv;

    if (this->cycle_tilde_02_ph_currentPhase < 0.)
        this->cycle_tilde_02_ph_currentPhase = 1. + this->cycle_tilde_02_ph_currentPhase;

    if (this->cycle_tilde_02_ph_currentPhase > 1.)
        this->cycle_tilde_02_ph_currentPhase = this->cycle_tilde_02_ph_currentPhase - 1.;

    number tmp = this->cycle_tilde_02_ph_currentPhase;
    this->cycle_tilde_02_ph_currentPhase += pincr;
    return tmp;
}

void cycle_tilde_02_ph_reset() {
    this->cycle_tilde_02_ph_currentPhase = 0;
}

void cycle_tilde_02_ph_dspsetup() {
    this->cycle_tilde_02_ph_conv = (number)1 / this->sr;
}

void cycle_tilde_02_dspsetup(bool force) {
    if ((bool)(this->cycle_tilde_02_setupDone) && (bool)(!(bool)(force)))
        return;

    this->cycle_tilde_02_phasei = 0;
    this->cycle_tilde_02_f2i = (number)4294967296 / this->sr;
    this->cycle_tilde_02_wrap = (Int)(this->cycle_tilde_02_buffer->getSize()) - 1;
    this->cycle_tilde_02_setupDone = true;
    this->cycle_tilde_02_ph_dspsetup();
}

void cycle_tilde_02_bufferUpdated() {
    this->cycle_tilde_02_wrap = (Int)(this->cycle_tilde_02_buffer->getSize()) - 1;
}

number cycle_tilde_03_ph_next(number freq, number reset) {
    {
        {
            if (reset >= 0.)
                this->cycle_tilde_03_ph_currentPhase = reset;
        }
    }

    number pincr = freq * this->cycle_tilde_03_ph_conv;

    if (this->cycle_tilde_03_ph_currentPhase < 0.)
        this->cycle_tilde_03_ph_currentPhase = 1. + this->cycle_tilde_03_ph_currentPhase;

    if (this->cycle_tilde_03_ph_currentPhase > 1.)
        this->cycle_tilde_03_ph_currentPhase = this->cycle_tilde_03_ph_currentPhase - 1.;

    number tmp = this->cycle_tilde_03_ph_currentPhase;
    this->cycle_tilde_03_ph_currentPhase += pincr;
    return tmp;
}

void cycle_tilde_03_ph_reset() {
    this->cycle_tilde_03_ph_currentPhase = 0;
}

void cycle_tilde_03_ph_dspsetup() {
    this->cycle_tilde_03_ph_conv = (number)1 / this->sr;
}

void cycle_tilde_03_dspsetup(bool force) {
    if ((bool)(this->cycle_tilde_03_setupDone) && (bool)(!(bool)(force)))
        return;

    this->cycle_tilde_03_phasei = 0;
    this->cycle_tilde_03_f2i = (number)4294967296 / this->sr;
    this->cycle_tilde_03_wrap = (Int)(this->cycle_tilde_03_buffer->getSize()) - 1;
    this->cycle_tilde_03_setupDone = true;
    this->cycle_tilde_03_ph_dspsetup();
}

void cycle_tilde_03_bufferUpdated() {
    this->cycle_tilde_03_wrap = (Int)(this->cycle_tilde_03_buffer->getSize()) - 1;
}

void globaltransport_advance() {}

void globaltransport_dspsetup(bool ) {}

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
    dspexpr_01_in1 = 0;
    dspexpr_01_in2 = 0;
    cycle_tilde_01_frequency = 440;
    cycle_tilde_01_phase_offset = 0;
    dspexpr_02_in1 = 0;
    dspexpr_02_in2 = 0.5;
    dspexpr_03_in1 = 0;
    dspexpr_03_in2 = 0;
    cycle_tilde_02_frequency = 440;
    cycle_tilde_02_phase_offset = 0;
    cycle_tilde_03_frequency = 440;
    cycle_tilde_03_phase_offset = 0;
    dspexpr_04_in1 = 0;
    dspexpr_04_in2 = 0;
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
    didAllocateSignals = 0;
    vs = 0;
    maxvs = 0;
    sr = 44100;
    invsr = 0.000022675736961451248;
    cycle_tilde_01_wrap = 0;
    cycle_tilde_01_ph_currentPhase = 0;
    cycle_tilde_01_ph_conv = 0;
    cycle_tilde_01_setupDone = false;
    feedbacktilde_01_feedbackbuffer = nullptr;
    cycle_tilde_02_wrap = 0;
    cycle_tilde_02_ph_currentPhase = 0;
    cycle_tilde_02_ph_conv = 0;
    cycle_tilde_02_setupDone = false;
    feedbacktilde_02_feedbackbuffer = nullptr;
    cycle_tilde_03_wrap = 0;
    cycle_tilde_03_ph_currentPhase = 0;
    cycle_tilde_03_ph_conv = 0;
    cycle_tilde_03_setupDone = false;
    feedbacktilde_03_feedbackbuffer = nullptr;
    globaltransport_tempo = nullptr;
    globaltransport_state = nullptr;
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

    number dspexpr_01_in1;
    number dspexpr_01_in2;
    number cycle_tilde_01_frequency;
    number cycle_tilde_01_phase_offset;
    number dspexpr_02_in1;
    number dspexpr_02_in2;
    number dspexpr_03_in1;
    number dspexpr_03_in2;
    number cycle_tilde_02_frequency;
    number cycle_tilde_02_phase_offset;
    number cycle_tilde_03_frequency;
    number cycle_tilde_03_phase_offset;
    number dspexpr_04_in1;
    number dspexpr_04_in2;
    MillisecondTime _currentTime;
    ENGINE _internalEngine;
    UInt64 audioProcessSampleCount;
    Index sampleOffsetIntoNextAudioBuffer;
    signal zeroBuffer;
    signal dummyBuffer;
    SampleValue * signals[5];
    bool didAllocateSignals;
    Index vs;
    Index maxvs;
    number sr;
    number invsr;
    SampleBufferRef cycle_tilde_01_buffer;
    Int cycle_tilde_01_wrap;
    UInt32 cycle_tilde_01_phasei;
    SampleValue cycle_tilde_01_f2i;
    number cycle_tilde_01_ph_currentPhase;
    number cycle_tilde_01_ph_conv;
    bool cycle_tilde_01_setupDone;
    signal feedbacktilde_01_feedbackbuffer;
    SampleBufferRef cycle_tilde_02_buffer;
    Int cycle_tilde_02_wrap;
    UInt32 cycle_tilde_02_phasei;
    SampleValue cycle_tilde_02_f2i;
    number cycle_tilde_02_ph_currentPhase;
    number cycle_tilde_02_ph_conv;
    bool cycle_tilde_02_setupDone;
    signal feedbacktilde_02_feedbackbuffer;
    SampleBufferRef cycle_tilde_03_buffer;
    Int cycle_tilde_03_wrap;
    UInt32 cycle_tilde_03_phasei;
    SampleValue cycle_tilde_03_f2i;
    number cycle_tilde_03_ph_currentPhase;
    number cycle_tilde_03_ph_conv;
    bool cycle_tilde_03_setupDone;
    signal feedbacktilde_03_feedbackbuffer;
    signal globaltransport_tempo;
    signal globaltransport_state;
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

