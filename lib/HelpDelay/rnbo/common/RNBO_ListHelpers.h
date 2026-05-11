#ifndef _RNBO_LISTHELPERS_H_
#define _RNBO_LISTHELPERS_H_

// RNBO_LISTHELPERS.h -- list utilities used by generated code

#include "RNBO_Std.h"

namespace RNBO {

    template<class T, size_t N> class array;


	ATTRIBUTE_UNUSED
	static list createListCopy(const list& l)
	{
		list tmp = l;
		return tmp;
	}

	/*
	* assume this statement:
	*
	* list newlist = oldlist;
	*
	* in C++ this will make a list copy, in JS this won't, so we need the below function (a NOOP in C++)
	*/
	template<typename L> const L& jsCreateListCopy(const L& l)
	{
		return l;
	}

    template<typename T> void flattenArray(T value, list& result) {
        result.push(value);
    }

    template<typename T, size_t N> void flattenArray(RNBO::array<T, N>& array, list& result) {
        for (size_t i = 0; i < array.size(); i++) {
            flattenArray(array[i], result);
        }
    }

    template <typename T, size_t N> list serializeSafeArrayToList(RNBO::array<T, N>& array, size_t size) {
		list result;
		result.reserve(size);
        flattenArray(array, result);
		return result;
	}

    template <typename T> list serializeArrayToList(T *array, size_t size) {
        list result;
        result.reserve(size);
        for (size_t i = 0; i < size; i++) {
            result.push((number)(array[i]));
        }
        return result;
    }

    template <typename T> void unflattenArray(const list& list, size_t& offset, T& result) {
        if (offset < list.length)
            result = list[offset];
        else
            result = 0;
        offset++;
    }

    template <typename T, size_t N> void unflattenArray(const list& l, size_t& offset, RNBO::array<T, N>& result) {
        for (size_t i = 0; i < result.size(); i++) {
            unflattenArray(l, offset, result[i]);
        }
    }

	template <typename T, size_t N> void deserializeSafeArrayFromList(const list& l, RNBO::array<T, N>& result, size_t size) {
        RNBO_UNUSED(size)
        size_t offset = 0;
		for (size_t i = 0; i < result.size(); i++) {
            unflattenArray(l, offset, result[i]);
		}
	}

    template <typename T> void deserializeArrayFromList(const list& l, T* result, size_t size) {
        for (size_t i = 0; i < size && i < l.length; i++) {
            result[i] = (T)(l[i]);
        }
    }

} // namespace RNBO

#endif // #ifndef _RNBO_LISTHELPERS_H_
