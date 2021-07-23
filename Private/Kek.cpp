// UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, AssetName);
	// TRefCountPtr<FUObjectSerializeContext> LoadContext(FUObjectThreadContext::Get().GetSerializeContext());
	// check(LoadContext);
	// std::cout << "Created thread context successfully!" << std::endl;
	//
	// UPackage* Package = CreatePackage(AssetName);
	//
	// BeginLoad(LoadContext, AssetName);
	//
	// bool bFullyLoadSkipped = false;
	//
	// const double StartTime = FPlatformTime::Seconds();
	// uint32 LoadFlags = 0;
	// FLinkerLoad* Linker =
	// FLinkerLoad::CreateLinker(LoadContext, Package, AssetPath, 0, nullptr, nullptr);
	//
	// std::cout << FPlatformProperties::RequiresCookedData() << std::endl;
	//
	// if (!Linker)
	// {
	// 	EndLoad(LoadContext);
	// 	exit(EXIT_FAILURE);
	// }
	//
	// UPackage* Result = Linker->LinkerRoot;
	// checkf(Result, TEXT("LinkerRoot is null"));
	//
	// auto EndLoadAndCopyLocalizationGatherFlag = [&]
	// {
	// 	EndLoad(Linker->GetSerializeContext());
	// 	// Set package-requires-localization flags from archive after loading. This reinforces flagging of packages that haven't yet been resaved.
	// 	Result->ThisRequiresLocalizationGather(Linker->RequiresLocalizationGather());
	// };
	//
	// if (Result->HasAnyFlags(RF_WasLoaded))
	// {
	// 	// The linker is associated with a package that has already been loaded.
	// 	// Loading packages that have already been loaded is unsupported.
	// 	EndLoadAndCopyLocalizationGatherFlag();	
	// }
	//
	// FExclusiveLoadPackageTimeTracker::FScopedPackageTracker Tracker(Result);
	//
	// if(LoadFlags & LOAD_ForDiff)
	// {
	// 	Result->SetPackageFlags(PKG_ForDiffing);
	// }
	//
	// {
	// 	// convert will succeed here, otherwise the linker will have been null
	// 	FString LongPackageFilename;
	// 	FPackageName::TryConvertFilenameToLongPackageName(AssetName, LongPackageFilename);
	// 	Result->FileName = FName(*LongPackageFilename);
	// }
	//
	// // is there a script SHA hash for this package?
	// uint8 SavedScriptSHA[20];
	// bool bHasScriptSHAHash = FSHA1::GetFileSHAHash(*Linker->LinkerRoot->GetName(), SavedScriptSHA, false);
	// if (bHasScriptSHAHash)
	// {
	// 	// if there is, start generating the SHA for any script code in this package
	// 	Linker->StartScriptSHAGeneration();
	// }
	// uint32 DoNotLoadExportsFlags = LOAD_Verify;
	// DoNotLoadExportsFlags |= LOAD_DeferDependencyLoads;
	// if ((LoadFlags & DoNotLoadExportsFlags) == 0)
	// {
	// 	// Make sure we pass the property that's currently being serialized by the linker that owns the import 
	// 	// that triggered this LoadPackage call
	// 	FSerializedPropertyScope SerializedProperty(*Linker, Linker->GetSerializedProperty());
	// 	Linker->LoadAllObjects(GEventDrivenLoaderEnabled);
	//
	// 	// @todo: remove me when loading can be self-contained (and EndLoad doesn't check for IsInAsyncLoadingThread) or there's just one loading path
	// 	// If we start a non-async loading during async loading and the serialization context is not associated with any other package and
	// 	// doesn't come from an async package, queue this package to be async loaded, otherwise we'll end up not loading its exports
	// 	if (!Linker->AsyncRoot && Linker->GetSerializeContext()->GetBeginLoadCount() == 1 && IsInAsyncLoadingThread())
	// 	{
	// 		LoadPackageAsync(Linker->LinkerRoot->GetName());
	// 	}
	// }
	// else
	// {
	// 	bFullyLoadSkipped = true;
	// }
	//
	// Linker->FinishExternalReadDependencies(0.0);
	//
	// EndLoadAndCopyLocalizationGatherFlag();
	//
	// if (bHasScriptSHAHash)
	// {
	// 	// now get the actual hash data
	// 	uint8 LoadedScriptSHA[20];
	// 	Linker->GetScriptSHAKey(LoadedScriptSHA);
	//
	// 	// compare SHA hash keys
	// 	if (FMemory::Memcmp(SavedScriptSHA, LoadedScriptSHA, 20) != 0)
	// 	{
	// 		appOnFailSHAVerification(*Linker->Filename, false);
	// 	}
	// }
	//
	// if( Result && !LoadContext->HasLoadedObjects() && !(LoadFlags & LOAD_Verify) )
	// {
	// 	Result->SetLoadTime( FPlatformTime::Seconds() - StartTime );
	// }
	//
	// Linker->Flush();
	//
	// if (!FPlatformProperties::RequiresCookedData())
	// {
	// 	// Flush cache on uncooked platforms to free precache memory
	// 	Linker->FlushCache();
	// }
	//
	// if (FPlatformProperties::RequiresCookedData())
	// {
	// 	if (!IsInAsyncLoadingThread())
	// 	{				
	// 		if (GGameThreadLoadCounter == 0)
	// 		{
	// 			// Sanity check to make sure that Linker is the linker that loaded our Result package or the linker has already been detached
	// 			check(!Result || Result->LinkerLoad == Linker || Result->LinkerLoad == nullptr);
	// 			if (Result && Linker->HasLoader())
	// 			{
	// 				ResetLoaders(Result);
	// 			}
	// 			// Reset loaders could have already deleted Linker so guard against deleting stale pointers
	// 			if (Result && Result->LinkerLoad)
	// 			{
	// 				Linker->DestroyLoader();
	// 			}
	// 			// And make sure no one can use it after it's been deleted
	// 			Linker = nullptr;
	// 		}
	// 		// Async loading removes delayed linkers on the game thread after streaming has finished
	// 		else
	// 		{
	// 			check(Linker->GetSerializeContext());
	// 			Linker->GetSerializeContext()->AddDelayedLinkerClosePackage(Linker);
	// 		}
	// 	}
	// }
	//
	// if (!bFullyLoadSkipped)
	// {
	// 	// Mark package as loaded.
	// 	Result->SetFlags(RF_WasLoaded);
	// }
	//
	// UObject* ResultObject = StaticFindObjectFast(UBlueprint::StaticClass(), Result, AssetShortName);
	// if (!ResultObject)
	// {
	// 	std::cerr << "That's sad :(" << std::endl;
	// 	LoadPackage(nullptr, *Result->GetOutermost()->GetName(), 0, nullptr);
	// 	ResultObject = StaticFindObjectFast(UBlueprint::StaticClass(), Result, AssetShortName);
	//
	// 	if (!ResultObject)
	// 	{
	// 		std::cerr << "That's very sad :(" << std::endl;
	// 	}
	// }