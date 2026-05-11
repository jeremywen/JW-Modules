#ifndef _RNBO_PROCESSINTERFACE_H_
#define _RNBO_PROCESSINTERFACE_H_

#include "RNBO_Types.h"
#include "RNBO_BaseInterface.h"

namespace RNBO {

	/**
	 * @brief An interface for signal-rate data processing
	 */
	class ProcessInterface : public BaseInterface {

	protected:

		~ProcessInterface() {}

	public:

		/**
		 * @brief Set the sample rate and vector size before doing any processing
		 *
		 * This method is almost a no-op when there is no change in sample rate or vector size and force is false (the
		 * default) so it is suitable to call before each processing call. Note that an increase of maxBlockSize will
		 * allocate memory, however.
		 *
		 * If this method returns false, do not call process() afterwards. Instead, call prepareToProcess() again.
		 *
		 * @param sampleRate the sample rate to use
		 * @param maxBlockSize the maximum vector size; actual processing could be made in smaller chunks
		 * @param force if true, ensures the dspsetup methods of patcher objects are called
		 * @return false if called during setPatcher, true otherwise
		 */
		virtual void prepareToProcess(number sampleRate, Index maxBlockSize, bool force = false) = 0;

		/**
		 * @brief Process non-interleaved SampleValue* audio buffers with optional MIDI I/O
		 *
		 * @param audioInputs non-interleaved audio inputs (e.g. a 2D array: SampleValues[numInputs][sampleFrames])
		 * @param numInputs the number of audio input buffers
		 * @param audioOutputs non-interleaved audio outputs (e.g. a 2D array: SampleValues[numOutputs][sampleFrames])
		 * @param numOutputs the number of audio output buffers
		 * @param sampleFrames the number of samples in a single input or output
		 * @param midiInput an optional pointer to a MidiEventList of input events associated with this audio vector
		 * @param midiOutput an optional pointer to a MidiEventList which can receive MIDI output generated during the
		 *                   audio vector
		 */
		virtual void process(const SampleValue* const* audioInputs, Index numInputs,
							 SampleValue* const* audioOutputs, Index numOutputs,
							 Index sampleFrames) = 0;

	};

} // namespace RNBO

#endif // #ifndef _RNBO_PROCESSINTERFACE_H_
