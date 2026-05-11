//
//  RNBO_Presets.h
//

#ifndef _RNBO_Presets_H_
#define _RNBO_Presets_H_

#ifdef RNBO_NOSTL
#define RNBO_NOPRESETS
#endif // RNBO_NOSTL

#include "RNBO_PatcherState.h"

#ifdef RNBO_NOPRESETS

namespace RNBO {
	using Preset = PatcherState;
	using PresetCallback = void(*);
	using UniquePresetPtr = void*;
    using PresetPtr = std::shared_ptr<Preset>;
    using ConstPresetPtr = std::shared_ptr<const Preset>;

    class DummyPreset : public PatcherState
    {
    public:
        bool isDummy() const override { return true; }
    };
}

#else

#include "RNBO_Debug.h"

RNBO_PUSH_DISABLE_WARNINGS
#include "3rdparty/json/json.hpp"
RNBO_POP_DISABLE_WARNINGS

#include "RNBO_Utils.h"
#include "RNBO_ExternalData.h"

#include "3rdparty/cppcodec/base64_rfc4648.hpp"

using base64 = cppcodec::base64_rfc4648;

/**
 * @file RNBO_Presets.h
 * @brief RNBO_Presets
 */


namespace RNBO {

	using PresetMap = StateMap;

    /**
     * @var typedef RNBO::PatcherState RNBO::Preset
     * @brief a preset is a saved patcher state
     */
	using Preset = PatcherState;

    /**
     * @var typedef std::shared_ptr<Preset> RNBO::PresetPtr
     * @brief an shared pointer to a Preset
     *
     * A reference-counted pointer to a Preset
     */
	using PresetPtr = std::shared_ptr<Preset>;

    /**
     * @var typedef std::function<void(std::shared_ptr<const Preset>)> RNBO::PresetCallback
     * @brief a function to execute when a preset is made available
     */
	using PresetCallback = std::function<void(std::shared_ptr<const Preset>)>;

    /**
     * @var typedef std::shared_ptr<const Preset> RNBO::ConstPresetPtr
     * @brief an shared pointer to a const Preset
     *
     * Used for read-only access to a Preset
     */
	using ConstPresetPtr = std::shared_ptr<const Preset>;

	/**
	 * @brief Associates a PresetPtr and its name
	 */
    struct NamedPresetEntry {
        std::string name;
        PresetPtr preset;
    };

    class DummyPreset : public PatcherState
    {
    public:
        bool isDummy() const override { return true; }
    };

#ifdef RNBO_NOSTL
	using UniquePresetPtr = UniquePtr<Preset>;
#else
    /**
     * @var typedef std::unique_ptr<Preset> RNBO::UniquePresetPtr
     * @brief a unique pointer to a Preset
     *
     * An owning pointer to a Preset
     */
	using UniquePresetPtr = std::unique_ptr<Preset>;
#endif // RNBO_NOSTL

#ifndef RNBO_NOJSONPRESETS

	using Json = nlohmann::json;

	static Json convertPresetToJSONObj(const Preset& preset) {
		Json json;
		for (auto const& entry : preset) {
			const char *key = entry.first.c_str();
			auto type = entry.second.getType();
			switch (type) {
				case ValueHolder::FLOAT: {
					float value = (float)entry.second;
					json[key] = value;
					break;
				}
				case ValueHolder::DOUBLE: {
					double value = (double)entry.second;
					json[key] = value;
					break;
				}
				case ValueHolder::LIST: {
					Json j;
					const list& value = entry.second;
					for (size_t i = 0; i < value.length; i++) {
						j.push_back(value[i]);
					}
					json[key] = j;
					break;
				}
				case ValueHolder::STRING: {
					const char * str = entry.second;
					json[key] = str;
					break;
				}
				case ValueHolder::SUBSTATE: {
					const Preset& subPreset = entry.second;
					json[key] = convertPresetToJSONObj(subPreset);
					break;
				}
				case ValueHolder::SUBSTATEMAP: {
					Index size = entry.second.getSubStateMapSize();
					if (size) {
						Json j;
						for (Index i = 0; i < size; i++) {
							const Preset& subPreset = entry.second[i];
							j.push_back(convertPresetToJSONObj(subPreset));
						}
						json[key] = j;
					}
					break;
				}
				case ValueHolder::UINT32: {
					UInt32 value = (UInt32)entry.second;
					json[key] = value;
					break;
				}
				case ValueHolder::UINT64: {
					UInt64 value = (UInt64)entry.second;
					json[key] = value;
					break;
				}
                case ValueHolder::BUFFER: {
                    SerializedBuffer& buffer = (SerializedBuffer&)entry.second;

                    Json binJson;
                    binJson["binary"] = true;
                    std::string binaryString = base64::encode(buffer.data, buffer.sizeInBytes);
                    binJson["data"] = binaryString;

                    Json typeJson;
                    switch (buffer.type.type) {
                        case DataType::Untyped:
                            typeJson["type"] = "Untyped";
                            break;
                        case DataType::Float32AudioBuffer:
                            typeJson["type"] = "Float32AudioBuffer";
                            break;
                        case DataType::Float64AudioBuffer:
                            typeJson["type"] = "Float64AudioBuffer";
                            break;
                        case DataType::TypedArray:
                            typeJson["type"] = "TypedArray";
                            break;
                        case DataType::SampleAudioBuffer:
                            RNBO_ASSERT(false); // we must not ever have a serialized buffer with undetermined bitness
                            break;
                    }

                    if (buffer.type.type == DataType::Float32AudioBuffer || buffer.type.type == DataType::Float64AudioBuffer) {
                        typeJson["channels"] = buffer.type.audioBufferInfo.channels;
                        typeJson["samplerate"] = buffer.type.audioBufferInfo.samplerate;
                    }

                    binJson["type"] = typeJson;
                    json[key] = binJson;
                    break;
                }
				case ValueHolder::NONE:
				case ValueHolder::EXTERNAL:
				case ValueHolder::EVENTTARGET:
				case ValueHolder::DATAREF:
				case ValueHolder::MULTIREF:
				case ValueHolder::SIGNAL:
				case ValueHolder::BOOLEAN:
				case ValueHolder::INTVALUE:
				default:
					// we do only support numbers, lists and substates
					RNBO_ASSERT(false);
			}
		}

		return json;
	}

	ATTRIBUTE_UNUSED
	static std::string convertPresetToJSON(const Preset& preset) {
		return convertPresetToJSONObj(preset).dump();
	}

	static void convertJSONObjToPreset(Json& json, Preset& preset) {
		for (Json::iterator it = json.begin(); it != json.end(); it++) {
			const char* key = it.key().c_str();
			if (it->is_number()) {
				number value = it.value();
				preset[key] = value;
			}
			else if (it->is_string()) {
				std::string value = it.value();
				preset[key] = value.c_str();
			}
			else if (it->is_array()) {
				Json& j = *it;
				if (j.size() > 0) {
					if (j[0].is_number()) {
						list value;
						for (Index i = 0; i < j.size(); i++) {
							value.push(j[i]);
						}
						preset[key] = value;
					}
					else if (j[0].is_object()) {
						for (Index i = 0; i < j.size(); i++) {
							Preset& subPreset = preset[key][i];
							convertJSONObjToPreset(j[i], subPreset);
						}
					}
				}
			}
			else if (it->is_object()) {
				Json& j = *it;
                if (j.contains("binary") && j["binary"] == true) {
                    SerializedBuffer buffer;

                    if (j.contains("data") && j["data"].is_string()) {
                        std::string data = j["data"];
                        size_t size = base64::decoded_max_size(data.length());

                        buffer.data = (uint8_t *)malloc(size);
                        buffer.sizeInBytes = base64::decode(buffer.data, size, data);
                    }

                    if (j.contains("type") && j["type"].is_object()) {
                        auto& typeJson = j.at("type");
                        std::string type = typeJson["type"];
                        if (type == "Float32AudioBuffer") {
                            buffer.type = Float32AudioBuffer(typeJson["channels"], typeJson["samplerate"]);
                        }
                        else if (type == "Float64AudioBuffer") {
                            buffer.type = Float64AudioBuffer(typeJson["channels"], typeJson["samplerate"]);
                        }
                        else if (type == "TypedArray") {
                            DataType t;
                            t.type = DataType::TypedArray;
                            buffer.type = t;
                        }
                        else {
                            buffer.type = UntypedDataBuffer();
                        }
                    }

                    preset[key] = buffer;
                }
                else {
                    Preset& subPreset = preset.getSubState(key);
                    convertJSONObjToPreset(j, subPreset);
                }
			}
		}
	}

    ATTRIBUTE_UNUSED
    static void convertJSONArrayToPresetList(std::string jsonString, std::vector<std::shared_ptr<NamedPresetEntry>>& presetList) {
		Json json = Json::parse(jsonString);
        for (Json::iterator it = json.begin(); it != json.end(); it++) {
            if (it->is_object()) {
                Json& j = *it;
                std::shared_ptr<NamedPresetEntry> entry(new NamedPresetEntry);
                std::string name = j["name"];
                Json presetPayload = j["preset"];
                entry->name = name;
                PresetPtr preset = std::make_shared<Preset>();
                convertJSONObjToPreset(presetPayload, *preset);
                entry->preset = preset;
                presetList.push_back(entry);
            }
        }
    }

	ATTRIBUTE_UNUSED
	static UniquePresetPtr convertJSONToPreset(std::string jsonString) {
		UniquePresetPtr preset = make_unique<Preset>();
		Json json = Json::parse(jsonString);
		convertJSONObjToPreset(json, *preset);
		return preset;
	}

	ATTRIBUTE_UNUSED
	static void copyPreset(const Preset& src, Preset &dst)
	{
			for (auto const& entry : src) {
					const char *key = entry.first.c_str();
					auto type = entry.second.getType();
					switch (type) {
						case ValueHolder::FLOAT: {
							float value = (float)entry.second;
							dst[key] = value;
							break;
						}
						case ValueHolder::DOUBLE: {
							double value = (double)entry.second;
							dst[key] = value;
							break;
						}
						case ValueHolder::UINT32: {
							UInt32 value = (UInt32)entry.second;
							dst[key] = value;
							break;
						}
						case ValueHolder::UINT64: {
							UInt64 value = (UInt64)entry.second;
							dst[key] = value;
							break;
						}
						case ValueHolder::LIST: {
							Json j;
							const list& srclist = entry.second;
							list dstlist;
							for (Index i = 0; i < srclist.length; i++) {
									dstlist.push(srclist[i]);
							}
							dst[key] = dstlist;
							break;
						}
						case ValueHolder::SUBSTATE: {
							const Preset& preset = entry.second;
							copyPreset(preset, dst[key]);
							break;
						}
						case ValueHolder::SUBSTATEMAP: {
							Index size = entry.second.getSubStateMapSize();
							if (size) {
									for (Index i = 0; i < size; i++) {
											const Preset& preset = entry.second[i];
											Preset& dstSubPreset = dst[key][i];
											copyPreset(preset, dstSubPreset);
									}
							}
							break;
						}
						case ValueHolder::STRING:
							//presetid
							RNBO_ASSERT(strcmp(key, "__presetid") == 0);
							break;
                        case ValueHolder::BUFFER: {
                            SerializedBuffer tmp = (SerializedBuffer&)entry.second;
                            dst[key] = tmp;
                            break;
                        }
						case ValueHolder::NONE:
						case ValueHolder::EXTERNAL:
						case ValueHolder::EVENTTARGET:
						case ValueHolder::DATAREF:
						case ValueHolder::MULTIREF:
						case ValueHolder::SIGNAL:
						case ValueHolder::BOOLEAN:
						case ValueHolder::INTVALUE:
						default:
							// we do only support numbers, lists and substates
							RNBO_ASSERT(false);
							break;
					}
			}
	}

#endif // RNBO_NOJSONPRESETS

} // namespace RNBO

#endif // RNBO_NOPRESETS

#endif  // _RNBO_Presets_H_
