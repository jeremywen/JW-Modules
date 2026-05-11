//
//  _RNBO_PatcherInterface_H_
//
//  Created by Rob Sussman on 8/4/15.
//
//

#ifndef _RNBO_PatcherInterface_H_
#define _RNBO_PatcherInterface_H_

#include "RNBO_Types.h"
#include "RNBO_ProcessInterface.h"
#include "RNBO_PatcherEventTarget.h"
#include "RNBO_EngineLink.h"
#include "RNBO_ProbingInterface.h"
#include "RNBO_PatcherStateInterface.h"
#include "RNBO_EngineInterface.h"

namespace RNBO {

	class DataRef;

	class PatcherInterface : public ProcessInterface,
							 public PatcherEventTarget,
							 public EngineLink,
							 public ProbingInterface
	{
	protected:
		// must be deleted by calling destroy()
		virtual ~PatcherInterface() { }

	public:
		PatcherInterface()
		{ }

		virtual void destroy() = 0;

		// Parameters:
		// - represent state of the patcher that can be set from the Engine
		// - "visible" parameters are made available to plugin environments
		// - Index identifies a parameter but might change when patcher code is regenerated
		// - Index starts at 0 and goes up to getNumParameters() - 1
		// - The parameter name is intended for users to see for plugin environments

		virtual void dump() {}

        // extract the current state from a patcher
        // this will leave the patcher in an UNDEFINED state and most likely unusable
		virtual void extractState(PatcherStateInterface& state)
		{
			RNBO_UNUSED(state);
		}

		void initialize() override
		{
			// you have to overload at least one initialize methods
			// assert(false); // can't use assert in Common
		}

        // initialize the patcehr and apply the given state - take care - the state is
        // in an UNDEFINED state afterwards and not re-usable
		virtual void initialize(PatcherStateInterface& state)
		{
            RNBO_UNUSED(state);
			// the state will only be used if the Patcher overloads this function
			initialize();
		}

		virtual void getPreset(PatcherStateInterface&) {}
		virtual void setPreset(MillisecondTime, PatcherStateInterface&) {}
		virtual DataRefIndex getNumDataRefs() const = 0;
		virtual DataRef* getDataRef(DataRefIndex index) = 0;
		virtual ParameterIndex getNumSignalInParameters() const = 0;
		virtual ParameterIndex getNumSignalOutParameters() const = 0;
		virtual MessageTagInfo resolveTag(MessageTag tag) const = 0;
		virtual MessageIndex getNumMessages() const { return 0; }
		virtual const MessageInfo& getMessageInfo(MessageIndex) const { return NullMessageInfo; }
		virtual Index getMaxBlockSize() const = 0;
		virtual number getSampleRate() const = 0;
		virtual bool hasFixedVectorSize() const = 0;
		virtual MillisecondTime getPatcherTime() const = 0;

	protected:

		virtual void sendParameter(ParameterIndex, bool) {}
        
	};

	/**
	 * @private
	 *
	 * RNBO::default_delete allows putting PatcherInterface into RNBO::UniquePtr
	 */
	template <>
	struct default_delete<RNBO::PatcherInterface>
	{
		void operator() (RNBO::PatcherInterface* pi) const
		{
			pi->destroy();
		}
	};

} // namespace RNBO

#ifndef RNBO_NOSTL

///@cond INTERNAL

// std::default_delete for RNBO::PatcherInterface allows putting PatcherInterface into std::unique_ptr
namespace std
{
	template <>
	struct default_delete<RNBO::PatcherInterface>
	{
		void operator() (RNBO::PatcherInterface* pi) const
		{
			pi->destroy();
		}
	};
}

///@endcond INTERNAL

#endif // RNBO_NOSTL


#endif // #ifndef _RNBO_PatcherInterface_H_
