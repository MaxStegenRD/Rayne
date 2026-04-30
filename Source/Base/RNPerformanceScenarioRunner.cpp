//
//  RNPerformanceScenarioRunner.cpp
//  Rayne
//
//  Copyright 2026 by Überpixel. All rights reserved.
//

#include "RNPerformanceScenarioRunner.h"
#include "../Debug/RNLogger.h"
#include "../Math/RNQuaternion.h"
#include "../Objects/RNArray.h"
#include "../Objects/RNJSONSerialization.h"
#include "../Objects/RNNumber.h"
#include "../Scene/RNSceneNode.h"
#include "../System/RNFileManager.h"
#include "RNKernel.h"

namespace RN
{
	static PerformanceScenarioRunner *__sharedPerformanceScenarioRunner = nullptr;

	static std::string StringToKey(const String *string)
	{
		if(!string) return std::string();
		return std::string(string->GetUTF8String());
	}

	static String *GetStringValue(const Dictionary *dictionary, const char *key)
	{
		if(!dictionary) return nullptr;
		return dictionary->GetObjectForKey<String>(String::WithString(key, true));
	}

	static Dictionary *GetDictionaryValue(const Dictionary *dictionary, const char *key)
	{
		if(!dictionary) return nullptr;
		return dictionary->GetObjectForKey<Dictionary>(String::WithString(key, true));
	}

	static Object *GetObjectValue(const Dictionary *dictionary, const char *key)
	{
		if(!dictionary) return nullptr;
		return dictionary->GetObjectForKey<Object>(String::WithString(key, true));
	}

	static Array *GetArrayValue(const Dictionary *dictionary, const char *key)
	{
		if(!dictionary) return nullptr;
		return dictionary->GetObjectForKey<Array>(String::WithString(key, true));
	}

	static Number *GetNumberValue(const Dictionary *dictionary, const char *key)
	{
		if(!dictionary) return nullptr;
		return dictionary->GetObjectForKey<Number>(String::WithString(key, true));
	}

	static float GetFloatValue(const Dictionary *dictionary, const char *key, float defaultValue)
	{
		Number *number = GetNumberValue(dictionary, key);
		return number? number->GetFloatValue() : defaultValue;
	}

	static bool HasNumberValue(const Dictionary *dictionary, const char *key)
	{
		return GetNumberValue(dictionary, key) != nullptr;
	}

	static double GetKernelTime()
	{
		Kernel *kernel = Kernel::GetSharedInstance();
		return kernel? kernel->GetTotalTime() : 0.0;
	}

	static bool ReadVector3(const Dictionary *dictionary, const char *key, Vector3 &vector)
	{
		Array *array = GetArrayValue(dictionary, key);
		if(!array || array->GetCount() < 3)
			return false;

		Number *x = array->GetObjectAtIndex<Number>(0);
		Number *y = array->GetObjectAtIndex<Number>(1);
		Number *z = array->GetObjectAtIndex<Number>(2);
		if(!x || !y || !z)
			return false;

		vector = Vector3(x->GetFloatValue(), y->GetFloatValue(), z->GetFloatValue());
		return true;
	}

	static bool ReadQuaternion(const Dictionary *dictionary, Quaternion &rotation)
	{
		Vector3 euler;
		if(ReadVector3(dictionary, "rotationEuler", euler))
		{
			rotation = Quaternion(euler);
			return true;
		}

		Array *array = GetArrayValue(dictionary, "rotation");
		if(!array || array->GetCount() < 4)
			return false;

		Number *x = array->GetObjectAtIndex<Number>(0);
		Number *y = array->GetObjectAtIndex<Number>(1);
		Number *z = array->GetObjectAtIndex<Number>(2);
		Number *w = array->GetObjectAtIndex<Number>(3);
		if(!x || !y || !z || !w)
			return false;

		rotation = Quaternion(x->GetFloatValue(), y->GetFloatValue(), z->GetFloatValue(), w->GetFloatValue());
		rotation.Normalize();
		return true;
	}

	static String *GetCollectorName(Object *collector)
	{
		if(!collector)
			return nullptr;

		String *name = collector->Downcast<String>();
		if(name)
			return name;

		Dictionary *dictionary = collector->Downcast<Dictionary>();
		return GetStringValue(dictionary, "name");
	}

	static Array *GetCollectorsForAction(const PerformanceScenario *scenario, const Dictionary *action)
	{
		Array *collectors = GetArrayValue(action, "collectors");
		if(collectors)
			return collectors;

		return scenario? scenario->GetCollectors() : nullptr;
	}

	static bool ValidateCollectors(Array *collectors, const char *&message)
	{
		if(!collectors || collectors->GetCount() == 0)
		{
			message = "performance capture action has no collectors";
			return false;
		}

		bool isValid = true;
		collectors->Enumerate([&](Object *object, size_t index, bool &stop) {
			if(!GetCollectorName(object))
			{
				message = "performance collector is missing name";
				isValid = false;
				stop = true;
			}

			(void)index;
		});

		return isValid;
	}

	static bool IsCaptureCommand(const std::string &command)
	{
		return command == "prepareCapture" || command == "startCapture" || command == "stopCapture";
	}

	static void SkipIgnoredActionLists(const Array *actions, size_t &index)
	{
		while(index < actions->GetCount())
		{
			Object *object = actions->GetObjectAtIndex<Object>(index);
			if(!object || !object->Downcast<Array>())
				return;

			index++;
		}
	}

	static std::string EncodeMarkerValue(const char *value)
	{
		if(!value)
			return std::string();

		std::string result;
		static const char *hex = "0123456789ABCDEF";
		for(const char *iterator = value; *iterator; iterator++)
		{
			uint8 character = static_cast<uint8>(*iterator);
			if((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9') || character == '-' || character == '_' || character == '.' || character == '/' || character == ':')
			{
				result.push_back(static_cast<char>(character));
			}
			else
			{
				result.push_back('%');
				result.push_back(hex[(character >> 4) & 0x0f]);
				result.push_back(hex[character & 0x0f]);
			}
		}

		return result;
	}

	static bool GetMarkerValue(Object *object, std::string &value)
	{
		String *string = object? object->Downcast<String>() : nullptr;
		if(string)
		{
			value = EncodeMarkerValue(string->GetUTF8String());
			return true;
		}

		Number *number = object? object->Downcast<Number>() : nullptr;
		if(!number)
			return false;

		if(number->GetType() == Number::Type::Boolean)
		{
			value = number->GetBoolValue()? "true" : "false";
			return true;
		}

		value = EncodeMarkerValue(RNSTR(number->GetDoubleValue())->GetUTF8String());
		return true;
	}

	RNDefineMeta(PerformanceScenario, Object)

	PerformanceScenario::PerformanceScenario(Dictionary *dictionary) :
		_dictionary(SafeRetain(dictionary))
	{}

	PerformanceScenario::~PerformanceScenario()
	{
		SafeRelease(_dictionary);
	}

	const String *PerformanceScenario::GetIdentifier() const
	{
		String *identifier = GetStringValue(_dictionary, "identifier");
		return identifier? identifier : RNCSTR("unnamed");
	}

	const String *PerformanceScenario::GetDescription() const
	{
		String *description = GetStringValue(_dictionary, "description");
		return description? description : GetIdentifier();
	}

	Dictionary *PerformanceScenario::GetDeviceConfiguration() const
	{
		return GetDictionaryValue(_dictionary, "device");
	}

	bool PerformanceScenario::GetDeviceBoolValue(const String *key, bool defaultValue) const
	{
		Dictionary *device = GetDeviceConfiguration();
		if(!device || !key)
			return defaultValue;

		Number *number = device->GetObjectForKey<Number>(key);
		return number? number->GetBoolValue() : defaultValue;
	}

	float PerformanceScenario::GetDeviceFloatValue(const String *key, float defaultValue) const
	{
		Dictionary *device = GetDeviceConfiguration();
		if(!device || !key)
			return defaultValue;

		Number *number = device->GetObjectForKey<Number>(key);
		return number? number->GetFloatValue() : defaultValue;
	}

	int32 PerformanceScenario::GetDeviceInt32Value(const String *key, int32 defaultValue) const
	{
		Dictionary *device = GetDeviceConfiguration();
		if(!device || !key)
			return defaultValue;

		Number *number = device->GetObjectForKey<Number>(key);
		return number? number->GetInt32Value() : defaultValue;
	}

	Array *PerformanceScenario::GetCollectors() const
	{
		return GetArrayValue(_dictionary, "collectors");
	}

	Array *PerformanceScenario::GetActions() const
	{
		return GetArrayValue(_dictionary, "actions");
	}

	PerformanceScenarioRunner::PerformanceScenarioRunner() :
		_scenario(nullptr),
		_state(State::Disabled),
		_commandIndex(0),
		_commandElapsed(0.0f),
		_scenarioStartTime(0.0),
		_activeCaptureIndex(0),
		_hasActiveCapture(false),
		_isActiveCaptureRunning(false)
	{
		__sharedPerformanceScenarioRunner = this;
		RegisterBuiltinCommands();
	}

	PerformanceScenarioRunner::~PerformanceScenarioRunner()
	{
		ClearScenario();

		for(auto &pair : _targets)
		{
			SafeRelease(pair.second);
		}
		_targets.clear();

		__sharedPerformanceScenarioRunner = nullptr;
	}

	PerformanceScenarioRunner *PerformanceScenarioRunner::GetSharedInstance()
	{
		return __sharedPerformanceScenarioRunner;
	}

	bool PerformanceScenarioRunner::LoadDefaultScenario()
	{
		String *externalPath = GetDefaultScenarioPath(true);
		if(FileManager::PathExists(externalPath))
			return LoadScenarioFromFile(externalPath);

		String *internalPath = GetDefaultScenarioPath(false);
		if(FileManager::PathExists(internalPath))
			return LoadScenarioFromFile(internalPath);

		return false;
	}

	bool PerformanceScenarioRunner::LoadScenarioFromFile(const String *path)
	{
		if(!path || !FileManager::PathExists(path))
			return false;

		try
		{
			Data *data = Data::WithContentsOfFile(path);
			Dictionary *dictionary = JSONSerialization::ObjectFromData<Dictionary>(data, 0);
			return LoadScenario(dictionary);
		}
		catch(Exception &e)
		{
			RNWarning("Failed loading performance scenario: " << e);
		}

		return false;
	}

	bool PerformanceScenarioRunner::LoadScenario(Dictionary *dictionary)
	{
		if(!dictionary)
			return false;

		const char *validationMessage = nullptr;
		if(!ValidateScenario(dictionary, validationMessage))
		{
			RNWarning("Failed loading performance scenario: " << (validationMessage? validationMessage : "invalid scenario"));
			return false;
		}

		ClearScenario();

		_scenario = new PerformanceScenario(dictionary);
		_scenarioStartTime = GetKernelTime();
		_actionRecords.clear();
		_captureRecords.clear();
		_activeCaptureIndex = 0;
		_hasActiveCapture = false;
		_isActiveCaptureRunning = false;

		BeginState(State::Running);
		RNInfo("RN_PERF_SCENARIO_LOADED identifier=" << _scenario->GetIdentifier());
		RNInfo("RN_PERF_SCENARIO_START identifier=" << _scenario->GetIdentifier());
		return true;
	}

	void PerformanceScenarioRunner::ClearScenario()
	{
		SafeRelease(_scenario);
		_scenario = nullptr;
		_state = State::Disabled;
		_commandIndex = 0;
		_commandElapsed = 0.0f;
		_actionRecords.clear();
		_captureRecords.clear();
		_activeCaptureIndex = 0;
		_hasActiveCapture = false;
		_isActiveCaptureRunning = false;
	}

	void PerformanceScenarioRunner::RegisterCommand(const String *name, const CommandCallback &callback)
	{
		if(!name || !callback)
			return;

		_commands[StringToKey(name)] = callback;
	}

	void PerformanceScenarioRunner::UnregisterCommand(const String *name)
	{
		if(!name)
			return;

		_commands.erase(StringToKey(name));
	}

	void PerformanceScenarioRunner::RegisterCondition(const String *name, const ConditionCallback &callback)
	{
		if(!name || !callback)
			return;

		_conditions[StringToKey(name)] = callback;
	}

	void PerformanceScenarioRunner::UnregisterCondition(const String *name)
	{
		if(!name)
			return;

		_conditions.erase(StringToKey(name));
	}

	void PerformanceScenarioRunner::RegisterTarget(const String *name, Object *target)
	{
		if(!name || !target)
			return;

		std::string key = StringToKey(name);
		auto iterator = _targets.find(key);
		if(iterator != _targets.end())
		{
			SafeRelease(iterator->second);
		}

		_targets[key] = SafeRetain(target);
	}

	void PerformanceScenarioRunner::UnregisterTarget(const String *name)
	{
		if(!name)
			return;

		std::string key = StringToKey(name);
		auto iterator = _targets.find(key);
		if(iterator == _targets.end())
			return;

		SafeRelease(iterator->second);
		_targets.erase(iterator);
	}

	Object *PerformanceScenarioRunner::GetTarget(const String *name) const
	{
		if(!name)
			return nullptr;

		auto iterator = _targets.find(StringToKey(name));
		if(iterator == _targets.end())
			return nullptr;

		return iterator->second;
	}

	bool PerformanceScenarioRunner::EvaluateCondition(const String *name, const PerformanceCommandContext &context) const
	{
		if(!name)
			return false;

		auto iterator = _conditions.find(StringToKey(name));
		if(iterator == _conditions.end())
			return false;

		return iterator->second(context);
	}

	void PerformanceScenarioRunner::Update(float delta)
	{
		if(!_scenario || _state == State::Disabled || _state == State::Completed || _state == State::Failed)
			return;

		switch(_state)
		{
			case State::Running:
				if(RunActions(delta))
				{
					Complete();
				}
				break;

			default:
				break;
		}
	}

	void PerformanceScenarioRunner::MarkCapturePrepare(const Dictionary *action)
	{
		if(!_scenario)
			return;

		if(_hasActiveCapture)
		{
			Fail("prepareCapture requires the previous capture to stop first");
			return;
		}

		CaptureRecord &record = BeginCaptureRecord(action);
		record.prepareTime = GetKernelTime();
		record.didPrepare = true;
		EmitCaptureMarker("RN_PERF_CAPTURE_PREPARE", action, record.index);
	}

	void PerformanceScenarioRunner::MarkCaptureStart(const Dictionary *action)
	{
		if(!_scenario)
			return;

		if(_hasActiveCapture && _isActiveCaptureRunning)
		{
			Fail("startCapture requires the previous capture to stop first");
			return;
		}

		CaptureRecord &record = _hasActiveCapture? _captureRecords[_activeCaptureIndex] : BeginCaptureRecord(action);
		UpdateCaptureRecordName(record, action);
		record.startTime = GetKernelTime();
		record.didStart = true;
		_isActiveCaptureRunning = true;
		EmitCaptureMarker("RN_PERF_CAPTURE_START", action, record.index);
	}

	void PerformanceScenarioRunner::MarkCaptureEnd(const Dictionary *action)
	{
		if(!_scenario)
			return;

		if(!_hasActiveCapture || !_isActiveCaptureRunning)
		{
			Fail("stopCapture requires a preceding startCapture");
			return;
		}

		CaptureRecord &record = _captureRecords[_activeCaptureIndex];
		UpdateCaptureRecordName(record, action);
		record.endTime = GetKernelTime();
		record.didEnd = true;
		EmitCaptureMarker("RN_PERF_CAPTURE_END", action, record.index);

		_hasActiveCapture = false;
		_isActiveCaptureRunning = false;
	}

	void PerformanceScenarioRunner::Fail(const char *message)
	{
		if(!_scenario || _state == State::Failed)
			return;

		_state = State::Failed;
		RNError("RN_PERF_FAILED identifier=" << _scenario->GetIdentifier() << " message=" << (message? message : "unknown"));
		WriteResult("failed", message);
	}

	void PerformanceScenarioRunner::RegisterBuiltinCommands()
	{
		RegisterCommand(RNCSTR("waitSeconds"), [](PerformanceCommandContext &context) {
			float seconds = GetFloatValue(context.GetCommand(), "seconds", 0.0f);
			return (context.GetElapsed() >= seconds)? PerformanceCommandResult::Succeeded() : PerformanceCommandResult::Running();
		});

		RegisterCommand(RNCSTR("waitFor"), [](PerformanceCommandContext &context) {
			String *condition = GetStringValue(context.GetCommand(), "condition");
			if(!condition)
				return PerformanceCommandResult::Failed("waitFor command is missing condition");

			if(context.EvaluateCondition(condition))
				return PerformanceCommandResult::Succeeded();

			return PerformanceCommandResult::Running();
		});

		RegisterCommand(RNCSTR("setNodePose"), [](PerformanceCommandContext &context) {
			String *targetName = GetStringValue(context.GetCommand(), "target");
			if(!targetName)
				return PerformanceCommandResult::Failed("setNodePose command is missing target");

			SceneNode *target = context.GetTarget<SceneNode>(targetName);
			if(!target)
				return PerformanceCommandResult::Failed("setNodePose command target was not registered");

			Vector3 position;
			if(ReadVector3(context.GetCommand(), "position", position))
				target->SetWorldPosition(position);

			Quaternion rotation;
			if(ReadQuaternion(context.GetCommand(), rotation))
				target->SetWorldRotation(rotation);

			return PerformanceCommandResult::Succeeded();
		});

		RegisterCommand(RNCSTR("setDeviceSettings"), [](PerformanceCommandContext &context) {
			(void)context;
			return PerformanceCommandResult::Succeeded();
		});

		RegisterCommand(RNCSTR("prepareCapture"), [](PerformanceCommandContext &context) {
			context.GetRunner()->MarkCapturePrepare(context.GetCommand());
			return PerformanceCommandResult::Succeeded();
		});

		RegisterCommand(RNCSTR("startCapture"), [](PerformanceCommandContext &context) {
			context.GetRunner()->MarkCaptureStart(context.GetCommand());
			return PerformanceCommandResult::Succeeded();
		});

		RegisterCommand(RNCSTR("stopCapture"), [](PerformanceCommandContext &context) {
			context.GetRunner()->MarkCaptureEnd(context.GetCommand());
			return PerformanceCommandResult::Succeeded();
		});

		RegisterCommand(RNCSTR("logMarker"), [](PerformanceCommandContext &context) {
			String *marker = GetStringValue(context.GetCommand(), "marker");
			if(!marker)
				return PerformanceCommandResult::Failed("logMarker command is missing marker");

			RNInfo("RN_PERF_MARKER marker=" << marker->GetUTF8String());
			return PerformanceCommandResult::Succeeded();
		});
	}

	bool PerformanceScenarioRunner::RunActions(float delta)
	{
		Array *commands = _scenario? _scenario->GetActions() : nullptr;
		if(!commands)
			return true;

		SkipIgnoredActionLists(commands, _commandIndex);
		if(_commandIndex >= commands->GetCount())
			return true;

		Dictionary *command = commands->GetObjectAtIndex<Dictionary>(_commandIndex);
		if(!command)
		{
			Fail("performance command is not a dictionary");
			return false;
		}

		String *commandName = GetStringValue(command, "command");
		if(!commandName)
		{
			Fail("performance command is missing command name");
			return false;
		}

		if(_commandElapsed == 0.0f)
			BeginAction(command);

		auto iterator = _commands.find(StringToKey(commandName));
		if(iterator == _commands.end())
		{
			RNError("RN_PERF_FAILED missing_command=" << commandName);
			EndAction("failed", "performance command is not registered");
			Fail("performance command is not registered");
			return false;
		}

		_commandElapsed += delta;
		PerformanceCommandContext context(this, command, delta, _commandElapsed);
		PerformanceCommandResult result = iterator->second(context);

		if(result.GetState() == PerformanceCommandResult::State::Running)
		{
			Number *timeout = GetNumberValue(command, "timeoutSeconds");
			if(timeout && _commandElapsed >= timeout->GetFloatValue())
			{
				EndAction("failed", "performance action timed out");
				Fail("performance action timed out");
				return false;
			}
		}

		switch(result.GetState())
		{
			case PerformanceCommandResult::State::Running:
				return false;

			case PerformanceCommandResult::State::Succeeded:
				EndAction("passed");
				_commandIndex++;
				_commandElapsed = 0.0f;
				SkipIgnoredActionLists(commands, _commandIndex);
				return _commandIndex >= commands->GetCount();

			case PerformanceCommandResult::State::Failed:
				EndAction("failed", result.GetMessage());
				Fail(result.GetMessage());
				return false;
		}

		return false;
	}

	bool PerformanceScenarioRunner::ValidateScenario(Dictionary *dictionary, const char *&message) const
	{
		Object *collectorsObject = GetObjectValue(dictionary, "collectors");
		Array *scenarioCollectors = GetArrayValue(dictionary, "collectors");
		if(collectorsObject && !scenarioCollectors)
		{
			message = "performance scenario collectors must be an array";
			return false;
		}
		if(scenarioCollectors && !ValidateCollectors(scenarioCollectors, message))
			return false;

		Array *actions = GetArrayValue(dictionary, "actions");
		if(!actions)
		{
			message = "performance scenario is missing actions";
			return false;
		}

		if(actions->GetCount() == 0)
		{
			message = "performance scenario actions is empty";
			return false;
		}

		bool isValid = true;
		bool hasOpenCapture = false;
		bool isCaptureRunning = false;
		actions->Enumerate([&](Object *object, size_t index, bool &stop) {
			if(object && object->Downcast<Array>())
				return;

			Dictionary *action = object->Downcast<Dictionary>();
			if(!ValidateAction(action, index, message))
			{
				isValid = false;
				stop = true;
				return;
			}

			std::string command = StringToKey(GetStringValue(action, "command"));
			if(IsCaptureCommand(command))
			{
				Object *actionCollectorsObject = GetObjectValue(action, "collectors");
				Array *actionCollectors = GetArrayValue(action, "collectors");
				if(actionCollectorsObject && !actionCollectors)
				{
					message = "performance capture action collectors must be an array";
					isValid = false;
					stop = true;
					return;
				}

				if(!ValidateCollectors(actionCollectors? actionCollectors : scenarioCollectors, message))
				{
					isValid = false;
					stop = true;
					return;
				}
			}

			if(command == "prepareCapture")
			{
				if(hasOpenCapture)
				{
					message = isCaptureRunning? "prepareCapture requires the previous capture to stop first" : "performance scenario has duplicate prepareCapture before startCapture";
					isValid = false;
					stop = true;
					return;
				}

				hasOpenCapture = true;
				isCaptureRunning = false;
			}
			else if(command == "startCapture")
			{
				if(hasOpenCapture && isCaptureRunning)
				{
					message = "startCapture requires the previous capture to stop first";
					isValid = false;
					stop = true;
					return;
				}

				hasOpenCapture = true;
				isCaptureRunning = true;
			}
			else if(command == "stopCapture")
			{
				if(!hasOpenCapture || !isCaptureRunning)
				{
					message = "stopCapture requires a preceding startCapture";
					isValid = false;
					stop = true;
					return;
				}

				hasOpenCapture = false;
				isCaptureRunning = false;
			}
		});

		if(isValid && hasOpenCapture)
		{
			message = isCaptureRunning? "startCapture requires a following stopCapture" : "prepareCapture requires a following startCapture";
			return false;
		}

		return isValid;
	}

	bool PerformanceScenarioRunner::ValidateAction(Dictionary *action, size_t index, const char *&message) const
	{
		if(!action)
		{
			message = "performance action is not a dictionary";
			return false;
		}

		String *command = GetStringValue(action, "command");
		if(!command)
		{
			message = "performance action is missing command";
			return false;
		}

		Number *timeout = GetNumberValue(action, "timeoutSeconds");
		if(timeout && timeout->GetFloatValue() < 0.0f)
		{
			message = "performance action has invalid timeoutSeconds";
			return false;
		}

		std::string commandKey = StringToKey(command);
		if(commandKey == "waitSeconds")
		{
			if(!HasNumberValue(action, "seconds") || GetFloatValue(action, "seconds", -1.0f) < 0.0f)
			{
				message = "waitSeconds action has invalid seconds";
				return false;
			}
		}
		else if(commandKey == "waitFor")
		{
			if(!GetStringValue(action, "condition"))
			{
				message = "waitFor action is missing condition";
				return false;
			}
		}
		else if(commandKey == "setNodePose")
		{
			if(!GetStringValue(action, "target"))
			{
				message = "setNodePose action is missing target";
				return false;
			}

			Vector3 position;
			Quaternion rotation;
			if(!ReadVector3(action, "position", position) && !ReadQuaternion(action, rotation))
			{
				message = "setNodePose action is missing position or rotation";
				return false;
			}
		}
		else if(commandKey == "logMarker")
		{
			if(!GetStringValue(action, "marker"))
			{
				message = "logMarker action is missing marker";
				return false;
			}
		}

		(void)index;
		return true;
	}

	PerformanceScenarioRunner::CaptureRecord &PerformanceScenarioRunner::BeginCaptureRecord(const Dictionary *action)
	{
		CaptureRecord record;
		record.index = _captureRecords.size();
		record.name = StringToKey(GetStringValue(action, "name"));
		record.prepareTime = 0.0;
		record.startTime = 0.0;
		record.endTime = 0.0;
		record.didPrepare = false;
		record.didStart = false;
		record.didEnd = false;

		_captureRecords.push_back(record);
		_activeCaptureIndex = record.index;
		_hasActiveCapture = true;
		_isActiveCaptureRunning = false;

		return _captureRecords.back();
	}

	void PerformanceScenarioRunner::UpdateCaptureRecordName(CaptureRecord &record, const Dictionary *action) const
	{
		if(!record.name.empty())
			return;

		record.name = StringToKey(GetStringValue(action, "name"));
	}

	void PerformanceScenarioRunner::EmitCaptureMarker(const char *marker, const Dictionary *action, size_t captureIndex) const
	{
		if(!_scenario || !marker)
			return;

		Array *collectors = GetCollectorsForAction(_scenario, action);
		if(!collectors)
			return;

		collectors->Enumerate([&](Object *object, size_t index, bool &stop) {
			String *collector = GetCollectorName(object);
			if(collector)
			{
				StringBuilder line;
				line << marker << " identifier=" << _scenario->GetIdentifier() << " captureIndex=" << captureIndex << " collector=" << EncodeMarkerValue(collector->GetUTF8String());

				Dictionary *dictionary = object->Downcast<Dictionary>();
				if(dictionary)
				{
					dictionary->Enumerate([&](Object *value, const Object *key, bool &metadataStop) {
						const String *keyString = key->Downcast<String>();
						if(!keyString || String::AreEqual(keyString, RNCSTR("name")))
							return;

						std::string markerValue;
						if(GetMarkerValue(value, markerValue))
							line << " " << keyString->GetUTF8String() << "=" << markerValue;

						(void)metadataStop;
					});
				}

				RNInfo(line.Build());
			}

			(void)index;
			(void)stop;
		});
	}

	void PerformanceScenarioRunner::BeginAction(const Dictionary *action)
	{
		ActionRecord record;
		record.index = _commandIndex;
		record.command = StringToKey(GetStringValue(action, "command"));
		record.name = StringToKey(GetStringValue(action, "name"));
		record.status = "running";
		record.startTime = GetKernelTime();
		record.endTime = 0.0;

		_actionRecords.push_back(record);
	}

	void PerformanceScenarioRunner::EndAction(const char *status, const char *message)
	{
		if(_actionRecords.empty())
			return;

		ActionRecord &record = _actionRecords.back();
		record.status = status? status : "unknown";
		if(message)
			record.message = message;
		record.endTime = GetKernelTime();
	}

	void PerformanceScenarioRunner::BeginState(State state)
	{
		_state = state;
		_commandIndex = 0;
		_commandElapsed = 0.0f;
	}

	void PerformanceScenarioRunner::Complete()
	{
		if(!_scenario)
			return;

		_state = State::Completed;
		RNInfo("RN_PERF_SCENARIO_END identifier=" << _scenario->GetIdentifier());
		RNInfo("RN_PERF_COMPLETED identifier=" << _scenario->GetIdentifier());
		WriteResult("passed");
	}

	void PerformanceScenarioRunner::WriteResult(const char *status, const char *message) const
	{
		if(!_scenario)
			return;

		Dictionary *result = new Dictionary();
		String *identifier = new String(_scenario->GetIdentifier());
		result->SetObjectForKey(identifier->Autorelease(), RNCSTR("identifier"));
		result->SetObjectForKey(String::WithString(status? status : "unknown"), RNCSTR("status"));
		if(message)
			result->SetObjectForKey(String::WithString(message), RNCSTR("message"));

		result->SetObjectForKey(Number::WithDouble(_scenarioStartTime), RNCSTR("startedAtSeconds"));

		Dictionary *device = _scenario->GetDeviceConfiguration();
		if(device)
			result->SetObjectForKey(device, RNCSTR("device"));

		if(!_captureRecords.empty())
		{
			Array *captures = new Array();
			for(const CaptureRecord &record : _captureRecords)
			{
				Dictionary *capture = new Dictionary();
				capture->SetObjectForKey(Number::WithUint64(record.index), RNCSTR("index"));
				if(!record.name.empty())
					capture->SetObjectForKey(String::WithString(record.name.c_str()), RNCSTR("name"));
				if(record.didPrepare)
					capture->SetObjectForKey(Number::WithDouble(record.prepareTime), RNCSTR("prepareSeconds"));
				if(record.didStart)
					capture->SetObjectForKey(Number::WithDouble(record.startTime), RNCSTR("startSeconds"));
				if(record.didEnd)
					capture->SetObjectForKey(Number::WithDouble(record.endTime), RNCSTR("endSeconds"));

				captures->AddObject(capture->Autorelease());
			}
			result->SetObjectForKey(captures->Autorelease(), RNCSTR("captures"));
		}

		Array *actions = new Array();
		for(const ActionRecord &record : _actionRecords)
		{
			Dictionary *action = new Dictionary();
			action->SetObjectForKey(Number::WithUint64(record.index), RNCSTR("index"));
			action->SetObjectForKey(String::WithString(record.command.c_str()), RNCSTR("command"));
			if(!record.name.empty())
				action->SetObjectForKey(String::WithString(record.name.c_str()), RNCSTR("name"));
			action->SetObjectForKey(String::WithString(record.status.c_str()), RNCSTR("status"));
			if(!record.message.empty())
				action->SetObjectForKey(String::WithString(record.message.c_str()), RNCSTR("message"));
			action->SetObjectForKey(Number::WithDouble(record.startTime), RNCSTR("startSeconds"));
			if(record.endTime > 0.0)
				action->SetObjectForKey(Number::WithDouble(record.endTime), RNCSTR("endSeconds"));

			actions->AddObject(action->Autorelease());
		}
		result->SetObjectForKey(actions->Autorelease(), RNCSTR("actions"));

		try
		{
			Data *data = JSONSerialization::JSONDataFromObject(result, JSONSerialization::Options::PrettyPrint);
			data->WriteToFile(GetDefaultResultPath());
		}
		catch(Exception &e)
		{
			RNWarning("Failed writing performance scenario result: " << e);
		}

		result->Release();
	}

	String *PerformanceScenarioRunner::GetDefaultScenarioPath(bool external) const
	{
		FileManager::Location location = external? FileManager::Location::ExternalSaveDirectory : FileManager::Location::InternalSaveDirectory;
		String *path = FileManager::GetSharedInstance()->GetPathForLocation(location);
		path->AppendPathComponent(RNCSTR("perf"));
		path->AppendPathComponent(RNCSTR("scenario.json"));
		return path;
	}

	String *PerformanceScenarioRunner::GetDefaultResultPath() const
	{
		FileManager *fileManager = FileManager::GetSharedInstance();
		String *directory = fileManager->GetPathForLocation(FileManager::Location::ExternalSaveDirectory);
		directory->AppendPathComponent(RNCSTR("perf"));
		fileManager->CreateDirectory(directory);

		return directory->StringByAppendingPathComponent(RNCSTR("result.json"));
	}
} // namespace RN
