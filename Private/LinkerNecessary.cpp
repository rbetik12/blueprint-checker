// Here we explicitly say to linker which entry point to use
#pragma comment(linker, "/SUBSYSTEM:CONSOLE /ENTRY:mainCRTStartup")

#include "LaunchEngineLoop.h"
#include <Windows.h>

FEngineLoop GEngineLoop;
bool GIsConsoleExecutable = true;

//TODO Get rid of two entry points in the file, but it looks like linker wants both of these functions here
int32 WINAPI WinMain( _In_ HINSTANCE hInInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ char*, _In_ int32 nCmdShow )
{
	return 0;
}