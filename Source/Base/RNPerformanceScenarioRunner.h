//
//  RNPerformanceScenarioRunner.h
//  Rayne
//
//  Copyright 2026 by Überpixel. All rights reserved.
//

#ifndef __RAYNE_PERFORMANCESCENARIORUNNER_H__
#define __RAYNE_PERFORMANCESCENARIORUNNER_H__

#include "../Objects/RNDictionary.h"
#include "../Objects/RNObject.h"
#include "../Objects/RNString.h"
#include "RNBase.h"

namespace RN
{
	class Array;
	class Kernel;
	class PerformanceCommandContext;

	/*
		Performance scenarios are optional JSON files loaded from:

			perf/scenario.json

		The runner checks the external save directory first and then the internal
		save directory. It executes one action at a time and writes:

			perf/result.json

		when the scenario completes or fails. result.json contains the scenario
		status, device block, capture records, and per-action timing records.
		Capture records are written to the "captures" array with index, optional
		name, prepareSeconds, startSeconds, and endSeconds.

		Minimal scenario shape:

		{
			"identifier": "forest_replay_90hz",
			"description": "Forest replay measured after streaming settles.",

			"device": {
				"preferredFrameRate": 90,
				"cpuLevel": 3,
				"gpuLevel": 3,
				"foveationLevel": 2,
				"dynamicFoveation": false,
				"dynamicResolution": false
			},

			"collectors": [
				{ "name": "logcat" },
				{ "name": "perfetto", "config": "perfetto/quest-frame-time.pbtxt" },
				{ "name": "ovrMetrics" }
			],

			"actions": [
				{ "name": "load_forest", "command": "loadLevel", "level": "ForestArena" },
				{ "name": "wait_level", "command": "waitFor", "condition": "levelLoaded", "timeoutSeconds": 60 },
				{ "name": "wait_streaming", "command": "waitFor", "condition": "streamingIdle", "timeoutSeconds": 30 },
				{ "command": "setDeviceSettings" },
				{ "command": "setNodePose", "target": "playerRig", "position": [0, 1.7, 4], "rotationEuler": [0, 180, 0] },
				{ "command": "waitSeconds", "seconds": 3 },
				{ "command": "prepareCapture" },
				{ "command": "waitSeconds", "seconds": 2 },
				{ "command": "startCapture" },
				{ "name": "measured_window", "command": "waitSeconds", "seconds": 60 },
				{ "command": "stopCapture" },
				{ "command": "setNodePose", "target": "playerRig", "position": [3, 1.7, 1], "rotationEuler": [0, 90, 0] },
				{ "command": "prepareCapture" },
				{ "command": "startCapture" },
				{ "name": "second_window", "command": "waitSeconds", "seconds": 30 },
				{ "command": "stopCapture" }
			]
		}

		Each action requires "command". "name" is optional and is copied into
		result.json. "timeoutSeconds" is a generic wrapper for any action: if the
		action still returns Running after that time, the scenario fails.

		Built-in commands:

			waitSeconds       Requires numeric "seconds".
			waitFor           Requires registered "condition".
			setNodePose       Requires registered SceneNode "target" and
			                  "position", "rotationEuler", or "rotation".
			setDeviceSettings No-op in Rayne core. Games/modules may override it.
			prepareCapture    Emits RN_PERF_CAPTURE_PREPARE once per collector.
			startCapture      Emits RN_PERF_CAPTURE_START once per collector.
			stopCapture       Emits RN_PERF_CAPTURE_END once per collector.
			logMarker         Requires "marker" and emits RN_PERF_MARKER.

		Capture actions use the top-level "collectors" array unless the action
		has its own "collectors" array. A collector can be a string or an object
		with "name". Scalar fields on collector objects are copied into capture
		marker lines, except "name", which is emitted as "collector". String
		values are percent-encoded for log parsing.

		Example marker:

			RN_PERF_CAPTURE_PREPARE identifier=forest_replay_90hz captureIndex=0 collector=perfetto config=perfetto/quest-frame-time.pbtxt

		Capture ordering is validated on load. A scenario may contain multiple
		capture windows. Each window is either startCapture/stopCapture or
		prepareCapture/startCapture/stopCapture. prepareCapture must be followed
		by startCapture, startCapture must be followed by stopCapture, and a new
		capture window cannot begin before the previous one is stopped.

		Game-side setup should be limited to registering game-specific commands,
		conditions, and targets once the relevant objects exist:

			PerformanceScenarioRunner *runner = PerformanceScenarioRunner::GetSharedInstance();
			runner->RegisterTarget(RNCSTR("playerRig"), cameraRig);
			runner->RegisterCommand(RNCSTR("loadLevel"), ...);
			runner->RegisterCondition(RNCSTR("levelLoaded"), ...);
			runner->RegisterCondition(RNCSTR("streamingIdle"), ...);

		A game/module may override setDeviceSettings and read values from the
		active scenario:

			PerformanceScenario *scenario = context.GetScenario();
			bool dynamicResolution = scenario->GetDeviceBoolValue(RNCSTR("dynamicResolution"), false);
			bool dynamicFoveation = scenario->GetDeviceBoolValue(RNCSTR("dynamicFoveation"), false);
			float preferredFrameRate = scenario->GetDeviceFloatValue(RNCSTR("preferredFrameRate"), 90.0f);
			int32 cpuLevel = scenario->GetDeviceInt32Value(RNCSTR("cpuLevel"), 3);
			int32 gpuLevel = scenario->GetDeviceInt32Value(RNCSTR("gpuLevel"), 3);

		Host-side tooling does not need to parse the scenario. It can push the
		scenario file, tail logcat, react to RN_PERF_SCENARIO_START, then use
		collector-specific RN_PERF_CAPTURE_PREPARE/START/END marker fields to
		prepare, start, and stop tools like Perfetto or OVR Metrics.
		captureIndex distinguishes multiple capture windows in one scenario. On
		RN_PERF_SCENARIO_END or RN_PERF_FAILED it should pull perf/result.json.
	*/

	class PerformanceScenario : public Object
	{
	public:
		RNAPI PerformanceScenario(Dictionary *dictionary);
		RNAPI ~PerformanceScenario() override;

		RNAPI const String *GetIdentifier() const;
		RNAPI const String *GetDescription() const override;
		RNAPI Dictionary *GetDictionary() const { return _dictionary; }
		RNAPI Dictionary *GetDeviceConfiguration() const;
		RNAPI bool GetDeviceBoolValue(const String *key, bool defaultValue = false) const;
		RNAPI float GetDeviceFloatValue(const String *key, float defaultValue = 0.0f) const;
		RNAPI int32 GetDeviceInt32Value(const String *key, int32 defaultValue = 0) const;
		RNAPI Array *GetCollectors() const;
		RNAPI Array *GetActions() const;

	private:
		Dictionary *_dictionary;

		__RNDeclareMetaInternal(PerformanceScenario)
	};

	RNObjectClass(PerformanceScenario)

	class PerformanceCommandResult
	{
	public:
		enum class State
		{
			Running,
			Succeeded,
			Failed
		};

		static PerformanceCommandResult Running() { return PerformanceCommandResult(State::Running, nullptr); }
		static PerformanceCommandResult Succeeded() { return PerformanceCommandResult(State::Succeeded, nullptr); }
		static PerformanceCommandResult Failed(const char *message = nullptr) { return PerformanceCommandResult(State::Failed, message); }

		State GetState() const { return _state; }
		const char *GetMessage() const { return _message; }

	private:
		PerformanceCommandResult(State state, const char *message) :
			_state(state),
			_message(message)
		{}

		State _state;
		const char *_message;
	};

	class PerformanceScenarioRunner
	{
	public:
		enum class State
		{
			Disabled,
			Running,
			Completed,
			Failed
		};

		using CommandCallback = std::function<PerformanceCommandResult(PerformanceCommandContext &)>;
		using ConditionCallback = std::function<bool(const PerformanceCommandContext &)>;

		RNAPI static PerformanceScenarioRunner *GetSharedInstance();

		RNAPI bool IsEnabled() const { return _scenario != nullptr && _state != State::Disabled; }
		RNAPI bool HasActiveScenario() const { return _scenario != nullptr; }
		RNAPI State GetState() const { return _state; }
		RNAPI PerformanceScenario *GetScenario() const { return _scenario; }

		RNAPI bool LoadDefaultScenario();
		RNAPI bool LoadScenarioFromFile(const String *path);
		RNAPI bool LoadScenario(Dictionary *dictionary);
		RNAPI void ClearScenario();

		RNAPI void RegisterCommand(const String *name, const CommandCallback &callback);
		RNAPI void UnregisterCommand(const String *name);
		RNAPI void RegisterCondition(const String *name, const ConditionCallback &callback);
		RNAPI void UnregisterCondition(const String *name);
		RNAPI void RegisterTarget(const String *name, Object *target);
		RNAPI void UnregisterTarget(const String *name);
		RNAPI Object *GetTarget(const String *name) const;

		template<class T = Object>
		T *GetTarget(const String *name) const
		{
			Object *target = GetTarget(name);
			return target? target->Downcast<T>() : nullptr;
		}

		RNAPI void Update(float delta);

		RNAPI void MarkCapturePrepare(const Dictionary *action = nullptr);
		RNAPI void MarkCaptureStart(const Dictionary *action = nullptr);
		RNAPI void MarkCaptureEnd(const Dictionary *action = nullptr);
		RNAPI void Fail(const char *message);

	private:
		friend class Kernel;
		friend class PerformanceCommandContext;

		struct CaptureRecord;

		PerformanceScenarioRunner();
		~PerformanceScenarioRunner();

		void RegisterBuiltinCommands();
		bool RunActions(float delta);
		bool EvaluateCondition(const String *name, const PerformanceCommandContext &context) const;
		bool ValidateScenario(Dictionary *dictionary, const char *&message) const;
		bool ValidateAction(Dictionary *action, size_t index, const char *&message) const;
		CaptureRecord &BeginCaptureRecord(const Dictionary *action);
		void UpdateCaptureRecordName(CaptureRecord &record, const Dictionary *action) const;
		void EmitCaptureMarker(const char *marker, const Dictionary *action, size_t captureIndex) const;
		void BeginAction(const Dictionary *action);
		void EndAction(const char *status, const char *message = nullptr);
		void BeginState(State state);
		void Complete();
		void WriteResult(const char *status, const char *message = nullptr) const;
		String *GetDefaultScenarioPath(bool external) const;
		String *GetDefaultResultPath() const;

		struct ActionRecord
		{
			size_t index;
			std::string command;
			std::string name;
			std::string status;
			std::string message;
			double startTime;
			double endTime;
		};

		struct CaptureRecord
		{
			size_t index;
			std::string name;
			double prepareTime;
			double startTime;
			double endTime;
			bool didPrepare;
			bool didStart;
			bool didEnd;
		};

		std::unordered_map<std::string, CommandCallback> _commands;
		std::unordered_map<std::string, ConditionCallback> _conditions;
		std::unordered_map<std::string, Object *> _targets;
		std::vector<ActionRecord> _actionRecords;
		std::vector<CaptureRecord> _captureRecords;

		PerformanceScenario *_scenario;
		State _state;
		size_t _commandIndex;
		float _commandElapsed;
		double _scenarioStartTime;
		size_t _activeCaptureIndex;
		bool _hasActiveCapture;
		bool _isActiveCaptureRunning;
	};

	class PerformanceCommandContext
	{
	public:
		RNAPI PerformanceScenarioRunner *GetRunner() const { return _runner; }
		RNAPI PerformanceScenario *GetScenario() const { return _runner->GetScenario(); }
		RNAPI const Dictionary *GetCommand() const { return _command; }
		RNAPI float GetDelta() const { return _delta; }
		RNAPI float GetElapsed() const { return _elapsed; }

		template<class T = Object>
		T *GetTarget(const String *name) const
		{
			return _runner->GetTarget<T>(name);
		}

		RNAPI bool EvaluateCondition(const String *name) const
		{
			return _runner->EvaluateCondition(name, *this);
		}

	private:
		friend class PerformanceScenarioRunner;

		PerformanceCommandContext(PerformanceScenarioRunner *runner, const Dictionary *command, float delta, float elapsed) :
			_runner(runner),
			_command(command),
			_delta(delta),
			_elapsed(elapsed)
		{}

		PerformanceScenarioRunner *_runner;
		const Dictionary *_command;
		float _delta;
		float _elapsed;
	};
} // namespace RN

#endif /* __RAYNE_PERFORMANCESCENARIORUNNER_H__ */
