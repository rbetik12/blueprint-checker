#pragma once

class FEngineWorker
{
public:
	// Custom init routine calls PreStartupScreen and PostStartupScreen, which are basically copy-paste from Unreal's LaunchEngineLoop.cpp,
	// except some initialization routines for GUI and Rendering.
	static void Init();

	// Destroys our headless editor
	static void Exit();
private:
	// Initialization functions. Actually, it's Unreal Engine initialization code, except plugins and some modules loading, because we don't need that
	static void PreStartupScreen();
	static void PostStartupScreen();
	static bool AppInit();
	static void InitializeEnvironment();
};
