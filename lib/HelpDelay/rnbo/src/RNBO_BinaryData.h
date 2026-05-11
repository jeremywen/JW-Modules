#ifndef RNBO_BINARYDATA_H
#define RNBO_BINARYDATA_H

#include <unordered_map>
#include <string>
#include <tuple>

namespace RNBO {
	class BinaryDataEntry {
		public:
			enum class DataType {
				Encoded, //use file extension
				RawFloat32Interleaved
			};
			BinaryDataEntry() {}
			BinaryDataEntry(const std::string filename, const uint8_t * data, size_t length) : mFileName(filename), mData(data), mLength(length) {}
			BinaryDataEntry(DataType dataType, const std::string filename, const uint8_t * data, size_t length, size_t channels, size_t samplerate) :
				mDataType(dataType), mFileName(filename), mData(data), mLength(length), mChannels(channels), mSampleRate(samplerate) {}
			BinaryDataEntry(const BinaryDataEntry&) = default;

			BinaryDataEntry& operator=(const BinaryDataEntry & other) {
				mDataType = other.mDataType;
				mFileName = other.mFileName;
				mData = other.mData;
				mLength = other.mLength;
				mChannels = other.mChannels;
				mSampleRate = other.mSampleRate;
				return *this;
			}

			const std::string& filename() const {
				return mFileName;
			}

			const uint8_t * data() const {
				return mData;
			}

			std::tuple<const float *, size_t> floatview() const {
				const float * p = nullptr;
				size_t l = 0;
				if (mDataType == DataType::RawFloat32Interleaved) {
					p = reinterpret_cast<const float *>(mData);
					l = mLength / sizeof(float);
				}
				return std::tuple<const float *, size_t> {p, l};
			}

			DataType datetype() const {
				return mDataType;
			}

			size_t length() const {
				return mLength;
			}

			size_t chans() const {
				return mChannels;
			}

			size_t samplerate() const {
				return mSampleRate;
			}

		private:
			DataType mDataType = DataType::Encoded;
			std::string mFileName;
			const uint8_t * mData = nullptr;

			size_t mLength = 0;
			size_t mChannels = 0;
			size_t mSampleRate = 0;
	};

	class BinaryData {
		public:
			//TODO discover keys?
			virtual bool get(const std::string& key, BinaryDataEntry& out) const = 0;
	};

	class BinaryDataImpl : public BinaryData {
		public:
			using Storage = std::unordered_map<std::string, BinaryDataEntry>;
			// storage must live as long as BinaryDataImpl
			BinaryDataImpl(const Storage& storage) : BinaryData(), mStorage(storage) {}
			BinaryDataImpl() : BinaryData(), mStorage(mDummy) {} //empty

			bool get(const std::string& key, BinaryDataEntry& out) const override {
				auto it = mStorage.find(key);
				if (it != mStorage.end()) {
					out = it->second;
					return true;
				}
				return false;
			}

		private:
			const Storage mDummy; // so we can have a reference to something
			const Storage& mStorage;
	};
}

#endif
