#pragma once

class FEngineWorker
{
public:
	static void Init();
	static void Exit();
private:
	// Initialization functions. Actually, it's Unreal Engine initialization code, except plugins and some modules loading, because we don't need that
	static void PreStartupScreen();
	static void PostStartupScreen();
	static bool AppInit();
};
