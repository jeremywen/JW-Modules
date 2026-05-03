#ifndef _RNBO_BASEINTERFACE_H_
#define _RNBO_BASEINTERFACE_H_

#include "RNBO_Types.h"
#include "RNBO_ParameterInterface.h"

namespace RNBO {

	/**
	 * Base class for objects which, like CoreObject, expose an interface to exported RNBO code.
	 */
	class BaseInterface : public ParameterInterface {

	public:

		/**
		 * Initialize data after construction of the BaseInterface
		 */
		virtual void initialize() {}

		/**
		 * @return the number of MIDI inputs
		 */
		virtual Index getNumMidiInputPorts() const = 0;

		/**
		 * @return the number of MIDI outputs
		 */
		virtual Index getNumMidiOutputPorts() const = 0;

		/**
		 * @return the number of audio input channels processed by the current patcher
		 */
		virtual Index getNumInputChannels() const = 0;

		/**
		 * @return the number of audio output channels processed by the current patcher
		 */
		virtual Index getNumOutputChannels() const = 0;

	protected:

		~BaseInterface() { }

	};

} // namespace RNBO

#endif // #ifndef _RNBO_BASEINTERFACE_H_
