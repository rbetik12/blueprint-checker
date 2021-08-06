// Here we explicitly say to linker which entry point to use
#pragma comment(linker, "/SUBSYSTEM:CONSOLE /ENTRY:mainCRTStartup")
#include <Windows.h>

//TODO Get rid of two entry points in the file, but it looks like linker wants both of these functions here
int WINAPI WinMain( _In_ HINSTANCE HInInstance, _In_opt_ HINSTANCE HPrevInstance, _In_ char*, _In_ int HCmdShow )
{
	return 0;
}