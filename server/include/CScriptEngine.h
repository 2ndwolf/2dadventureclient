#pragma once

#ifndef CSCRIPTENGINE_H
#define CSCRIPTENGINE_H

#include <cassert>
#include <string>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "CRunnerStub.h"
#include "ScriptBindings.h"
#include "ScriptAction.h"
#include "ScriptFactory.h"

class IScriptEnv;
class IScriptFunction;

class TNPC;
class CRunnerStub;
class TWeapon;

class CScriptEngine
{
public:
	explicit CScriptEngine(CRunnerStub *runner);
	~CScriptEngine();

	bool Initialize();
	void Cleanup(bool shutDown = false);
	void RunScripts(bool timedCall = false);

	void ScriptWatcher();
	void StartScriptExecution(const std::chrono::high_resolution_clock::time_point& startTime);
	bool StopScriptExecution();

	CRunnerStub * getRunner() const;
	IScriptEnv * getScriptEnv() const;
	IScriptWrapped<CRunnerStub> * getRunnerObject() const;

	bool ExecuteNpc(TNPC *npc);
	bool ExecuteWeapon(TWeapon *weapon);

	void RegisterNpcTimer(TNPC *npc);
	void RegisterNpcUpdate(TNPC *npc);
	void RegisterWeaponUpdate(TWeapon *weapon);

	void UnregisterNpcTimer(TNPC *npc);
	void UnregisterNpcUpdate(TNPC *npc);
	void UnregisterWeaponUpdate(TWeapon *weapon);

	// callbacks
	IScriptFunction * getCallBack(const std::string& callback) const;
	void removeCallBack(const std::string& callback);
	void setCallBack(const std::string& callback, IScriptFunction *cbFunc);

	// Script Compile / Cache
	IScriptFunction * CompileCache(const std::string& code, bool referenceCount = true);
	bool ClearCache(const std::string& code);

	const ScriptRunError& getScriptError() const;

	template<class... Args>
	ScriptAction * CreateAction(const std::string& action, Args... An);

	template<class T>
	IScriptWrapped<T> * WrapObject(T *obj) const;

	template <typename T>
	static std::string WrapScript(const std::string& code);

private:
	IScriptEnv *_env;
	IScriptFunction *_bootstrapFunction;
	IScriptWrapped<CRunnerStub> *_environmentObject;
	IScriptWrapped<CRunnerStub> *_runnerObject;
	CRunnerStub *_runner;

	// Script watcher
	std::atomic<bool> _scriptIsRunning;
	std::atomic<bool> _scriptWatcherRunning;
	std::chrono::high_resolution_clock::time_point _scriptStartTime;
	std::mutex _scriptWatcherLock;
	std::thread _scriptWatcherThread;

	std::unordered_map<std::string, IScriptFunction *> _cachedScripts;
	std::unordered_map<std::string, IScriptFunction *> _callbacks;
	std::unordered_set<TNPC *> _updateNpcs;
	std::unordered_set<TNPC *> _updateNpcsTimer;
	std::unordered_set<TWeapon *> _updateWeapons;
	std::unordered_set<IScriptFunction *> _deletedFunctions;
};

inline void CScriptEngine::StartScriptExecution(const std::chrono::high_resolution_clock::time_point& startTime)
{
	{
		std::lock_guard<std::mutex> guard(_scriptWatcherLock);
		_scriptStartTime = startTime;
	}
	_scriptIsRunning.store(true);
}

inline bool CScriptEngine::StopScriptExecution()
{
	bool res = _scriptIsRunning.load();
	if (res)
		_scriptIsRunning.store(false);
	return res;
}

// Getters

inline CRunnerStub * CScriptEngine::getRunner() const {
	return _runner;
}

inline IScriptEnv * CScriptEngine::getScriptEnv() const {
	return _env;
}

inline IScriptWrapped<CRunnerStub> * CScriptEngine::getRunnerObject() const {
	return _runnerObject;
}

inline IScriptFunction * CScriptEngine::getCallBack(const std::string& callback) const {
	auto it = _callbacks.find(callback);
	if (it != _callbacks.end())
		return it->second;

	return nullptr;
}

inline const ScriptRunError& CScriptEngine::getScriptError() const {
	return _env->getScriptError();
}

// Setters
inline void CScriptEngine::removeCallBack(const std::string& callback) {
	auto it = _callbacks.find(callback);
	if (it != _callbacks.end())
	{
		_deletedFunctions.insert(it->second);
		_callbacks.erase(it);
	}
}

inline void CScriptEngine::setCallBack(const std::string& callback, IScriptFunction *cbFunc) {
	removeCallBack(callback);
	_callbacks[callback] = cbFunc;
}

// Register scripts for processing

inline void CScriptEngine::RegisterNpcTimer(TNPC *npc) {
	_updateNpcsTimer.insert(npc);
}

inline void CScriptEngine::RegisterNpcUpdate(TNPC *npc) {
	_updateNpcs.insert(npc);
}

inline void CScriptEngine::RegisterWeaponUpdate(TWeapon *weapon) {
	_updateWeapons.insert(weapon);
}

// Unregister scripts from processing

inline void CScriptEngine::UnregisterWeaponUpdate(TWeapon *weapon) {
	_updateWeapons.erase(weapon);
}

inline void CScriptEngine::UnregisterNpcUpdate(TNPC *npc) {
	_updateNpcs.erase(npc);
}

inline void CScriptEngine::UnregisterNpcTimer(TNPC *npc) {
	_updateNpcsTimer.erase(npc);
}

//

template<class... Args>
ScriptAction * CScriptEngine::CreateAction(const std::string& action, Args... An)
{
	// TODO(joey): This just creates an action, and leaves it up to the user to do something with this. Most-likely will be renamed, or changed.
	constexpr size_t Argc = (sizeof...(Args));
	assert(Argc > 0);

	SCRIPTENV_D("Runner_RegisterAction:\n");
	SCRIPTENV_D("\tAction: %s\n", action.c_str());
	SCRIPTENV_D("\tArguments: %zu\n", Argc);

	auto funcIt = _callbacks.find(action);
	if (funcIt == _callbacks.end())
	{
		SCRIPTENV_D("Global::Runner_RegisterAction: Callback not registered for %s\n", action.c_str());
		return nullptr;
	}

	// Create an arguments object, and pass it to ScriptAction
	IScriptArguments *args = ScriptFactory::CreateArguments(_env, std::forward<Args>(An)...);

	ScriptAction *newScriptAction = new ScriptAction(funcIt->second, args, action);
	return newScriptAction;
}

template<class T>
inline IScriptWrapped<T> * CScriptEngine::WrapObject(T *obj) const
{
	SCRIPTENV_D("Begin Global::WrapObject()\n");

	// Wrap the object, and set the new script object on the original object
	IScriptWrapped<T> *wrappedObject = ScriptFactory::WrapObject(_env, ScriptConstructorId<T>::result, obj);
	obj->setScriptObject(wrappedObject);

	SCRIPTENV_D("End Global::WrapObject()\n\n");
	return wrappedObject;
}

template <typename T>
inline std::string CScriptEngine::WrapScript(const std::string& code) {
	return code;
}

template <>
inline std::string CScriptEngine::WrapScript<TNPC>(const std::string& code) {
	// self.onCreated || onCreated, for first declared to take precedence
	// if (onCreated) for latest function to override
	static const char *prefixString = "(function(npc) {" \
		"var onCreated, onTimeout, onNpcWarped, onPlayerChats, onPlayerEnters, onPlayerLeaves, onPlayerTouchsMe, onPlayerLogin, onPlayerLogout;" \
		"const self = npc;" \
		"if (onCreated) self.onCreated = onCreated;" \
		"if (onTimeout) self.onTimeout = onTimeout;" \
		"if (onNpcWarped) self.onNpcWarped = onNpcWarped;" \
		"if (onPlayerChats) self.onPlayerChats = onPlayerChats;" \
		"if (onPlayerEnters) self.onPlayerEnters = onPlayerEnters;" \
		"if (onPlayerLeaves) self.onPlayerLeaves = onPlayerLeaves;" \
		"if (onPlayerTouchsMe) self.onPlayerTouchsMe = onPlayerTouchsMe;" \
		"if (onPlayerLogin) self.onPlayerLogin = onPlayerLogin;" \
		"if (onPlayerLogout) self.onPlayerLogout = onPlayerLogout;" \
		"\n";

	std::string wrappedCode = std::string(prefixString);
	wrappedCode.append(code);
	wrappedCode.append("\n});");
	return wrappedCode;
}

class TPlayer;
template <>
inline std::string CScriptEngine::WrapScript<TPlayer>(const std::string& code) {
	static const char *prefixString = "(function(player) {" \
		"const self = player;\n";

	std::string wrappedCode = std::string(prefixString);
	wrappedCode.append(code);
	wrappedCode.append("\n});");
	return wrappedCode;
}

class TWeapon;
template <>
inline std::string CScriptEngine::WrapScript<TWeapon>(const std::string& code) {
	static const char *prefixString = "(function(weapon) {" \
		"var onCreated, onActionServerSide;" \
		"const self = weapon;" \
		"self.onCreated = onCreated;" \
		"self.onActionServerSide = onActionServerSide;\n";

	std::string wrappedCode = std::string(prefixString);
	wrappedCode.append(code);
	wrappedCode.append("\n});");
	return wrappedCode;
}

#endif
