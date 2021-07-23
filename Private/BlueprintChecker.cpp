#pragma comment(linker, "/SUBSYSTEM:CONSOLE /ENTRY:mainCRTStartup")

#include "BlueprintChecker.h"
#include "RequiredProgramMainCPPInclude.h"
#include "UObject/CoreRedirects.h"
#include "Serialization/AsyncLoadingThread.h"

DEFINE_LOG_CATEGORY_STATIC(LogBlueprintChecker, Log, All);

IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultModuleImpl, BlueprintChecker, BlueprintChecker)

FEngineLoop GEngineLoop;
bool GIsConsoleExecutable = true;
__declspec( dllimport ) void InitUObject();

void InitializeEngine()
{
	// Initialization
	FCommandLine::Set(TEXT(""));
	FPlatformProcess::SetCurrentWorkingDirectoryToBaseDir();
	FConfigCacheIni::InitializeConfigSystem();
	// InitUObject();
	FCoreRedirects::Initialize();
	// FLinkerLoad::CreateActiveRedirectsMap(TEXT(""));
	FTaskGraphInterface::Startup(FPlatformMisc::NumberOfCores());
	FTaskGraphInterface::Get().AttachToThread(ENamedThreads::GameThread);
	FThreadStats::StartThread();

	GEventDrivenLoaderEnabled = false;
	GWarn = FPlatformApplicationMisc::GetFeedbackContext();
	//TODO Find where UE4 gets asset file size
	// FMaxPackageSummarySize::Value = 100000;
}

// !!!Entry point is HERE!!!
int main()
{
	// InitializeEngine();
	GEngineLoop.PreInit(TEXT(""));
	UE_LOG(LogBlueprintChecker, Display, TEXT("Initialized engine!"))
	// FEngineLoop::AppPreExit();
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, TEXT("/Engine/MyAssets/NewBlueprint"));
	
	return 0;
}

//TODO Get rid of two entry points in the file, but it looks like linker wants both of these functions here
int32 WINAPI WinMain( _In_ HINSTANCE hInInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ char*, _In_ int32 nCmdShow )
{
	return 0;
}
