﻿#include "FEngineWorker.h"
#include "RequiredProgramMainCPPInclude.h"

#define mcheck(X)

#define ERROR std::cout << "Error: " << __LINE__ << std::endl; \
			exit(EXIT_FAILURE);

void FEngineWorker::Init()
{
	PreStartupScreen();
	PostStartupScreen();
	std::cout << "Successfully initialized engine!" << std::endl;
}

void FEngineWorker::Exit()
{
	GEngineLoop.Exit();
	std::cout << "Successfully destroyed engine!" << std::endl;
}

void FEngineWorker::PreStartupScreen()
{
#if !CUSTOM_ENGINE_INITIALIZATION
	GEngineLoop.PreInitPreStartupScreen(TEXT(""));
#else
	const TCHAR* CmdLine = TEXT("");
	FDelayedAutoRegisterHelper::RunAndClearDelayedAutoRegisterDelegates(EDelayedRegisterRunPhase::StartOfEnginePreInit);
	SCOPED_BOOT_TIMING("FEngineLoop::PreInitPreStartupScreen");

	// The GLog singleton is lazy initialised and by default will assume that
	// its "master thread" is the one it was created on. This lazy initialisation
	// can happen during the dynamic init (e.g. static) of a DLL which modern
	// Windows does on a worker thread thus makeing its master thread not this one.
	// So we make it this one and GLog->TearDown() is happy.
	if (GLog != nullptr)
	{
		GLog->SetCurrentThreadAsMasterThread();
	}

	mcheck(GConfig);
	
	// Set the flag for whether we've build DebugGame instead of Development. The engine does not know this (whereas the launch module does) because it is always built in development.
#if UE_BUILD_DEVELOPMENT && defined(UE_BUILD_DEVELOPMENT_WITH_DEBUGGAME) && UE_BUILD_DEVELOPMENT_WITH_DEBUGGAME
	FApp::SetDebugGame(true);
#endif

#if PLATFORM_WINDOWS
	// Register a handler for Ctrl-C so we've effective signal handling from the outset.
	FWindowsPlatformMisc::SetGracefulTerminationHandler();
#endif // PLATFORM_WINDOWS

#if BUILD_EMBEDDED_APP
#ifdef EMBEDDED_LINKER_GAME_HELPER_FUNCTION
	extern void EMBEDDED_LINKER_GAME_HELPER_FUNCTION();
	EMBEDDED_LINKER_GAME_HELPER_FUNCTION();
#endif
	FEmbeddedCommunication::Init();
	FEmbeddedCommunication::KeepAwake(TEXT("Startup"), false);
#endif

	FMemory::SetupTLSCachesOnCurrentThread();

	if (FParse::Param(CmdLine, TEXT("UTF8Output")))
	{
		FPlatformMisc::SetUTF8Output();
	}

	mcheck(GConfig);
	// Switch into executable's directory.
	FPlatformProcess::SetCurrentWorkingDirectoryToBaseDir();

	// this is set later with shorter command lines, but we want to make sure it is set ASAP as some subsystems will do the tests themselves...
	// also realize that command lines can be pulled from the network at a slightly later time.
	if (!FCommandLine::Set(CmdLine))
	{
		// Fail, shipping builds will crash if setting command line fails
		ERROR
	}

	// Avoiding potential exploits by not exposing command line overrides in the shipping games.
#if !UE_BUILD_SHIPPING && WITH_EDITORONLY_DATA
	// Retrieve additional command line arguments from environment variable.
	FString Env = FPlatformMisc::GetEnvironmentVariable(TEXT("UE-CmdLineArgs")).TrimStart();
	if (Env.Len())
	{
		// Append the command line environment after inserting a space as we can't set it in the
		// environment.
		FCommandLine::Append(TEXT(" -EnvAfterHere "));
		FCommandLine::Append(*Env);
		CmdLine = FCommandLine::Get();
	}
#endif

	// Initialize trace
	FTraceAuxiliary::Initialize(CmdLine);

	// disable/enable LLM based on commandline
	{
		SCOPED_BOOT_TIMING("LLM Init");
		LLM(FLowLevelMemTracker::Get().ProcessCommandLine(CmdLine));
#if MEMPRO_ENABLED
		FMemProProfiler::Init(CmdLine);
#endif
	}
	LLM_SCOPE(ELLMTag::EnginePreInitMemory);

	mcheck(GConfig);
	{
		SCOPED_BOOT_TIMING("InitTaggedStorage");
		FPlatformMisc::InitTaggedStorage(1024);
	}

#if WITH_ENGINE
	FCoreUObjectDelegates::PostGarbageCollectConditionalBeginDestroy.AddStatic(DeferredPhysResourceCleanup);
#endif
	FCoreDelegates::OnSamplingInput.AddStatic(UpdateGInputTime);

#if defined(WITH_LAUNCHERCHECK) && WITH_LAUNCHERCHECK
	if (ILauncherCheckModule::Get().WasRanFromLauncher() == false)
	{
		// Tell Launcher to run us instead
		ILauncherCheckModule::Get().RunLauncher(ELauncherAction::AppLaunch);
		// We wish to exit
		RequestEngineExit(TEXT("Run outside of launcher; restarting via launcher"));
		return 1;
	}
#endif

#if	STATS
	// Create the stats malloc profiler proxy.
	if (FStatsMallocProfilerProxy::HasMemoryProfilerToken())
	{
		if (PLATFORM_USES_FIXED_GMalloc_CLASS)
		{
			UE_LOG(LogMemory, Fatal, TEXT("Cannot do malloc profiling with PLATFORM_USES_FIXED_GMalloc_CLASS."));
		}
		// Assumes no concurrency here.
		GMalloc = FStatsMallocProfilerProxy::Get();
	}
#endif // STATS

	// Name of project file before normalization (as specified in command line).
	// Used to fixup project name if necessary.
	FString GameProjectFilePathUnnormalized;
	mcheck(GConfig);
	{
		SCOPED_BOOT_TIMING("LaunchSetGameName");

		// Set GameName, based on the command line
		if (LaunchSetGameName(CmdLine, GameProjectFilePathUnnormalized) == false)
		{
			// If it failed, do not continue
			ERROR
		}
	}
	mcheck(GConfig);
#if WITH_APPLICATION_CORE
	{
		SCOPED_BOOT_TIMING("CreateConsoleOutputDevice");
		// Initialize log console here to avoid statics initialization issues when launched from the command line.
		GScopedLogConsole = TUniquePtr<FOutputDeviceConsole>(FPlatformApplicationMisc::CreateConsoleOutputDevice());
	}
#endif

	// Always enable the backlog so we get all messages, we will disable and clear it in the game
	// as soon as we determine whether GIsEditor == false
	GLog->EnableBacklog(true);

	// Initialize std out device as early as possible if requested in the command line
#if PLATFORM_DESKTOP
	// consoles don't typically have stdout, and FOutputDeviceDebug is responsible for echoing logs to the
	// terminal
	if (FParse::Param(FCommandLine::Get(), TEXT("stdout")))
	{
		InitializeStdOutDevice();
	}
#endif

#if !UE_BUILD_SHIPPING
	if (FPlatformProperties::SupportsQuit())
	{
		FString ExitPhrases;
		if (FParse::Value(FCommandLine::Get(), TEXT("testexit="), ExitPhrases))
		{
			TArray<FString> ExitPhrasesList;
			if (ExitPhrases.ParseIntoArray(ExitPhrasesList, TEXT("+"), true) > 0)
			{
				GScopedTestExit = MakeUnique<FOutputDeviceTestExit>(ExitPhrasesList);
				GLog->AddOutputDevice(GScopedTestExit.Get());
			}
		}
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("emitdrawevents")))
	{
		SetEmitDrawEvents(true);
	}
#endif // !UE_BUILD_SHIPPING

#if RHI_COMMAND_LIST_DEBUG_TRACES
	// Enable command-list-only draw events if we haven't already got full draw events enabled.
	if (!GetEmitDrawEvents())
	{
		EnableEmitDrawEventsOnlyOnCommandlist();
	}
#endif // RHI_COMMAND_LIST_DEBUG_TRACES
	mcheck(GConfig);
	// Switch into executable's directory (may be required by some of the platform file overrides)
	FPlatformProcess::SetCurrentWorkingDirectoryToBaseDir();

	// This fixes up the relative project path, needs to happen before we set platform file paths
	if (FPlatformProperties::IsProgram() == false)
	{
		SCOPED_BOOT_TIMING("Fix up the relative project path");

		if (FPaths::IsProjectFilePathSet())
		{
			FString ProjPath = FPaths::GetProjectFilePath();
			if (FPaths::FileExists(ProjPath) == false)
			{
				// display it multiple ways, it's very important error message...
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Project file not found: %s\n"), *ProjPath);
				UE_LOG(LogInit, Display, TEXT("Project file not found: %s"), *ProjPath);
				UE_LOG(LogInit, Display, TEXT("\tAttempting to find via project info helper."));
				// Use the uprojectdirs
				FString GameProjectFile = FUProjectDictionary::GetDefault().GetRelativeProjectPathForGame(FApp::GetProjectName(), FPlatformProcess::BaseDir());
				if (GameProjectFile.IsEmpty() == false)
				{
					UE_LOG(LogInit, Display, TEXT("\tFound project file %s."), *GameProjectFile);
					FPaths::SetProjectFilePath(GameProjectFile);

					// Fixup command line if project file wasn't found in specified directory to properly parse next arguments.
					FString OldCommandLine = FString(FCommandLine::Get());
					OldCommandLine.ReplaceInline(*GameProjectFilePathUnnormalized, *GameProjectFile, ESearchCase::CaseSensitive);
					FCommandLine::Set(*OldCommandLine);
					CmdLine = FCommandLine::Get();
				}
			}
		}
	}
	mcheck(GConfig);
	// Output devices.
	{
		SCOPED_BOOT_TIMING("Init Output Devices");
#if WITH_APPLICATION_CORE
		GError = FPlatformApplicationMisc::GetErrorOutputDevice();
		GWarn = FPlatformApplicationMisc::GetFeedbackContext();
#else
		GError = FPlatformOutputDevices::GetError();
		GWarn = FPlatformOutputDevices::GetFeedbackContext();
#endif
	}

	// Avoiding potential exploits by not exposing command line overrides in the shipping games.
#if !UE_BUILD_SHIPPING && WITH_EDITORONLY_DATA
	{
		SCOPED_BOOT_TIMING("Command Line Adjustments");

		bool bChanged = false;
		LaunchCheckForCommandLineAliases(bChanged);
		if (bChanged)
		{
			CmdLine = FCommandLine::Get();
		}

		bChanged = false;
		LaunchCheckForCmdLineFile(CmdLine, bChanged);
		if (bChanged)
		{
			CmdLine = FCommandLine::Get();
		}
 	}
#endif

	{
		SCOPED_BOOT_TIMING("BeginPreInitTextLocalization");
		BeginPreInitTextLocalization();
	}

	// allow the command line to override the platform file singleton
	bool bFileOverrideFound = false;
	{
		SCOPED_BOOT_TIMING("LaunchCheckForFileOverride");
		if (LaunchCheckForFileOverride(CmdLine, bFileOverrideFound) == false)
		{
			// if it failed, we cannot continue
			ERROR
		}
	}

	// 
#if PLATFORM_DESKTOP && !IS_MONOLITHIC
	{
		SCOPED_BOOT_TIMING("AddExtraBinarySearchPaths");
		FModuleManager::Get().AddExtraBinarySearchPaths();
	}
#endif
	mcheck(GConfig);
	// Initialize file manager
	{
		SCOPED_BOOT_TIMING("IFileManager::Get().ProcessCommandLineOptions");
		IFileManager::Get().ProcessCommandLineOptions();
	}

#if WITH_COREUOBJECT
	{
		SCOPED_BOOT_TIMING("InitializeNewAsyncIO");
		FPlatformFileManager::Get().InitializeNewAsyncIO();
	}
#endif

	FDelayedAutoRegisterHelper::RunAndClearDelayedAutoRegisterDelegates(EDelayedRegisterRunPhase::FileSystemReady);

	if (GIsGameAgnosticExe)
	{
		// If we launched without a project file, but with a game name that is incomplete, warn about the improper use of a Game suffix
		if (LaunchHasIncompleteGameName())
		{
			// We did not find a non-suffixed folder and we DID find the suffixed one.
			// The engine MUST be launched with <GameName>Game.
			const FText GameNameText = FText::FromString(FApp::GetProjectName());
			ERROR
		}
	}

	// remember thread id of the main thread
	GGameThreadId = FPlatformTLS::GetCurrentThreadId();
	GIsGameThreadIdInitialized = true;

	FPlatformProcess::SetThreadAffinityMask(FPlatformAffinity::GetMainGameMask());
	FPlatformProcess::SetupGameThread();

	// Figure out whether we're the editor, ucc or the game.
	const SIZE_T CommandLineSize = FCString::Strlen(CmdLine) + 1;
	TCHAR* CommandLineCopy = new TCHAR[CommandLineSize];
	FCString::Strcpy(CommandLineCopy, CommandLineSize, CmdLine);
	const TCHAR* ParsedCmdLine = CommandLineCopy;

	FString Token = FParse::Token(ParsedCmdLine, 0);

#if WITH_ENGINE
	// Add the default engine shader dir
	AddShaderSourceDirectoryMapping(TEXT("/Engine"), FGenericPlatformProcess::ShaderDir());

	TArray<FString> Tokens;
	TArray<FString> Switches;
	UCommandlet::ParseCommandLine(CommandLineCopy, Tokens, Switches);

	bool bHasCommandletToken = false;

	for (int32 TokenIndex = 0; TokenIndex < Tokens.Num(); ++TokenIndex)
	{
		if (Tokens[TokenIndex].EndsWith(TEXT("Commandlet")))
		{
			bHasCommandletToken = true;
			Token = Tokens[TokenIndex];
			break;
		}
	}

	for (int32 SwitchIndex = 0; SwitchIndex < Switches.Num() && !bHasCommandletToken; ++SwitchIndex)
	{
		if (Switches[SwitchIndex].StartsWith(TEXT("RUN=")))
		{
			bHasCommandletToken = true;
			Token = Switches[SwitchIndex];
			break;
		}
	}

	if (bHasCommandletToken)
	{
		// will be reset later once the commandlet class loaded
		PRIVATE_GIsRunningCommandlet = true;
	}

#endif // WITH_ENGINE

	mcheck(GConfig);
	// trim any whitespace at edges of string - this can happen if the token was quoted with leading or trailing whitespace
	// VC++ tends to do this in its "external tools" config
	Token.TrimStartAndEndInline();

	// Path returned by FPaths::GetProjectFilePath() is normalized, so may have symlinks and ~ resolved and may differ from the original path to .uproject passed in the command line
	FString NormalizedToken = Token;
	FPaths::NormalizeFilename(NormalizedToken);

	const bool bFirstTokenIsGameName = (FApp::HasProjectName() && Token == FApp::GetProjectName());
	const bool bFirstTokenIsGameProjectFilePath = (FPaths::IsProjectFilePathSet() && NormalizedToken == FPaths::GetProjectFilePath());
	const bool bFirstTokenIsGameProjectFileShortName = (FPaths::IsProjectFilePathSet() && Token == FPaths::GetCleanFilename(FPaths::GetProjectFilePath()));

	if (bFirstTokenIsGameName || bFirstTokenIsGameProjectFilePath || bFirstTokenIsGameProjectFileShortName)
	{
		// first item on command line was the game name, remove it in all cases
		FString RemainingCommandline = ParsedCmdLine;
		FCString::Strcpy(CommandLineCopy, CommandLineSize, *RemainingCommandline);
		ParsedCmdLine = CommandLineCopy;

		// Set a new command-line that doesn't include the game name as the first argument
		FCommandLine::Set(ParsedCmdLine);

		Token = FParse::Token(ParsedCmdLine, 0);
		Token.TrimStartInline();

		// if the next token is a project file, then we skip it (which can happen on some platforms that combine
		// commandlines... this handles extra .uprojects, but if you run with MyGame MyGame, we can't tell if
		// the second MyGame is a map or not)
		while (FPaths::GetExtension(Token) == FProjectDescriptor::GetExtension())
		{
			Token = FParse::Token(ParsedCmdLine, 0);
			Token.TrimStartInline();
		}

		if (bFirstTokenIsGameProjectFilePath || bFirstTokenIsGameProjectFileShortName)
		{
			// Convert it to relative if possible...
			FString RelativeGameProjectFilePath = FFileManagerGeneric::DefaultConvertToRelativePath(*FPaths::GetProjectFilePath());
			if (RelativeGameProjectFilePath != FPaths::GetProjectFilePath())
			{
				FPaths::SetProjectFilePath(RelativeGameProjectFilePath);
			}
		}
	}
	mcheck(GConfig);
	// look early for the editor token
	bool bHasEditorToken = false;

#if UE_EDITOR
	// Check each token for '-game', '-server' or '-run='
	bool bIsNotEditor = false;

	// This isn't necessarily pretty, but many requests have been made to allow
	//   UE4Editor.exe <GAMENAME> -game <map>
	// or
	//   UE4Editor.exe <GAMENAME> -game 127.0.0.0
	// We don't want to remove the -game from the commandline just yet in case
	// we need it for something later. So, just move it to the end for now...
	const bool bFirstTokenIsGame = (Token == TEXT("-GAME"));
	const bool bFirstTokenIsServer = (Token == TEXT("-SERVER"));
	const bool bFirstTokenIsModeOverride = bFirstTokenIsGame || bFirstTokenIsServer || bHasCommandletToken;
	const TCHAR* CommandletCommandLine = nullptr;
	if (bFirstTokenIsModeOverride)
	{
		bIsNotEditor = true;
		if (bFirstTokenIsGame || bFirstTokenIsServer)
		{
			// Move the token to the end of the list...
			FString RemainingCommandline = ParsedCmdLine;
			RemainingCommandline.TrimStartInline();
			RemainingCommandline += FString::Printf(TEXT(" %s"), *Token);
			FCommandLine::Set(*RemainingCommandline);
		}
		if (bHasCommandletToken)
		{
#if STATS
			// Leave the stats enabled.
			if (!FStats::EnabledForCommandlet())
			{
				FThreadStats::MasterDisableForever();
			}
#endif
			if (Token.StartsWith(TEXT("run=")))
			{
				Token.RightChopInline(4, false);
				if (!Token.EndsWith(TEXT("Commandlet")))
				{
					Token += TEXT("Commandlet");
				}
			}
			CommandletCommandLine = ParsedCmdLine;
		}
	}

	if (bHasCommandletToken)
	{
		// will be reset later once the commandlet class loaded
		PRIVATE_GIsRunningCommandlet = true;
	}

	if (!bIsNotEditor && GIsGameAgnosticExe)
	{
		// If we launched without a game name or project name, try to load the most recently loaded project file.
		// We can not do this if we are using a FilePlatform override since the game directory may already be established.
		const bool bIsBuildMachine = FParse::Param(FCommandLine::Get(), TEXT("BUILDMACHINE"));
		const bool bLoadMostRecentProjectFileIfItExists = !FApp::HasProjectName() && !bFileOverrideFound && !bIsBuildMachine && !FParse::Param(CmdLine, TEXT("norecentproject"));
		if (bLoadMostRecentProjectFileIfItExists)
		{
			LaunchUpdateMostRecentProjectFile();
		}
	}

	FString CheckToken = Token;
	bool bFoundValidToken = false;
	while (!bFoundValidToken && (CheckToken.Len() > 0))
	{
		if (!bIsNotEditor)
		{
			bool bHasNonEditorToken = (CheckToken == TEXT("-GAME")) || (CheckToken == TEXT("-SERVER")) || (CheckToken.StartsWith(TEXT("RUN="))) || CheckToken.EndsWith(TEXT("Commandlet"));
			if (bHasNonEditorToken)
			{
				bIsNotEditor = true;
				bFoundValidToken = true;
			}
		}

		CheckToken = FParse::Token(ParsedCmdLine, 0);
	}

	bHasEditorToken = !bIsNotEditor;
#elif WITH_ENGINE
	const TCHAR* CommandletCommandLine = nullptr;
	if (bHasCommandletToken)
	{
#if STATS
		// Leave the stats enabled.
		if (!FStats::EnabledForCommandlet())
		{
			FThreadStats::MasterDisableForever();
		}
#endif
		if (Token.StartsWith(TEXT("run=")))
		{
			Token.RightChopInline(4, false);
			if (!Token.EndsWith(TEXT("Commandlet")))
			{
				Token += TEXT("Commandlet");
			}
		}
		CommandletCommandLine = ParsedCmdLine;
	}
#if WITH_EDITOR && WITH_EDITORONLY_DATA
	// If a non-editor target build w/ WITH_EDITOR and WITH_EDITORONLY_DATA, use the old token check...
	//@todo. Is this something we need to support?
	bHasEditorToken = Token == TEXT("EDITOR");
#else
	// Game, server and commandlets never set the editor token
	bHasEditorToken = false;
#endif
#endif	//UE_EDITOR

#if !UE_BUILD_SHIPPING && !IS_PROGRAM
	if (!bHasEditorToken)
	{
		FTraceAuxiliary::TryAutoConnect();
	}
#endif

#if !UE_BUILD_SHIPPING
	// Benchmarking.
	FApp::SetBenchmarking(FParse::Param(FCommandLine::Get(), TEXT("BENCHMARK")));
#else
	FApp::SetBenchmarking(false);
#endif // !UE_BUILD_SHIPPING

	// "-Deterministic" is a shortcut for "-UseFixedTimeStep -FixedSeed"
	bool bDeterministic = FParse::Param(FCommandLine::Get(), TEXT("Deterministic"));

	FApp::SetUseFixedTimeStep(bDeterministic || FParse::Param(FCommandLine::Get(), TEXT("UseFixedTimeStep")));

	FApp::bUseFixedSeed = bDeterministic || FApp::IsBenchmarking() || FParse::Param(FCommandLine::Get(), TEXT("FixedSeed"));

	// Initialize random number generator.
	{
		uint32 Seed1 = 0;
		uint32 Seed2 = 0;

		if (!FApp::bUseFixedSeed)
		{
			Seed1 = FPlatformTime::Cycles();
			Seed2 = FPlatformTime::Cycles();
		}

		FMath::RandInit(Seed1);
		FMath::SRandInit(Seed2);

		UE_LOG(LogInit, Verbose, TEXT("RandInit(%d) SRandInit(%d)."), Seed1, Seed2);
	}
	mcheck(GConfig);
#if !IS_PROGRAM
	if (!GIsGameAgnosticExe && FApp::HasProjectName() && !FPaths::IsProjectFilePathSet())
	{
		// If we are using a non-agnostic exe where a name was specified but we did not specify a project path. Assemble one based on the game name.
		const FString ProjectFilePath = FPaths::Combine(*FPaths::ProjectDir(), *FString::Printf(TEXT("%s.%s"), FApp::GetProjectName(), *FProjectDescriptor::GetExtension()));
		FPaths::SetProjectFilePath(ProjectFilePath);
	}
#endif

	// Now verify the project file if we have one
	if (FPaths::IsProjectFilePathSet()
#if IS_PROGRAM
		// Programs don't need uproject files to exist, but some do specify them and if they exist we should load them
		&& FPaths::FileExists(FPaths::GetProjectFilePath())
#endif
		)
	{
		SCOPED_BOOT_TIMING("IProjectManager::Get().LoadProjectFile");

		if (!IProjectManager::Get().LoadProjectFile(FPaths::GetProjectFilePath()))
		{
			// The project file was invalid or saved with a newer version of the engine. Exit.
			UE_LOG(LogInit, Warning, TEXT("Could not find a valid project file, the engine will exit now."));
			ERROR
		}

		if (IProjectManager::Get().IsEnterpriseProject() && FPaths::DirectoryExists(FPaths::EnterpriseDir()))
		{
			// Add the enterprise binaries directory if we're an enterprise project
			FModuleManager::Get().AddBinariesDirectory(*FPaths::Combine(FPaths::EnterpriseDir(), TEXT("Binaries"), FPlatformProcess::GetBinariesSubdirectory()), false);
		}
	}
	mcheck(GConfig);
#if !IS_PROGRAM
	// Fix the project file path case before we attempt to fix the game name
	LaunchFixProjectPathCase();

	if (FApp::HasProjectName())
	{
		// Tell the module manager what the game binaries folder is
		const FString ProjectBinariesDirectory = FPaths::Combine(FPlatformMisc::ProjectDir(), TEXT("Binaries"), FPlatformProcess::GetBinariesSubdirectory());
		FPlatformProcess::AddDllDirectory(*ProjectBinariesDirectory);
		FModuleManager::Get().SetGameBinariesDirectory(*ProjectBinariesDirectory);

		LaunchFixGameNameCase();
	}
#endif

	// Some programs might not use the taskgraph or thread pool
	bool bCreateTaskGraphAndThreadPools = true;
	// If STATS is defined (via FORCE_USE_STATS or other), we have to call FTaskGraphInterface::Startup()
#if IS_PROGRAM && !STATS
	bCreateTaskGraphAndThreadPools = !FParse::Param(FCommandLine::Get(), TEXT("ReduceThreadUsage"));
#endif
	if (bCreateTaskGraphAndThreadPools)
	{
		// initialize task graph sub-system with potential multiple threads
		SCOPED_BOOT_TIMING("FTaskGraphInterface::Startup");
		FTaskGraphInterface::Startup(FPlatformMisc::NumberOfCores());
		FTaskGraphInterface::Get().AttachToThread(ENamedThreads::GameThread);
	}

	FDelayedAutoRegisterHelper::RunAndClearDelayedAutoRegisterDelegates(EDelayedRegisterRunPhase::TaskGraphSystemReady);

#if STATS
	FThreadStats::StartThread();
#endif

	FDelayedAutoRegisterHelper::RunAndClearDelayedAutoRegisterDelegates(EDelayedRegisterRunPhase::StatSystemReady);

	FScopeCycleCounter CycleCount_AfterStats(GET_STATID(STAT_FEngineLoop_PreInitPreStartupScreen_AfterStats));

	// Load Core modules required for everything else to work (needs to be loaded before InitializeRenderingCVarsCaching)
	{
		SCOPED_BOOT_TIMING("LoadCoreModules");
		if (!GEngineLoop.LoadCoreModules())
		{
			UE_LOG(LogInit, Error, TEXT("Failed to load Core modules."));
			ERROR
		}
	}
	mcheck(GConfig);
	const bool bDumpEarlyConfigReads = FParse::Param(FCommandLine::Get(), TEXT("DumpEarlyConfigReads"));
	const bool bDumpEarlyPakFileReads = FParse::Param(FCommandLine::Get(), TEXT("DumpEarlyPakFileReads"));
	const bool bForceQuitAfterEarlyReads = FParse::Param(FCommandLine::Get(), TEXT("ForceQuitAfterEarlyReads"));

	// Overly verbose to avoid a dumb static analysis warning
#if WITH_CONFIG_PATCHING
	constexpr bool bWithConfigPatching = true;
#else
	constexpr bool bWithConfigPatching = false;
#endif

	if (bDumpEarlyConfigReads)
	{
		RecordConfigReadsFromIni();
	}

	if (bDumpEarlyPakFileReads)
	{
		RecordFileReadsFromPaks();
	}

	if(bWithConfigPatching)
	{
		UE_LOG(LogInit, Verbose, TEXT("Begin recording CVar changes for config patching."));

		RecordApplyCVarSettingsFromIni();
	}

#if WITH_ENGINE
	extern ENGINE_API void InitializeRenderingCVarsCaching();
	InitializeRenderingCVarsCaching();
#endif

	bool bTokenDoesNotHaveDash = Token.Len() && FCString::Strnicmp(*Token, TEXT("-"), 1) != 0;

#if WITH_EDITOR
	// If we're running as an game but don't have a project, inform the user and exit.
	if (bHasEditorToken == false && bHasCommandletToken == false)
	{
		if (!FPaths::IsProjectFilePathSet())
		{
			//@todo this is too early to localize
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("Engine", "UE4RequiresProjectFiles", "UE4 games require a project file as the first parameter."));
			ERROR
		}
	}

	if (GIsUCCMakeStandaloneHeaderGenerator)
	{
		// Rebuilding script requires some hacks in the engine so we flag that.
		PRIVATE_GIsRunningCommandlet = true;
	}
#endif //WITH_EDITOR

	if (FPlatformProcess::SupportsMultithreading() && bCreateTaskGraphAndThreadPools)
	{
		SCOPED_BOOT_TIMING("Init FQueuedThreadPool's");

		int StackSize = 128;
		bool bForceEditorStackSize = false;
#if WITH_EDITOR
		bForceEditorStackSize = true;
#endif

		if (bHasEditorToken || bForceEditorStackSize)
		{
			StackSize = 1000;
		}


		{
			GThreadPool = FQueuedThreadPool::Allocate();
			int32 NumThreadsInThreadPool = FPlatformMisc::NumberOfWorkerThreadsToSpawn();

			// we are only going to give dedicated servers one pool thread
			if (FPlatformProperties::IsServerOnly())
			{
				NumThreadsInThreadPool = 1;
			}
			verify(GThreadPool->Create(NumThreadsInThreadPool, StackSize * 1024, TPri_SlightlyBelowNormal, TEXT("ThreadPool")));
		}
		{
			GBackgroundPriorityThreadPool = FQueuedThreadPool::Allocate();
			int32 NumThreadsInThreadPool = 2;
			if (FPlatformProperties::IsServerOnly())
			{
				NumThreadsInThreadPool = 1;
			}

			verify(GBackgroundPriorityThreadPool->Create(NumThreadsInThreadPool, StackSize * 1024, TPri_Lowest, TEXT("BackgroundThreadPool")));
		}

#if WITH_EDITOR
		{
			// when we are in the editor we like to do things like build lighting and such
			// this thread pool can be used for those purposes
			GLargeThreadPool = FQueuedThreadPool::Allocate();
			int32 NumThreadsInLargeThreadPool = FMath::Max(FPlatformMisc::NumberOfCoresIncludingHyperthreads() - 2, 2);

			// The default priority is above normal on Windows, which WILL make the system unresponsive when the thread-pool is heavily used.
			// Also need to be lower than the game-thread to avoid impacting the frame rate with too much preemption. 
			verify(GLargeThreadPool->Create(NumThreadsInLargeThreadPool, StackSize * 1024, TPri_SlightlyBelowNormal, TEXT("LargeThreadPool")));
		}
#endif
	}

#if WITH_APPLICATION_CORE
	// Get a pointer to the log output device
	GLogConsole = GScopedLogConsole.Get();
#endif

	{
		SCOPED_BOOT_TIMING("LoadPreInitModules");
		GEngineLoop.LoadPreInitModules();
	}

#if WITH_ENGINE && CSV_PROFILER
	if (!IsRunningDedicatedServer())
	{
		FCoreDelegates::OnBeginFrame.AddStatic(UpdateCoreCsvStats_BeginFrame);
		FCoreDelegates::OnEndFrame.AddStatic(UpdateCoreCsvStats_EndFrame);
	}
	FCsvProfiler::Get()->Init();
#endif

#if WITH_ENGINE
	AppLifetimeEventCapture::Init();
#endif

#if WITH_ENGINE && TRACING_PROFILER
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FTracingProfiler::Get()->Init();
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
	mcheck(GConfig)
	// Start the application
	{
		SCOPED_BOOT_TIMING("AppInit");
		if (!GEngineLoop.AppInit())
		{
			ERROR
		}
	}
	mcheck(GConfig)
	if (FPlatformProcess::SupportsMultithreading())
	{
		{
			SCOPED_BOOT_TIMING("GIOThreadPool->Create");
			GIOThreadPool = FQueuedThreadPool::Allocate();
			int32 NumThreadsInThreadPool = FPlatformMisc::NumberOfIOWorkerThreadsToSpawn();
			if (FPlatformProperties::IsServerOnly())
			{
				NumThreadsInThreadPool = 2;
			}
			verify(GIOThreadPool->Create(NumThreadsInThreadPool, 96 * 1024, TPri_AboveNormal, TEXT("IOThreadPool")));
		}
	}

	FEmbeddedCommunication::ForceTick(1);

#if WITH_ENGINE
	{
		SCOPED_BOOT_TIMING("System settings and cvar init");
		// Initialize system settings before anyone tries to use it...
		GSystemSettings.Initialize(bHasEditorToken);

		// Apply renderer settings from console variables stored in the INI.
		ApplyCVarSettingsFromIni(TEXT("/Script/Engine.RendererSettings"), *GEngineIni, ECVF_SetByProjectSetting);
		ApplyCVarSettingsFromIni(TEXT("/Script/Engine.RendererOverrideSettings"), *GEngineIni, ECVF_SetByProjectSetting);
		ApplyCVarSettingsFromIni(TEXT("/Script/Engine.StreamingSettings"), *GEngineIni, ECVF_SetByProjectSetting);
		ApplyCVarSettingsFromIni(TEXT("/Script/Engine.GarbageCollectionSettings"), *GEngineIni, ECVF_SetByProjectSetting);
		ApplyCVarSettingsFromIni(TEXT("/Script/Engine.NetworkSettings"), *GEngineIni, ECVF_SetByProjectSetting);
#if WITH_EDITOR
		ApplyCVarSettingsFromIni(TEXT("/Script/UnrealEd.CookerSettings"), *GEngineIni, ECVF_SetByProjectSetting);
#endif

#if !UE_SERVER
		if (!IsRunningDedicatedServer())
		{
			if (!IsRunningCommandlet())
			{
				// Note: It is critical that resolution settings are loaded before the movie starts playing so that the window size and fullscreen state is known
				UGameUserSettings::PreloadResolutionSettings();
			}
		}
#endif
	}
	{
		{
			SCOPED_BOOT_TIMING("InitScalabilitySystem");
			// Init scalability system and defaults
			Scalability::InitScalabilitySystem();
		}

		{
			SCOPED_BOOT_TIMING("InitializeCVarsForActiveDeviceProfile");
			// Set all CVars which have been setup in the device profiles.
			// This may include scalability group settings which will override
			// the defaults set above which can then be replaced below when
			// the game user settings are loaded and applied.
			UDeviceProfileManager::InitializeCVarsForActiveDeviceProfile();
		}

		{
			SCOPED_BOOT_TIMING("Scalability::LoadState");
			// As early as possible to avoid expensive re-init of subsystems,
			// after SystemSettings.ini file loading so we get the right state,
			// before ConsoleVariables.ini so the local developer can always override.
			// after InitializeCVarsForActiveDeviceProfile() so the user can override platform defaults
			Scalability::LoadState((bHasEditorToken && !GEditorSettingsIni.IsEmpty()) ? GEditorSettingsIni : GGameUserSettingsIni);
		}

		if (FPlatformMisc::UseRenderThread())
		{
			GUseThreadedRendering = true;
		}
	}
#endif

	{
		SCOPED_BOOT_TIMING("LoadConsoleVariablesFromINI");
		FConfigCacheIni::LoadConsoleVariablesFromINI();
	}

	{
		SCOPED_BOOT_TIMING("Platform Initialization");
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Platform Initialization"), STAT_PlatformInit, STATGROUP_LoadTime);

		// platform specific initialization now that the SystemSettings are loaded
		FPlatformMisc::PlatformInit();
#if WITH_APPLICATION_CORE
		FPlatformApplicationMisc::Init();
#endif
		FPlatformMemory::Init();
	}

#if !(IS_PROGRAM || WITH_EDITOR)
	if (FIoDispatcher::IsInitialized())
	{
		SCOPED_BOOT_TIMING("InitIoDispatcher");
		FIoDispatcher::InitializePostSettings();
	}
#endif

	// Let LogConsole know what ini file it should use to save its setting on exit.
	// We can't use GGameIni inside log console because it's destroyed in the global
	// scoped pointer and at that moment GGameIni may already be gone.
	if (GLogConsole != nullptr)
	{
		GLogConsole->SetIniFilename(*GGameIni);
	}


#if CHECK_PUREVIRTUALS
	FMessageDialog::Open(EAppMsgType::Ok, *NSLOCTEXT("Engine", "Error_PureVirtualsEnabled", "The game cannot run with CHECK_PUREVIRTUALS enabled.  Please disable CHECK_PUREVIRTUALS and rebuild the executable.").ToString());
	FPlatformMisc::RequestExit(false);
#endif

	FEmbeddedCommunication::ForceTick(2);
	
	// allow for game explorer processing (including parental controls) and firewalls installation
	if (!FPlatformMisc::CommandLineCommands())
	{
		FPlatformMisc::RequestExit(false);
	}

	bool bIsRegularClient = false;

	if (!bHasEditorToken)
	{
		// See whether the first token on the command line is a commandlet.

		//@hack: We need to set these before calling StaticLoadClass so all required data gets loaded for the commandlets.
		GIsClient = true;
		GIsServer = true;
#if WITH_EDITOR
		GIsEditor = true;
#endif	//WITH_EDITOR
		PRIVATE_GIsRunningCommandlet = true;

		// Allow commandlet rendering and/or audio based on command line switch (too early to let the commandlet itself override this).
		PRIVATE_GAllowCommandletRendering = FParse::Param(FCommandLine::Get(), TEXT("AllowCommandletRendering"));
		PRIVATE_GAllowCommandletAudio = FParse::Param(FCommandLine::Get(), TEXT("AllowCommandletAudio"));

		// We need to disregard the empty token as we try finding Token + "Commandlet" which would result in finding the
		// UCommandlet class if Token is empty.
		bool bDefinitelyCommandlet = (bTokenDoesNotHaveDash && Token.EndsWith(TEXT("Commandlet")));
		if (!bTokenDoesNotHaveDash)
		{
			if (Token.StartsWith(TEXT("run=")))
			{
				Token.RightChopInline(4, false);
				bDefinitelyCommandlet = true;
				if (!Token.EndsWith(TEXT("Commandlet")))
				{
					Token += TEXT("Commandlet");
				}
			}
		}
		else
		{
			if (!bDefinitelyCommandlet)
			{
				UClass* TempCommandletClass = FindObject<UClass>(ANY_PACKAGE, *(Token + TEXT("Commandlet")), false);

				if (TempCommandletClass)
				{
					check(TempCommandletClass->IsChildOf(UCommandlet::StaticClass())); // ok so you have a class that ends with commandlet that is not a commandlet

					Token += TEXT("Commandlet");
					bDefinitelyCommandlet = true;
				}
			}
		}

		if (!bDefinitelyCommandlet)
		{
			bIsRegularClient = true;
			GIsClient = true;
			GIsServer = false;
#if WITH_EDITORONLY_DATA
			GIsEditor = false;
#endif
			PRIVATE_GIsRunningCommandlet = false;
		}
	}

	bool bDisableDisregardForGC = bHasEditorToken;
	if (IsRunningDedicatedServer())
	{
		GIsClient = false;
		GIsServer = true;
		PRIVATE_GIsRunningCommandlet = false;
#if WITH_EDITOR
		GIsEditor = false;
#endif
		bDisableDisregardForGC |= FPlatformProperties::RequiresCookedData() && (GUseDisregardForGCOnDedicatedServers == 0);
	}

	// If std out device hasn't been initialized yet (there was no -stdout param in the command line) and
	// we meet all the criteria, initialize it now.
	if (!GScopedStdOut && !bHasEditorToken && !bIsRegularClient && !IsRunningDedicatedServer())
	{
		SCOPED_BOOT_TIMING("InitializeStdOutDevice");

		InitializeStdOutDevice();
	}

	bool bIsCook = bHasCommandletToken && (Token == TEXT("cookcommandlet"));
#if WITH_EDITOR
	{
		if (bIsCook)
		{
			// Target platform manager can only be initialized successfully after 
			ITargetPlatformManagerModule* TargetPlatformManager = GetTargetPlatformManager(false);
			FString InitErrors;
			if (TargetPlatformManager && TargetPlatformManager->HasInitErrors(&InitErrors))
			{
				RequestEngineExit(InitErrors);
				ERROR
			}
		}
	}
#endif

	{
		SCOPED_BOOT_TIMING("IPlatformFeaturesModule::Get()");
		// allow the platform to start up any features it may need
		IPlatformFeaturesModule::Get();
	}
#endif
}

void FEngineWorker::PostStartupScreen()
{
// #if !CUSTOM_ENGINE_INITIALIZATION
	// GEngineLoop.PreInitPostStartupScreen(TEXT(""));
// #else
	bool bDumpEarlyConfigReads = false;
	bool bDumpEarlyPakFileReads = false;
	bool bForceQuitAfterEarlyReads = false;
	bool bWithConfigPatching = false;
	bool bHasEditorToken = true;
	bool bDisableDisregardForGC = true;
	bool bIsRegularClient = false;
	bool bTokenDoesNotHaveDash = false;
	FString Token;
	const TCHAR* CommandletCommandLine = nullptr;
	TCHAR* CommandLineCopy = TEXT("");
	
	FPackageName::RegisterShortPackageNamesForUObjectModules();

	{
		// If we don't do this now and the async loading thread is active, then we will attempt to load this module from a thread
		FModuleManager::Get().LoadModule("AssetRegistry");
	}

	FEmbeddedCommunication::ForceTick(5);

	// for any auto-registered functions that want to wait until main(), run them now
	FDelayedAutoRegisterHelper::RunAndClearDelayedAutoRegisterDelegates(EDelayedRegisterRunPhase::PreObjectSystemReady);

	// Make sure all UObject classes are registered and default properties have been initialized
	ProcessNewlyLoadedUObjects();

	FDelayedAutoRegisterHelper::RunAndClearDelayedAutoRegisterDelegates(EDelayedRegisterRunPhase::ObjectSystemReady);
	{
		UMaterialInterface::InitDefaultMaterials();
		UMaterialInterface::AssertDefaultMaterialsExist();
		UMaterialInterface::AssertDefaultMaterialsPostLoaded();
	}

	{
		// Initialize the texture streaming system (needs to happen after RHIInit and ProcessNewlyLoadedUObjects).
		IStreamingManager::Get();
	}

	FModuleManager::Get().StartProcessingNewlyLoadedObjects();

	if (bDisableDisregardForGC)
	{
		SCOPED_BOOT_TIMING("DisableDisregardForGC");
		GUObjectArray.DisableDisregardForGC();
	}

	GEngineLoop.LoadStartupCoreModules();

	FUnrealEdMisc::Get().MountTemplateSharedPaths();

	if (GUObjectArray.IsOpenForDisregardForGC())
	{
		SCOPED_BOOT_TIMING("CloseDisregardForGC");
		GUObjectArray.CloseDisregardForGC();
	}
	NotifyRegistrationComplete();
	FReferencerFinder::NotifyRegistrationComplete();
	
	// if (UOnlineEngineInterface::Get()->IsLoaded())
	// {
	// 	SetIsServerForOnlineSubsystemsDelegate(FQueryIsRunningServer::CreateStatic(&IsServerDelegateForOSS));
	// }

	FModuleManager::Get().LoadModule(TEXT("TaskGraph"));
	if (FPlatformProcess::SupportsMultithreading())
	{
		FModuleManager::Get().LoadModule(TEXT("ProfilerService"));
		FModuleManager::Get().GetModuleChecked<IProfilerServiceModule>("ProfilerService").CreateProfilerServiceManager();
	}
// #endif
}
