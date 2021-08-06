#include "FEngineWorker.h"
#include "RequiredProgramMainCPPInclude.h"

#include <memory>
#include <thread>

#define mcheck(X)

#define ERROR std::cout << "Error: " << __FILE__ << " " << __LINE__ << std::endl; \
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
	// Console flags that tells prevent initialization of rendering system
	const TCHAR* CmdLine = TEXT("-nullrhi -NoShaderCompile");
	
	if (GLog != nullptr)
	{
		GLog->SetCurrentThreadAsMasterThread();
	}
	
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

	GWarn = FPlatformApplicationMisc::GetFeedbackContext();
	
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
				FString GameProjectFile = FUProjectDictionary::GetDefault().GetRelativeProjectPathForGame(
					FApp::GetProjectName(), FPlatformProcess::BaseDir());
				if (GameProjectFile.IsEmpty() == false)
				{
					UE_LOG(LogInit, Display, TEXT("\tFound project file %s."), *GameProjectFile);
					FPaths::SetProjectFilePath(GameProjectFile);

					// Fixup command line if project file wasn't found in specified directory to properly parse next arguments.
					FString OldCommandLine = FString(FCommandLine::Get());
					OldCommandLine.ReplaceInline(*GameProjectFilePathUnnormalized, *GameProjectFile,
					                             ESearchCase::CaseSensitive);
					FCommandLine::Set(*OldCommandLine);
					CmdLine = FCommandLine::Get();
				}
			}
		}
	}
	// Output devices.
	{
		SCOPED_BOOT_TIMING("Init Output Devices");
#if WITH_APPLICATION_CORE
		GError = FPlatformApplicationMisc::GetErrorOutputDevice();
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
			// FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("RequiresGamePrefix", "Error: UE4Editor does not append 'Game' to the passed in game name.\nYou must use the full name.\nYou specified '{0}', use '{0}Game'."), GameNameText));
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
	const bool bFirstTokenIsGameProjectFilePath = (FPaths::IsProjectFilePathSet() && NormalizedToken ==
		FPaths::GetProjectFilePath());
	const bool bFirstTokenIsGameProjectFileShortName = (FPaths::IsProjectFilePathSet() && Token ==
		FPaths::GetCleanFilename(FPaths::GetProjectFilePath()));

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
			FString RelativeGameProjectFilePath = FFileManagerGeneric::DefaultConvertToRelativePath(
				*FPaths::GetProjectFilePath());
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
		const bool bLoadMostRecentProjectFileIfItExists = !FApp::HasProjectName() && !bFileOverrideFound && !
			bIsBuildMachine && !FParse::Param(CmdLine, TEXT("norecentproject"));
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
			bool bHasNonEditorToken = (CheckToken == TEXT("-GAME")) || (CheckToken == TEXT("-SERVER")) || (CheckToken.
				StartsWith(TEXT("RUN="))) || CheckToken.EndsWith(TEXT("Commandlet"));
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

	FApp::bUseFixedSeed = bDeterministic || FApp::IsBenchmarking() || FParse::Param(
		FCommandLine::Get(), TEXT("FixedSeed"));

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
		const FString ProjectFilePath = FPaths::Combine(*FPaths::ProjectDir(),
		                                                *FString::Printf(
			                                                TEXT("%s.%s"), FApp::GetProjectName(),
			                                                *FProjectDescriptor::GetExtension()));
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
			FModuleManager::Get().AddBinariesDirectory(
				*FPaths::Combine(FPaths::EnterpriseDir(), TEXT("Binaries"),
				                 FPlatformProcess::GetBinariesSubdirectory()), false);
		}
	}
	mcheck(GConfig);
#if !IS_PROGRAM
	// Fix the project file path case before we attempt to fix the game name
	LaunchFixProjectPathCase();

	if (FApp::HasProjectName())
	{
		// Tell the module manager what the game binaries folder is
		const FString ProjectBinariesDirectory = FPaths::Combine(FPlatformMisc::ProjectDir(), TEXT("Binaries"),
		                                                         FPlatformProcess::GetBinariesSubdirectory());
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
			FMessageDialog::Open(EAppMsgType::Ok,
			                     NSLOCTEXT("Engine", "UE4RequiresProjectFiles",
			                               "UE4 games require a project file as the first parameter."));
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
			verify(
				GThreadPool->Create(NumThreadsInThreadPool, StackSize * 1024, TPri_SlightlyBelowNormal, TEXT(
					"ThreadPool")));
		}
		{
			GBackgroundPriorityThreadPool = FQueuedThreadPool::Allocate();
			int32 NumThreadsInThreadPool = 2;
			if (FPlatformProperties::IsServerOnly())
			{
				NumThreadsInThreadPool = 1;
			}

			verify(
				GBackgroundPriorityThreadPool->Create(NumThreadsInThreadPool, StackSize * 1024, TPri_Lowest, TEXT(
					"BackgroundThreadPool")));
		}

#if WITH_EDITOR
		{
			// when we are in the editor we like to do things like build lighting and such
			// this thread pool can be used for those purposes
			GLargeThreadPool = FQueuedThreadPool::Allocate();
			int32 NumThreadsInLargeThreadPool = FMath::Max(FPlatformMisc::NumberOfCoresIncludingHyperthreads() - 2, 2);

			// The default priority is above normal on Windows, which WILL make the system unresponsive when the thread-pool is heavily used.
			// Also need to be lower than the game-thread to avoid impacting the frame rate with too much preemption. 
			verify(
				GLargeThreadPool->Create(NumThreadsInLargeThreadPool, StackSize * 1024, TPri_SlightlyBelowNormal, TEXT(
					"LargeThreadPool")));
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
		if (!AppInit())
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
		ApplyCVarSettingsFromIni(
			TEXT("/Script/Engine.RendererOverrideSettings"), *GEngineIni, ECVF_SetByProjectSetting);
		ApplyCVarSettingsFromIni(TEXT("/Script/Engine.StreamingSettings"), *GEngineIni, ECVF_SetByProjectSetting);
		ApplyCVarSettingsFromIni(
			TEXT("/Script/Engine.GarbageCollectionSettings"), *GEngineIni, ECVF_SetByProjectSetting);
		ApplyCVarSettingsFromIni(TEXT("/Script/Engine.NetworkSettings"), *GEngineIni, ECVF_SetByProjectSetting);
#if WITH_EDITOR
		ApplyCVarSettingsFromIni(TEXT("/Script/UnrealEd.CookerSettings"), *GEngineIni, ECVF_SetByProjectSetting);
#endif

#if !UE_SERVER
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
			Scalability::LoadState((bHasEditorToken && !GEditorSettingsIni.IsEmpty())
				                       ? GEditorSettingsIni
				                       : GGameUserSettingsIni);
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

#if WITH_ENGINE
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
					check(TempCommandletClass->IsChildOf(UCommandlet::StaticClass()));
					// ok so you have a class that ends with commandlet that is not a commandlet

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
		bDisableDisregardForGC |= FPlatformProperties::RequiresCookedData() && (GUseDisregardForGCOnDedicatedServers ==
			0);
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

	{
		SCOPED_BOOT_TIMING("InitGamePhys");
		// Init physics engine before loading anything, in case we want to do things like cook during post-load.
		if (!InitGamePhys())
		{
			// If we failed to initialize physics we cannot continue.
			ERROR
		}
	}

	{
		bool bShouldCleanShaderWorkingDirectory = true;
#if !(UE_BUILD_SHIPPING && WITH_EDITOR)
		// Only clean the shader working directory if we are the first instance, to avoid deleting files in use by other instances
		//@todo - check if any other instances are running right now
		bShouldCleanShaderWorkingDirectory = GIsFirstInstance;
#endif

		if (bShouldCleanShaderWorkingDirectory && !FParse::Param(FCommandLine::Get(), TEXT("Multiprocess")))
		{
			SCOPED_BOOT_TIMING("FPlatformProcess::CleanShaderWorkingDirectory");

			// get shader path, and convert it to the userdirectory
			for (const auto& SHaderSourceDirectoryEntry : AllShaderSourceDirectoryMappings())
			{
				FString ShaderDir = FString(FPlatformProcess::BaseDir()) / SHaderSourceDirectoryEntry.Value;
				FString UserShaderDir = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*ShaderDir);
				FPaths::CollapseRelativeDirectories(ShaderDir);

				// make sure we don't delete from the source directory
				if (ShaderDir != UserShaderDir)
				{
					IFileManager::Get().DeleteDirectory(*UserShaderDir, false, true);
				}
			}

			FPlatformProcess::CleanShaderWorkingDir();
		}
	}

#if !UE_BUILD_SHIPPING
	GIsDemoMode = FParse::Param(FCommandLine::Get(), TEXT("DEMOMODE"));
#endif

	if (bHasEditorToken)
	{
#if WITH_EDITOR

		// We're the editor.
		GIsClient = true;
		GIsServer = true;
		GIsEditor = true;
		PRIVATE_GIsRunningCommandlet = false;

		GWarn = &UnrealEdWarn;

#else
		FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("Engine", "EditorNotSupported", "Editor not supported in this mode."));
		FPlatformMisc::RequestExit(false);
		return 1;
#endif //WITH_EDITOR
	}

#endif // WITH_ENGINE
	// If we're not in the editor stop collecting the backlog now that we know
	if (!GIsEditor)
	{
		GLog->EnableBacklog(false);
	}
#if WITH_ENGINE
	bool bForceEnableHighDPI = false;
#if WITH_EDITOR
	bForceEnableHighDPI = FPIEPreviewDeviceModule::IsRequestingPreviewDevice();
#endif

	// This must be called before any window (including the splash screen is created
	FSlateApplication::InitHighDPI(bForceEnableHighDPI);

	UStringTable::InitializeEngineBridge();

	if (FApp::ShouldUseThreadingForPerformance() && FPlatformMisc::AllowAudioThread())
	{
		bool bUseThreadedAudio = false;
		if (!GIsEditor)
		{
			GConfig->GetBool(TEXT("Audio"), TEXT("UseAudioThread"), bUseThreadedAudio, GEngineIni);
		}
		FAudioThread::SetUseThreadedAudio(bUseThreadedAudio);
	}

	if (!IsRunningDedicatedServer() && (bHasEditorToken || bIsRegularClient))
	{
		// Init platform application
		SCOPED_BOOT_TIMING("FSlateApplication::Create()");
		FSlateApplication::Create();
	}
	else
	{
		// If we're not creating the slate application there is some basic initialization
		// that it does that still must be done
		EKeys::Initialize();
		FCoreStyle::ResetToDefault();
	}

	FEmbeddedCommunication::ForceTick(3);

#if USE_LOCALIZED_PACKAGE_CACHE
	FPackageLocalizationManager::Get().InitializeFromLazyCallback(
		[](FPackageLocalizationManager& InPackageLocalizationManager)
		{
			InPackageLocalizationManager.InitializeFromCache(MakeShareable(new FEnginePackageLocalizationCache()));
		});
#endif	// USE_LOCALIZED_PACKAGE_CACHE

	{
		SCOPED_BOOT_TIMING("FUniformBufferStruct::InitializeStructs()");
		FShaderParametersMetadata::InitializeAllUniformBufferStructs();
	}

	{
		SCOPED_BOOT_TIMING("RHIInit");
		// Initialize the RHI.
		RHIInit(bHasEditorToken);
	}

	{
		SCOPED_BOOT_TIMING("RenderUtilsInit");
		// One-time initialization of global variables based on engine configuration.
		RenderUtilsInit();
	}

	{
		bool bUseCodeLibrary = FPlatformProperties::RequiresCookedData() || GAllowCookedDataInEditorBuilds;
		if (bUseCodeLibrary)
		{
			{
				SCOPED_BOOT_TIMING("FShaderCodeLibrary::InitForRuntime");
				// Will open material shader code storage if project was packaged with it
				// This only opens the Global shader library, which is always in the content dir.
				FShaderCodeLibrary::InitForRuntime(GMaxRHIShaderPlatform);
			}

#if !UE_EDITOR
			// Cooked data only - but also requires the code library - game only
			if (FPlatformProperties::RequiresCookedData())
			{
				SCOPED_BOOT_TIMING("FShaderPipelineCache::Initialize");
				// Initialize the pipeline cache system. Opening is deferred until the manual call to
				// OpenPipelineFileCache below, after content pak's ShaderCodeLibraries are loaded.
				FShaderPipelineCache::Initialize(GMaxRHIShaderPlatform);
			}
#endif //!UE_EDITOR
		}
	}


	bool bEnableShaderCompile = !FParse::Param(FCommandLine::Get(), TEXT("NoShaderCompile"));
	if (bEnableShaderCompile && !FPlatformProperties::RequiresCookedData())
	{
		check(!GShaderCompilerStats);
		GShaderCompilerStats = new FShaderCompilerStats();

		check(!GShaderCompilingManager);
		GShaderCompilingManager = new FShaderCompilingManager();

		check(!GDistanceFieldAsyncQueue);
		GDistanceFieldAsyncQueue = new FDistanceFieldAsyncQueue();

		// Shader hash cache is required only for shader compilation.
		InitializeShaderHashCache();
	}

	{
		SCOPED_BOOT_TIMING("GetRendererModule");
		// Cache the renderer module in the main thread so that we can safely retrieve it later from the rendering thread.
		GetRendererModule();
	}

	{
		if (bEnableShaderCompile)
		{
			SCOPED_BOOT_TIMING("InitializeShaderTypes");
			// Initialize shader types before loading any shaders
			InitializeShaderTypes();
		}

		// FDelayedAutoRegisterHelper::RunAndClearDelayedAutoRegisterDelegates(EDelayedRegisterRunPhase::ShaderTypesReady);

		// SlowTask.EnterProgressFrame(30);

		// Load the global shaders.
		// if (!IsRunningCommandlet())
		// hack: don't load global shaders if we are cooking we will load the shaders for the correct platform later
		if (bEnableShaderCompile &&
			!IsRunningDedicatedServer() &&
			!bIsCook)
			// if (FParse::Param(FCommandLine::Get(), TEXT("Multiprocess")) == false)
		{
			LLM_SCOPE(ELLMTag::Shaders);
			SCOPED_BOOT_TIMING("CompileGlobalShaderMap");
			// CompileGlobalShaderMap(false);
			if (IsEngineExitRequested())
			{
				// This means we can't continue without the global shader map.
				ERROR
			}
		}
		else if (FPlatformProperties::RequiresCookedData() == false)
		{
			// GetDerivedDataCacheRef();
		}

		{
			SCOPED_BOOT_TIMING("CreateMoviePlayer");
			CreateMoviePlayer();
		}

		if (FPreLoadScreenManager::ArePreLoadScreensEnabled())
		{
			SCOPED_BOOT_TIMING("FPreLoadScreenManager::Create");
			FPreLoadScreenManager::Create();
			ensure(FPreLoadScreenManager::Get());
		}

		// If platforms support early movie playback we have to start the rendering thread much earlier
#if PLATFORM_SUPPORTS_EARLY_MOVIE_PLAYBACK
		{
			SCOPED_BOOT_TIMING("PostInitRHI");
			PostInitRHI();
		}

		if (GUseThreadedRendering)
		{
			if (GRHISupportsRHIThread)
			{
				const bool DefaultUseRHIThread = true;
				GUseRHIThread_InternalUseOnly = DefaultUseRHIThread;
				if (FParse::Param(FCommandLine::Get(), TEXT("rhithread")))
				{
					GUseRHIThread_InternalUseOnly = true;
				}
				else if (FParse::Param(FCommandLine::Get(), TEXT("norhithread")))
				{
					GUseRHIThread_InternalUseOnly = false;
				}
			}
				
			SCOPED_BOOT_TIMING("StartRenderingThread");
			StartRenderingThread();
		}
#endif

		FEmbeddedCommunication::ForceTick(4);

		{
#if !UE_SERVER// && !UE_EDITOR
			if (!IsRunningDedicatedServer() && !IsRunningCommandlet())
			{
				TSharedPtr<FSlateRenderer> SlateRenderer = GUsingNullRHI
					                                           ? FModuleManager::Get().LoadModuleChecked<
						                                           ISlateNullRendererModule>("SlateNullRenderer").
					                                           CreateSlateNullRenderer()
					                                           : FModuleManager::Get().GetModuleChecked<
						                                           ISlateRHIRendererModule>("SlateRHIRenderer").
					                                           CreateSlateRHIRenderer();
				TSharedRef<FSlateRenderer> SlateRendererSharedRef = SlateRenderer.ToSharedRef();

				{
					SCOPED_BOOT_TIMING("CurrentSlateApp.InitializeRenderer");
					// If Slate is being used, initialize the renderer after RHIInit
					FSlateApplication& CurrentSlateApp = FSlateApplication::Get();
					CurrentSlateApp.InitializeRenderer(SlateRendererSharedRef);
				}

				{
					SCOPED_BOOT_TIMING("FEngineFontServices::Create");
					// Create the engine font services now that the Slate renderer is ready
					FEngineFontServices::Create();
				}

				{
					SCOPED_BOOT_TIMING("PlayFirstPreLoadScreen");

					if (FPreLoadScreenManager::Get())
					{
						{
							SCOPED_BOOT_TIMING("PlayFirstPreLoadScreen - FPreLoadScreenManager::Get()->Initialize");
							// initialize and present custom splash screen
							FPreLoadScreenManager::Get()->Initialize(SlateRendererSharedRef.Get());
						}

						if (FPreLoadScreenManager::Get()->HasRegisteredPreLoadScreenType(
							EPreLoadScreenTypes::CustomSplashScreen))
						{
							FPreLoadScreenManager::Get()->PlayFirstPreLoadScreen(
								EPreLoadScreenTypes::CustomSplashScreen);
						}
#if PLATFORM_XBOXONE && WITH_LEGACY_XDK && ENABLE_XBOXONE_FAST_ACTIVATION
						else
						{
							UE_LOG(LogInit, Warning, TEXT("Enable fast activation without enabling a custom splash screen may cause garbage frame buffer being presented"));
						}
#endif
					}
				}
			}
#endif // !UE_SERVER
		}
	}
#endif // WITH_ENGINE
	

#endif
}

void FEngineWorker::PostStartupScreen()
{
	bool bDisableDisregardForGC = true;
	FString Token;

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
	
	FModuleManager::Get().LoadModule(TEXT("TaskGraph"));
	if (FPlatformProcess::SupportsMultithreading())
	{
		FModuleManager::Get().LoadModule(TEXT("ProfilerService"));
		FModuleManager::Get().GetModuleChecked<IProfilerServiceModule>("ProfilerService").
		                      CreateProfilerServiceManager();
	}

	// CommandletClass->GetDefaultObject<UCommandlet>()->CreateCustomEngine(CommandletCommandLine);
	if ( GEngine == nullptr )
	{
#if WITH_EDITOR
		if ( GIsEditor )
		{
			FString EditorEngineClassName;
			GConfig->GetString(TEXT("/Script/Engine.Engine"), TEXT("EditorEngine"), EditorEngineClassName, GEngineIni);
			UClass* EditorEngineClass = StaticLoadClass( UEditorEngine::StaticClass(), nullptr, *EditorEngineClassName);
			if (EditorEngineClass == nullptr)
			{
				UE_LOG(LogInit, Fatal, TEXT("Failed to load Editor Engine class '%s'."), *EditorEngineClassName);
			}

			GEngine = GEditor = NewObject<UEditorEngine>(GetTransientPackage(), EditorEngineClass);

			GEngine->ParseCommandline();

			UE_LOG(LogInit, Log, TEXT("Initializing Editor Engine..."));
			GEditor->InitEditor(&GEngineLoop);
			UE_LOG(LogInit, Log, TEXT("Initializing Editor Engine Completed"));
		}
		else
#endif
		{
			FString GameEngineClassName;
			GConfig->GetString(TEXT("/Script/Engine.Engine"), TEXT("GameEngine"), GameEngineClassName, GEngineIni);

			UClass* EngineClass = StaticLoadClass( UEngine::StaticClass(), nullptr, *GameEngineClassName);

			if (EngineClass == nullptr)
			{
				UE_LOG(LogInit, Fatal, TEXT("Failed to load Engine class '%s'."), *GameEngineClassName);
			}

			// must do this here so that the engine object that we create on the next line receives the correct property values
			GEngine = NewObject<UEngine>(GetTransientPackage(), EngineClass);
			check(GEngine);

			GEngine->ParseCommandline();

			UE_LOG(LogInit, Log, TEXT("Initializing Game Engine..."));
			GEngine->Init(&GEngineLoop);
			UE_LOG(LogInit, Log, TEXT("Initializing Game Engine Completed"));
		}
	}
}

bool FEngineWorker::AppInit()
{
	{
		SCOPED_BOOT_TIMING("BeginInitTextLocalization");
		BeginInitTextLocalization();
	}
	
	// Error history.
	FCString::Strcpy(GErrorHist, TEXT("Fatal error!" LINE_TERMINATOR LINE_TERMINATOR));

	// Platform specific pre-init.
	{
		SCOPED_BOOT_TIMING("FPlatformMisc::PlatformPreInit");
		FPlatformMisc::PlatformPreInit();
	}
#if WITH_APPLICATION_CORE
	{
		SCOPED_BOOT_TIMING("FPlatformApplicationMisc::PreInit");
		FPlatformApplicationMisc::PreInit();
	}
#endif

	// Keep track of start time.
	GSystemStartTime = FDateTime::Now().ToString();

	// Switch into executable's directory.
	FPlatformProcess::SetCurrentWorkingDirectoryToBaseDir();

	{
		SCOPED_BOOT_TIMING("IFileManager::Get().ProcessCommandLineOptions()");
		// Now finish initializing the file manager after the command line is set up
		IFileManager::Get().ProcessCommandLineOptions();
	}

	FPageAllocator::Get().LatchProtectedMode();

	if (FParse::Param(FCommandLine::Get(), TEXT("purgatorymallocproxy")))
	{
		FMemory::EnablePurgatoryTests();
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("poisonmallocproxy")))
	{
		FMemory::EnablePoisonTests();
	}

#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("BUILDMACHINE")))
	{
		GIsBuildMachine = true;
	}

	// If "-WaitForDebugger" was specified, halt startup and wait for a debugger to attach before continuing
	if( FParse::Param( FCommandLine::Get(), TEXT( "WaitForDebugger" ) ) )
	{
		while( !FPlatformMisc::IsDebuggerPresent() )
		{
			FPlatformProcess::Sleep( 0.1f );
		}
	}
#endif // !UE_BUILD_SHIPPING

#if PLATFORM_WINDOWS

	// make sure that the log directory tree exists
	IFileManager::Get().MakeDirectory( *FPaths::ProjectLogDir(), true );

	// update the mini dump filename now that we have enough info to point it to the log folder even in installed builds
	FCString::Strcpy(MiniDumpFilenameW, *IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*FString::Printf(TEXT("%sunreal-v%i-%s.dmp"), *FPaths::ProjectLogDir(), FEngineVersion::Current().GetChangelist(), *FDateTime::Now().ToString())));
#endif
	{
		SCOPED_BOOT_TIMING("FPlatformOutputDevices::SetupOutputDevices");
		// Init logging to disk
		FPlatformOutputDevices::SetupOutputDevices();
	}

#if WITH_EDITOR
	// Append any command line overrides when running as a preview device
	if (FPIEPreviewDeviceModule::IsRequestingPreviewDevice())
	{
		FPIEPreviewDeviceModule* PIEPreviewDeviceProfileSelectorModule = FModuleManager::LoadModulePtr<FPIEPreviewDeviceModule>("PIEPreviewDeviceProfileSelector");
		if (PIEPreviewDeviceProfileSelectorModule)
		{
			PIEPreviewDeviceProfileSelectorModule->ApplyCommandLineOverrides();
		}
	}
#endif

	{
		SCOPED_BOOT_TIMING("FConfigCacheIni::InitializeConfigSystem");
		LLM_SCOPE(ELLMTag::ConfigSystem);
		// init config system
		FConfigCacheIni::InitializeConfigSystem();
	}

	FDelayedAutoRegisterHelper::RunAndClearDelayedAutoRegisterDelegates(EDelayedRegisterRunPhase::IniSystemReady);

	// Load "asap" plugin modules
	IPluginManager&  PluginManager = IPluginManager::Get();
	IProjectManager& ProjectManager = IProjectManager::Get();

	{
		SCOPED_BOOT_TIMING("FPlatformStackWalk::Init");
		// Now that configs have been initialized, setup stack walking options
		FPlatformStackWalk::Init();
	}

	CheckForPrintTimesOverride();

	// Check whether the project or any of its plugins are missing or are out of date
#if UE_EDITOR && !IS_MONOLITHIC
	if(!GIsBuildMachine && !FApp::IsUnattended() && FPaths::IsProjectFilePathSet())
	{
		// Check all the plugins are present
		if(!PluginManager.AreRequiredPluginsAvailable())
		{
			return false;
		}

		// Find the editor target
		FString EditorTargetFileName;
		FString DefaultEditorTarget;
		GConfig->GetString(TEXT("/Script/BuildSettings.BuildSettings"), TEXT("DefaultEditorTarget"), DefaultEditorTarget, GEngineIni);

		for (const FTargetInfo& Target : FDesktopPlatformModule::Get()->GetTargetsForProject(FPaths::GetProjectFilePath()))
		{
			if (Target.Type == EBuildTargetType::Editor && (DefaultEditorTarget.Len() == 0 || Target.Name == DefaultEditorTarget))
			{
				if (FPaths::IsUnderDirectory(Target.Path, FPlatformMisc::ProjectDir()))
				{
					EditorTargetFileName = FTargetReceipt::GetDefaultPath(FPlatformMisc::ProjectDir(), *Target.Name, FPlatformProcess::GetBinariesSubdirectory(), FApp::GetBuildConfiguration(), nullptr);
				}
				else if (FPaths::IsUnderDirectory(Target.Path, FPaths::EngineDir()))
				{
					EditorTargetFileName = FTargetReceipt::GetDefaultPath(*FPaths::EngineDir(), *Target.Name, FPlatformProcess::GetBinariesSubdirectory(), FApp::GetBuildConfiguration(), nullptr);
				}
				break;
			}
		}

		// If we're not running the correct executable for the current target, and the listed executable exists, run that instead
		if(LaunchCorrectEditorExecutable(EditorTargetFileName))
		{
			return false;
		}

		// Check if we need to compile
		bool bNeedCompile = false;
		GConfig->GetBool(TEXT("/Script/UnrealEd.EditorLoadingSavingSettings"), TEXT("bForceCompilationAtStartup"), bNeedCompile, GEditorPerProjectIni);
		if(FParse::Param(FCommandLine::Get(), TEXT("SKIPCOMPILE")) || FParse::Param(FCommandLine::Get(), TEXT("MULTIPROCESS")))
		{
			bNeedCompile = false;
		}
		if(!bNeedCompile)
		{
			// Check if any of the project or plugin modules are out of date, and the user wants to compile them.
			TArray<FString> IncompatibleFiles;
			ProjectManager.CheckModuleCompatibility(IncompatibleFiles);

			TArray<FString> IncompatibleEngineFiles;
			PluginManager.CheckModuleCompatibility(IncompatibleFiles, IncompatibleEngineFiles);

			if (IncompatibleFiles.Num() > 0)
			{
				// Log the modules which need to be rebuilt
				for (int Idx = 0; Idx < IncompatibleFiles.Num(); Idx++)
				{
					UE_LOG(LogInit, Warning, TEXT("Incompatible or missing module: %s"), *IncompatibleFiles[Idx]);
				}

				// Build the error message for the dialog box
				FString ModulesList = TEXT("The following modules are missing or built with a different engine version:\n\n");

				int NumModulesToDisplay = (IncompatibleFiles.Num() <= 20)? IncompatibleFiles.Num() : 15;
				for (int Idx = 0; Idx < NumModulesToDisplay; Idx++)
				{
					ModulesList += FString::Printf(TEXT("  %s\n"), *IncompatibleFiles[Idx]);
				}
				if(IncompatibleFiles.Num() > NumModulesToDisplay)
				{
					ModulesList += FString::Printf(TEXT("  (+%d others, see log for details)\n"), IncompatibleFiles.Num() - NumModulesToDisplay);
				}

				// If we're running with -stdout, assume that we're a non interactive process and about to fail
				if (FApp::IsUnattended() || FParse::Param(FCommandLine::Get(), TEXT("stdout")))
				{
					return false;
				}

				// If there are any engine modules that need building, force the user to build through the IDE
				if(IncompatibleEngineFiles.Num() > 0)
				{
					FString CompileForbidden = ModulesList + TEXT("\nEngine modules cannot be compiled at runtime. Please build through your IDE.");
					FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *CompileForbidden, TEXT("Missing Modules"));
					return false;
				}

				// Ask whether to compile before continuing
				FString CompilePrompt = ModulesList + TEXT("\nWould you like to rebuild them now?");
				if (FPlatformMisc::MessageBoxExt(EAppMsgType::YesNo, *CompilePrompt, *FString::Printf(TEXT("Missing %s Modules"), FApp::GetProjectName())) == EAppReturnType::No)
				{
					return false;
				}

				bNeedCompile = true;
			}
			else if(EditorTargetFileName.Len() > 0 && !FPaths::FileExists(EditorTargetFileName))
			{
				// Prompt to compile. The target file isn't essential, but we 
				if (FPlatformMisc::MessageBoxExt(EAppMsgType::YesNo, *FString::Printf(TEXT("The %s file does not exist. Would you like to build the editor?"), *FPaths::GetCleanFilename(EditorTargetFileName)), TEXT("Missing target file")) == EAppReturnType::Yes)
				{
					bNeedCompile = true;
				}
			}
		}

		FEmbeddedCommunication::ForceTick(16);
		
		if(bNeedCompile)
		{
			// Try to compile it
			FFeedbackContext *Context = (FFeedbackContext*)FDesktopPlatformModule::Get()->GetNativeFeedbackContext();
			Context->BeginSlowTask(FText::FromString(TEXT("Starting build...")), true, true);
			ECompilationResult::Type CompilationResult = ECompilationResult::Unknown;
			bool bCompileResult = FDesktopPlatformModule::Get()->CompileGameProject(FPaths::RootDir(), FPaths::GetProjectFilePath(), Context, &CompilationResult);
			Context->EndSlowTask();

			// Check if we're running the wrong executable now
			if(bCompileResult && LaunchCorrectEditorExecutable(EditorTargetFileName))
			{
				return false;
			}

			// Check if we needed to modify engine files
			if (!bCompileResult && CompilationResult == ECompilationResult::FailedDueToEngineChange)
			{
				FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, TEXT("Engine modules are out of date, and cannot be compiled while the engine is running. Please build through your IDE."), TEXT("Missing Modules"));
				return false;
			}

			// Get a list of modules which are still incompatible
			TArray<FString> StillIncompatibleFiles;
			ProjectManager.CheckModuleCompatibility(StillIncompatibleFiles);

			TArray<FString> StillIncompatibleEngineFiles;
			PluginManager.CheckModuleCompatibility(StillIncompatibleFiles, StillIncompatibleEngineFiles);

			if(!bCompileResult || StillIncompatibleFiles.Num() > 0)
			{
				for (int Idx = 0; Idx < StillIncompatibleFiles.Num(); Idx++)
				{
					UE_LOG(LogInit, Warning, TEXT("Still incompatible or missing module: %s"), *StillIncompatibleFiles[Idx]);
				}
				if (!FApp::IsUnattended())
				{
					FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *FString::Printf(TEXT("%s could not be compiled. Try rebuilding from source manually."), FApp::GetProjectName()), TEXT("Error"));
				}
				return false;
			}
		}
	}
#endif

	// Put the command line and config info into the suppression system (before plugins start loading)
	FLogSuppressionInterface::Get().ProcessConfigAndCommandLine();

#if PLATFORM_IOS || PLATFORM_TVOS
	// Now that the config system is ready, init the audio system.
	[[IOSAppDelegate GetDelegate] InitializeAudioSession];
#endif
	

	// Register the callback that allows the text localization manager to load data for plugins
	FCoreDelegates::GatherAdditionalLocResPathsCallback.AddLambda([](TArray<FString>& OutLocResPaths)
	{
		IPluginManager::Get().GetLocalizationPathsForEnabledPlugins(OutLocResPaths);
	});

	FEmbeddedCommunication::ForceTick(17);

	// PreInitHMDDevice();

	// after the above has run we now have the REQUIRED set of engine .INIs  (all of the other .INIs)
	// that are gotten from .h files' config() are not requires and are dynamically loaded when the .u files are loaded

#if !UE_BUILD_SHIPPING
	// Prompt the user for remote debugging?
	bool bPromptForRemoteDebug = false;
	GConfig->GetBool(TEXT("Engine.ErrorHandling"), TEXT("bPromptForRemoteDebugging"), bPromptForRemoteDebug, GEngineIni);
	bool bPromptForRemoteDebugOnEnsure = false;
	GConfig->GetBool(TEXT("Engine.ErrorHandling"), TEXT("bPromptForRemoteDebugOnEnsure"), bPromptForRemoteDebugOnEnsure, GEngineIni);

	if (FParse::Param(FCommandLine::Get(), TEXT("PROMPTREMOTEDEBUG")))
	{
		bPromptForRemoteDebug = true;
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("PROMPTREMOTEDEBUGENSURE")))
	{
		bPromptForRemoteDebug = true;
		bPromptForRemoteDebugOnEnsure = true;
	}

	FPlatformMisc::SetShouldPromptForRemoteDebugging(bPromptForRemoteDebug);
	FPlatformMisc::SetShouldPromptForRemoteDebugOnEnsure(bPromptForRemoteDebugOnEnsure);

	// Feedback context.
	if (FParse::Param(FCommandLine::Get(), TEXT("WARNINGSASERRORS")))
	{
		GWarn->TreatWarningsAsErrors = true;
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("SILENT")))
	{
		GIsSilent = true;
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("RUNNINGUNATTENDEDSCRIPT")))
	{
		GIsRunningUnattendedScript = true;
	}

#endif // !UE_BUILD_SHIPPING

	// Show log if wanted.
	if (GLogConsole && FParse::Param(FCommandLine::Get(), TEXT("LOG")))
	{
		GLogConsole->Show(true);
	}

	// Print all initial startup logging
	FApp::PrintStartupLogMessages();

	// if a logging build, clear out old log files. Avoid races when multiple processes are running at once.
#if !NO_LOGGING
	if (!FParse::Param(FCommandLine::Get(), TEXT("MULTIPROCESS")))
	{
		FMaintenance::DeleteOldLogs();
	}
#endif

#if !UE_BUILD_SHIPPING
	{
		SCOPED_BOOT_TIMING("FApp::InitializeSession");
		FApp::InitializeSession();
	}
#endif

#if PLATFORM_USE_PLATFORM_FILE_MANAGED_STORAGE_WRAPPER
	// Delay initialization of FPersistentStorageManager to a point where GConfig is initialized
	FPersistentStorageManager::Get().Initialize();
#endif

	// Checks.
	check(sizeof(uint8) == 1);
	check(sizeof(int8) == 1);
	check(sizeof(uint16) == 2);
	check(sizeof(uint32) == 4);
	check(sizeof(uint64) == 8);
	check(sizeof(ANSICHAR) == 1);

#if PLATFORM_TCHAR_IS_4_BYTES
	check(sizeof(TCHAR) == 4);
#else
	check(sizeof(TCHAR) == 2);
#endif

	check(sizeof(int16) == 2);
	check(sizeof(int32) == 4);
	check(sizeof(int64) == 8);
	check(sizeof(bool) == 1);
	check(sizeof(float) == 4);
	check(sizeof(double) == 8);

	// Init list of common colors.
	GColorList.CreateColorMap();

	bool bForceSmokeTests = false;
	GConfig->GetBool(TEXT("AutomationTesting"), TEXT("bForceSmokeTests"), bForceSmokeTests, GEngineIni);
	bForceSmokeTests |= FParse::Param(FCommandLine::Get(), TEXT("bForceSmokeTests"));
	FAutomationTestFramework::Get().SetForceSmokeTests(bForceSmokeTests);

	FEmbeddedCommunication::ForceTick(18);

	// Init other systems.
	{
		SCOPED_BOOT_TIMING("FCoreDelegates::OnInit.Broadcast");
		FCoreDelegates::OnInit.Broadcast();
	}

	FEmbeddedCommunication::ForceTick(19);

	return true;
}
