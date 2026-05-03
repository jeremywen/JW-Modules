#ifndef RNBO_ParameterInterface_h
#define RNBO_ParameterInterface_h

#include "RNBO_Types.h"
#include "RNBO_Math.h"

namespace RNBO {

	class ParameterInterface {
		
	protected:
		~ParameterInterface() { }

	public:

		/**
		 * @brief Get the number of visible parameters
		 *
		 * Note that not all parameters are visible (for example, params in gen~ are not automatically exposed as RNBO
		 * parameters). The count of visible parameters is usually the same as the number of param/param~ objects in
		 * a RNBO~ patch.
		 *
		 * @return the number of visible parameters
		 */
		virtual ParameterIndex getNumParameters() const = 0;

		/**
		 * @brief Get the parameter name (may not be unique)
		 *
		 * @param index the parameter index
		 * @return a C-style string containing the parameter name
		 */
		virtual ConstCharPointer getParameterName(ParameterIndex index) const = 0;

		/**
		 * @brief Get the unique ID of a parameter
		 *
		 * The unique ID of a parameter can be the same as the parameter ID.
		 * If a parameter is nested in a subpatcher, a prefix is added to the
		 * parameter name in order to disambiguate. Examples of parameter IDs:
		 *   - "my_toplevel_param"
		 *   - "p_obj-2/my_nested_param"
		 *   - "poly/p_obj-18/my_nested_poly_param"
		 *
		 * @param index the parameter index
		 * @return a C-style string containing the parameter ID
		 */
		virtual ConstCharPointer getParameterId(ParameterIndex index) const = 0;

		/**
		 * @brief Get detailed information about a parameter
		 *
		 * @param index the parameter index
		 * @param info a pointer to a ParameterInfo object to fill
		 */
		virtual void getParameterInfo(ParameterIndex index, ParameterInfo* info) const = 0;

		/**
		 * @param index the parameter index
		 * @return the value of the parameter
		 */
		virtual ParameterValue getParameterValue(ParameterIndex index) = 0;

		/**
		 * @brief Set a parameter's normalized [0..1] value
		 *
		 * @param index the parameter index
		 * @param normalizedValue parameter normalized value to set
		 * @param time when the parameter change should happen (default is RNBOTimeNow)
		 */
		virtual void setParameterValue(ParameterIndex index, ParameterValue value, MillisecondTime time = RNBOTimeNow) = 0;

		/**
		 * @brief Get a parameter's normalized [0..1] value
		 *
		 * @param index the parameter index
		 */
		virtual ParameterValue getParameterNormalized(ParameterIndex index) {
			return convertToNormalizedParameterValue(index, getParameterValue(index));
		}

		/**
		 * @brief Set a parameter's normalized [0..1] value
		 *
		 * @param index the parameter index
		 * @param normalizedValue parameter normalized value to set
		 * @param time when the parameter change should happen (default is RNBOTimeNow)
		 */
		virtual void setParameterValueNormalized(ParameterIndex index, ParameterValue normalizedValue, MillisecondTime time = RNBOTimeNow) {
			setParameterValue(index, convertFromNormalizedParameterValue(index, normalizedValue), time);
		}

		/**
		 * @brief Convert a parameter value from its real value to a normalized representation [0..1]
		 *
		 * @param index the parameter index
		 * @param value the real value of the parameter
		 * @return a normalized ParameterValue
		 */
		virtual ParameterValue convertToNormalizedParameterValue(ParameterIndex index, ParameterValue value) const = 0;

		/**
		 * @brief Convert a parameter value from a normalized representation [0..1] to its real value
		 *
		 * @param index the parameter index
		 * @param normalizedValue the normalized value
		 * @return the real value of the parameter
		 */
		virtual ParameterValue convertFromNormalizedParameterValue(ParameterIndex index, ParameterValue normalizedValue) const = 0;

		/**
		 * @brief Apply and get the constrained parameter value
		 *
		 * Parameter constraints include a minimum/maximum value and step constraints
		 *
		 * @param index the parameter index
		 * @param value an unconstrained parameter value
		 * @return the constrained parameter value
		 */
		virtual ParameterValue constrainParameterValue(ParameterIndex, ParameterValue value) const { return value; }
	};

} // namespace RNBO

#endif // RNBO_ParameterInterface_h
