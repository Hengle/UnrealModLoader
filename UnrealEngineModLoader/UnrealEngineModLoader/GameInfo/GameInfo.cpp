#include "GameInfo.h"
#include <iostream>
#include "Utilities/Logger.h"
#include <string>
#include <filesystem>
#include "INI.h"
#include "Utilities/Pattern.h"
#include "../Hooks.h"
#include "../UE4/Ue4.hpp"
GameProfile GameProfile::SelectedGameProfile;

DWORD StringToDWord(std::string str)
{
	unsigned int m_dwIP;
	std::istringstream ss(&str[2]);
	ss >> std::hex >> m_dwIP;
	return m_dwIP;
}

std::string GetModuleFilePath(HMODULE hModule)
{
	std::string ModuleName = "";
	char szFileName[MAX_PATH] = { 0 };

	if (GetModuleFileNameA(hModule, szFileName, MAX_PATH))
		ModuleName = szFileName;

	return ModuleName;
}

void SetupProfile(std::string Path)
{
	char game_c[MAX_PATH];
	GetModuleFileNameA(NULL, game_c, MAX_PATH);
	std::string gamename = std::string(game_c);
	gamename = gamename.substr(0, gamename.find_last_of("."));
	gamename = gamename.substr(gamename.find_last_of("/\\"));
	gamename = gamename.substr(1);
	auto Game = Path + "\\Profiles\\";
	Game = Game + gamename;
	GameProfile::SelectedGameProfile.LoaderPath = Path;
	auto LoaderInfoInI = Path + "\\ModLoaderInfo.ini";
	std::string Profile = Game + ".profile";
	INI::PARSE_FLAGS = INI::PARSE_COMMENTS_ALL | INI::PARSE_COMMENTS_SLASH | INI::PARSE_COMMENTS_HASH;
	INI LoaderInfo(LoaderInfoInI, true);
	//Output File Initialization
	LoaderInfo.select("DEBUG");
	if (LoaderInfo.getAs<int>("DEBUG", "UseConsole", 0) == 1)
	{
		ShowWindow(GetConsoleWindow(), SW_SHOW);
		FreeConsole();
		AllocConsole();
		freopen("CON", "w", LOG_STREAM);
		Log::Info("Created by ~Russell.J Release V2.0.0");
	}
	if (std::filesystem::exists(Profile))
	{
		GameProfile::SelectedGameProfile.ProfileName = gamename;
		Log::Info("Profile Detected: %s", gamename.c_str());
		std::ifstream file("Profile");

		INI GameInfo(Profile, true);
		GameInfo.select("GameInfo");
		GameProfile::SelectedGameProfile.IsUsingFChunkedFixedUObjectArray = GameInfo.getAs<int>("GameInfo", "IsUsingFChunkedFixedUObjectArray", 0);
		GameProfile::SelectedGameProfile.UsesFNamePool = GameInfo.getAs<int>("GameInfo", "UsesFNamePool", 0);
		GameProfile::SelectedGameProfile.IsUsingDeferedSpawn = GameInfo.getAs<int>("GameInfo", "IsUsingDeferedSpawn", 0);
		GameProfile::SelectedGameProfile.IsUsing4_22 = GameInfo.getAs<int>("GameInfo", "IsUsing4_22", 0);
		GameProfile::SelectedGameProfile.bIsDefaultObjectArrayed = GameInfo.getAs<int>("GameInfo", "IsDefaultObjectArrayed", 0);

		if (GameInfo.get("GameInfo", "BeginPlayOverwrite", "") != "")
		{
			GameProfile::SelectedGameProfile.BeginPlayOverwrite = GameInfo.get("GameInfo", "BeginPlayOverwrite", "");
		}

		if (GameInfo.select("GInfo"))
		{
			if (GameInfo.getAs<int>("GInfo", "IsGInfoPatterns", 0) == 1)
			{
				auto GName = (DWORD64)Pattern::Find(GameInfo.get("GInfo", "GName", "").c_str());
				auto GNamesOffset = *reinterpret_cast<uint32_t*>(GName + GameInfo.getAs<int>("GInfo", "GNameFirstOpCodes", 0));
				GameProfile::SelectedGameProfile.GName = (GName + GameInfo.getAs<int>("GInfo", "GNameTotalByteInstruction", 0) + GNamesOffset);


				auto GObject = (DWORD64)Pattern::Find(GameInfo.get("GInfo", "GObject", "").c_str());
				auto GObjectOffset = *reinterpret_cast<uint32_t*>(GObject + GameInfo.getAs<int>("GInfo", "GObjectFirstOpCodes", 0));
				GameProfile::SelectedGameProfile.GObject = (GObject + GameInfo.getAs<int>("GInfo", "GObjectTotalByteInstruction", 0) + GObjectOffset);

				auto GWorld = (DWORD64)Pattern::Find(GameInfo.get("GInfo", "GWorld", "").c_str());
				auto GWorldOffset = *reinterpret_cast<uint32_t*>(GWorld + GameInfo.getAs<int>("GInfo", "GWorldFirstOpCodes", 0));
				GameProfile::SelectedGameProfile.GWorld = (GWorld + GameInfo.getAs<int>("GInfo", "GWorldTotalByteInstruction", 0) + GWorldOffset);

				Log::Info("GSTuff Patterns Loaded");
			}
			else
			{
				GameProfile::SelectedGameProfile.GName = (DWORD64)GetModuleHandleW(0) + StringToDWord(GameInfo.get("GInfo", "GName", ""));
				GameProfile::SelectedGameProfile.GObject = (DWORD64)GetModuleHandleW(0) + StringToDWord(GameInfo.get("GInfo", "GObject", ""));
				GameProfile::SelectedGameProfile.GWorld = (DWORD64)GetModuleHandleW(0) + StringToDWord(GameInfo.get("GInfo", "GWorld", ""));
				Log::Info("GSTuff Offset Loaded");
			}
		}
		else
		{
			if (GameProfile::SelectedGameProfile.UsesFNamePool)
			{
				auto FPoolPat = Pattern::Find("74 09 48 8D 15 ? ? ? ? EB 16");
				if (FPoolPat != nullptr)
				{
					auto FPoolPatoffset = *reinterpret_cast<uint32_t*>(FPoolPat + 5);
					GameProfile::SelectedGameProfile.GName = (DWORD64)(FPoolPat + 9 + FPoolPatoffset);
					Log::Info("FoundNamePool: 0x%p", GameProfile::SelectedGameProfile.GName);
				}
				else
				{
					Log::Error("GName Could Not Be Found!");
				}
			}
			else
			{
				auto GNamePat = Pattern::Find("E8 ? ? ? ? 48 8B C3 48 89 1D ? ? ? ? 48 8B 5C 24");
				if (GNamePat != nullptr)
				{
					auto GNamesAddress = *reinterpret_cast<uint32_t*>(GNamePat + 11);
					GameProfile::SelectedGameProfile.GName = (DWORD64)(GNamePat + 15 + GNamesAddress);
					Log::Info("GName: 0x%p", GameProfile::SelectedGameProfile.GName);
				}
				else
				{
					Log::Error("GName Could Not Be Found!");
				}
			}

			auto GObjectPat = Pattern::Find("8B 46 10 3B 46 3C 75 0F 48 8B D6 48 8D 0D ? ? ? ? E8");
			if (GObjectPat != nullptr)
			{
				auto GObjectOffset = *reinterpret_cast<uint32_t*>(GObjectPat + 14);
				GameProfile::SelectedGameProfile.GObject = (DWORD64)(GObjectPat + 18 + GObjectOffset);
				Log::Info("GObject: 0x%p", GameProfile::SelectedGameProfile.GObject);
			}
			else
			{
				Log::Error("GObject Could Not Be Found!");
			}
			
			auto GWorldPat = Pattern::Find("0F 2E ? 74 ? 48 8B 1D ? ? ? ? 48 85 DB 74");
			if (GWorldPat != nullptr)
			{
				auto GWorldAddress = *reinterpret_cast<uint32_t*>(GWorldPat + 8);
				GameProfile::SelectedGameProfile.GWorld = (DWORD64)(GWorldPat + 12 + GWorldAddress);
				Log::Info("GWorld: 0x%p", GameProfile::SelectedGameProfile.GWorld);
			}
			else
			{
				Log::Error("GWorld Could Not Be Found!");
			}
		}

		if (GameInfo.select("UObjectDef"))
		{
			GameProfile::SelectedGameProfile.defs.UObject.Index = StringToDWord(GameInfo.get("UObjectDef", "Index", ""));
			GameProfile::SelectedGameProfile.defs.UObject.Class = StringToDWord(GameInfo.get("UObjectDef", "Class", ""));
			GameProfile::SelectedGameProfile.defs.UObject.Name = StringToDWord(GameInfo.get("UObjectDef", "Name", ""));
			GameProfile::SelectedGameProfile.defs.UObject.Outer = StringToDWord(GameInfo.get("UObjectDef", "Outer", ""));
			
			GameInfo.select("UFieldDef");
			GameProfile::SelectedGameProfile.defs.UField.Next = StringToDWord(GameInfo.get("UFieldDef", "Next", ""));
			
			GameInfo.select("UStructDef");
			GameProfile::SelectedGameProfile.defs.UStruct.SuperStruct = StringToDWord(GameInfo.get("UStructDef", "SuperStruct", ""));
			GameProfile::SelectedGameProfile.defs.UStruct.Children = StringToDWord(GameInfo.get("UStructDef", "Children", ""));
			GameProfile::SelectedGameProfile.defs.UStruct.PropertiesSize = StringToDWord(GameInfo.get("UStructDef", "PropertiesSize", ""));
			
			GameInfo.select("UFunctionDef");
			GameProfile::SelectedGameProfile.defs.UFunction.FunctionFlags = StringToDWord(GameInfo.get("UFunctionDef", "FunctionFlags", ""));
			GameProfile::SelectedGameProfile.defs.UFunction.Func = StringToDWord(GameInfo.get("UFunctionDef", "Func", ""));
			
		}
		else
		{
			GameProfile::SelectedGameProfile.IsEngineDefsMissing = true;
		}

		if (GameInfo.select("Property"))
		{
			GameProfile::SelectedGameProfile.bIsFProperty = GameInfo.getAs<int>("Property", "IsFProperty", 0);
			GameProfile::SelectedGameProfile.defs.Property.ArrayDim = StringToDWord(GameInfo.get("Property", "ArrayDim", ""));
			GameProfile::SelectedGameProfile.defs.Property.Offset = StringToDWord(GameInfo.get("Property", "Offset", ""));
			if (GameProfile::SelectedGameProfile.bIsFProperty)
			{
				GameInfo.select("FField");
				GameProfile::SelectedGameProfile.defs.FField.Name = StringToDWord(GameInfo.get("FField", "Name", ""));
				GameProfile::SelectedGameProfile.defs.FField.Next = StringToDWord(GameInfo.get("FField", "Next", ""));
			}
		}
		else
		{
			GameProfile::SelectedGameProfile.IsPropertyMissing = true;
		}


		if (GameInfo.select("FunctionInfo"))
		{
			if (GameInfo.getAs<int>("FunctionInfo", "IsFunctionPatterns", 0) == 0)
			{
				GameProfile::SelectedGameProfile.GameStateInit = (DWORD64)GetModuleHandleW(0) + StringToDWord(GameInfo.get("FunctionInfo", "GameStateInit", ""));
				GameProfile::SelectedGameProfile.BeginPlay = (DWORD64)GetModuleHandleW(0) + StringToDWord(GameInfo.get("FunctionInfo", "BeginPlay", ""));
				GameProfile::SelectedGameProfile.StaticLoadObject = (DWORD64)GetModuleHandleW(0) + StringToDWord(GameInfo.get("FunctionInfo", "StaticLoadObject", ""));
				GameProfile::SelectedGameProfile.SpawnActorFTrans = (DWORD64)GetModuleHandleW(0) + StringToDWord(GameInfo.get("FunctionInfo", "SpawnActorFTrans", ""));
				GameProfile::SelectedGameProfile.ProcessEvent = (DWORD64)GetModuleHandleW(0) + StringToDWord(GameInfo.get("FunctionInfo", "ProcessEvent", ""));
				GameProfile::SelectedGameProfile.CreateDefaultObject = (DWORD64)GetModuleHandleW(0) + StringToDWord(GameInfo.get("FunctionInfo", "CreateDefaultObject", ""));
				Log::Info("Function Offsets Set!");
			}
			else
			{
				GameProfile::SelectedGameProfile.GameStateInit = (DWORD64)Pattern::Find(GameInfo.get("FunctionInfo", "GameStateInit", "").c_str());
				GameProfile::SelectedGameProfile.BeginPlay = (DWORD64)Pattern::Find(GameInfo.get("FunctionInfo", "BeginPlay", "").c_str());
				GameProfile::SelectedGameProfile.StaticLoadObject = (DWORD64)Pattern::Find(GameInfo.get("FunctionInfo", "StaticLoadObject", "").c_str());
				GameProfile::SelectedGameProfile.SpawnActorFTrans = (DWORD64)Pattern::Find(GameInfo.get("FunctionInfo", "SpawnActorFTrans", "").c_str());
				GameProfile::SelectedGameProfile.ProcessEvent = (DWORD64)Pattern::Find(GameInfo.get("FunctionInfo", "ProcessEvent", "").c_str());
				GameProfile::SelectedGameProfile.CreateDefaultObject = (DWORD64)Pattern::Find(GameInfo.get("FunctionInfo", "CreateDefaultObject", "").c_str());
				Log::Info("Function Patterns Set!");
			}
		}
		else
		{
			GameProfile::SelectedGameProfile.GameStateInit = (DWORD64)Pattern::Find("40 53 48 83 EC 20 48 8B 41 10 48 8B D9 48 8B 91");
			std::cout << "GameProfile::SelectedGameProfile.GameStateInit: " << (void*)GameProfile::SelectedGameProfile.GameStateInit << std::endl;
			if (!GameProfile::SelectedGameProfile.GameStateInit)
			{
				Log::Error("GameStateInit NOT FOUND!");
			}

			auto BeginPlay = Pattern::Find("48 8B D9 E8 ?? ?? ?? ?? F6 83 ?? ?? ?? ?? ?? 74 12 48 8B 03");
			BeginPlay += 0x3;
			if (BeginPlay != nullptr)
			{
				GameProfile::SelectedGameProfile.BeginPlay = (DWORD64)MEM::GetAddressPTR(BeginPlay, 0x1, 0x5);
				std::cout << "GameProfile::SelectedGameProfile.BeginPlay: " << (void*)GameProfile::SelectedGameProfile.BeginPlay << std::endl;
			}
			else
			{
				Log::Error("AActor::BeginPlay NOT FOUND!");
			}

			auto StaticLoadObject = Pattern::Find("89 64 24 ? 48 8B C8 E8 ? ? ? ? 41 BE ? ? ? ? EB 05 E8"); // Sig 1
			if (StaticLoadObject != nullptr)
			{
				StaticLoadObject += 0x7;
				GameProfile::SelectedGameProfile.StaticLoadObject = (DWORD64)MEM::GetAddressPTR(StaticLoadObject, 0x1, 0x5);
				std::cout << "GameProfile::SelectedGameProfile.StaticLoadObject: " << (void*)GameProfile::SelectedGameProfile.StaticLoadObject << std::endl;
			}
			else
			{
				StaticLoadObject = Pattern::Find("C7 44 24 ? ? ? ? ? E8 ? ? ? ? 48 8B 8D ? ? ? ? 48 85 C9 74 05 E8 ? ? ? ? 45 33 C9 ? 89 74 24");
				if (StaticLoadObject != nullptr)
				{
					StaticLoadObject += 0x8;
					GameProfile::SelectedGameProfile.StaticLoadObject = (DWORD64)MEM::GetAddressPTR(StaticLoadObject, 0x1, 0x5);
					std::cout << "GameProfile::SelectedGameProfile.StaticLoadObject: " << (void*)GameProfile::SelectedGameProfile.StaticLoadObject << std::endl;
				}
				else
				{
					StaticLoadObject = Pattern::Find("89 6C 24 20 48 8B C8 E8 ? ? ? ? 48 8B 4C 24 ? 48 8B F0 48 85 C9 74 05");
					if (StaticLoadObject != nullptr)
					{
						StaticLoadObject += 0x7;
						GameProfile::SelectedGameProfile.StaticLoadObject = (DWORD64)MEM::GetAddressPTR(StaticLoadObject, 0x1, 0x5);
						std::cout << "GameProfile::SelectedGameProfile.StaticLoadObject: " << (void*)GameProfile::SelectedGameProfile.StaticLoadObject << std::endl;
					}
					else
					{
						if (StaticLoadObject = Pattern::Find("48 8B C8 89 5C 24 20 E8 ? ? ? ? 48"))
						{
							StaticLoadObject += 0x7;
							GameProfile::SelectedGameProfile.StaticLoadObject = (DWORD64)MEM::GetAddressPTR(StaticLoadObject, 0x1, 0x5);
							std::cout << "GameProfile::SelectedGameProfile.StaticLoadObject: " << (void*)GameProfile::SelectedGameProfile.StaticLoadObject << std::endl;
						}
						else
						{
							Log::Error("StaticLoadObject NOT FOUND!");
						}
					}
				}
			}
			//Second 4C 8B CE 4C 8D 44 24 ? 48 8B D7 48 8B CB E8 ? ? ? ? 48 8B 4C 24 ? 48 33 CC 
			auto SpawnActorFTrans = Pattern::Find("4C 8B C6 48 8B C8 48 8B D3 E8 ? ? ? ? 48 8B 5C 24 ? 48 8B 74 24");
			if (SpawnActorFTrans != nullptr)
			{
				SpawnActorFTrans += 0x9;
				GameProfile::SelectedGameProfile.SpawnActorFTrans = (DWORD64)MEM::GetAddressPTR(SpawnActorFTrans, 0x1, 0x5);
				std::cout << "GameProfile::SelectedGameProfile.SpawnActorFTrans: " << (void*)GameProfile::SelectedGameProfile.SpawnActorFTrans << std::endl;
			}
			else
			{
				SpawnActorFTrans = Pattern::Find("4C 8B CE 4C 8D 44 24 ? 48 8B D7 48 8B CB E8 ? ? ? ? 48 8B 4C 24 ? 48 33 CC");
				if (SpawnActorFTrans != nullptr)
				{
					SpawnActorFTrans += 0xE;
					GameProfile::SelectedGameProfile.SpawnActorFTrans = (DWORD64)MEM::GetAddressPTR(SpawnActorFTrans, 0x1, 0x5);
					std::cout << "GameProfile::SelectedGameProfile.SpawnActorFTrans: " << (void*)GameProfile::SelectedGameProfile.SpawnActorFTrans << std::endl;
				}
				else
				{
					Log::Error("SpawnActorFTrans NOT FOUND!");
				}
			}

			auto ProcessEvent = Pattern::Find("75 0E ? ? ? 48 ? ? 48 ? ? E8 ? ? ? ? 48 8B ? 24 ? 48 8B ? 24 38 48 8B ? 24 40");
			ProcessEvent += 0xB;
			if (ProcessEvent != nullptr)
			{
				GameProfile::SelectedGameProfile.ProcessEvent = (DWORD64)MEM::GetAddressPTR(ProcessEvent, 0x1, 0x5);
				std::cout << "GameProfile::SelectedGameProfile.ProcessEvent: " << (void*)GameProfile::SelectedGameProfile.ProcessEvent << std::endl;
			}
			else
			{
				Log::Error("ProcessEvent NOT FOUND!");
			}

			GameProfile::SelectedGameProfile.CreateDefaultObject = (DWORD64)Pattern::Find("4C 8B DC 57 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 84 24 ? ? ? ? 48 83 B9 ? ? ? ? ? 48 8B F9 ");
			if (!GameProfile::SelectedGameProfile.CreateDefaultObject)
			{
				//FallBack 1
				GameProfile::SelectedGameProfile.CreateDefaultObject = (DWORD64)Pattern::Find("4C 8B DC 55 53 49 8D AB ? ? ? ? 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 85 ? ? ? ? 48 83 B9 ? ? ? ? ? 48 8B D9 0F 85");
				if (!GameProfile::SelectedGameProfile.CreateDefaultObject)
				{
					//FallBack 2
					GameProfile::SelectedGameProfile.CreateDefaultObject = (DWORD64)Pattern::Find("4C 8B DC 53 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 84 24 ? ? ? ? 48 83 B9");
					if (!GameProfile::SelectedGameProfile.CreateDefaultObject)
					{
						//Final FallBack
						GameProfile::SelectedGameProfile.CreateDefaultObject = (DWORD64)Pattern::Find("4C 8B DC ?? ?? ?? ?? ?? ? ? ? ? ?? ?? ?? ? ? ? ? ?? ?? ?? ? ? ? ? ?? ?? ?? 48 ?? ?? ? ? ? ? ?? ?? ?? ? ? ? ? ? ?? ?? ?? ?? ?? ? ? ? ? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ? ? ? ? ?? 8B ?? ? ? ? ? ?? ?? ?? ?? ?? ?? 8B ?? ?? ?? ?? ?? ?? ? ? ? ? ?? ?? ?? ? ? ? ? ?? ?? ?? ?? ?? ?? ? ? ? ? ?? ?? ?? ?? ?? ? ? ? ? ?? ?? ? ? ? ? ?? ?? ?? 48");
						if (!GameProfile::SelectedGameProfile.CreateDefaultObject)
						{
							GameProfile::SelectedGameProfile.bIsDefaultObjectArrayed = true;
							Log::Warn("CreateDefualtObject NOT FOUND!, Will Use Object Array Instead!");
						}
					}
				}
			}
			std::cout << "GameProfile::SelectedGameProfile.CreateDefualtObject: " << (void*)GameProfile::SelectedGameProfile.CreateDefaultObject << std::endl;
			//Log::SetupErrorMessage("Function Information Could Not Be Found!");
		}
		Log::Info("Setup %s", gamename.c_str());
		Hooks::SetupHooks();
	}
	else
	{
		Log::Error("Profile %s Not Detected!", Profile.c_str());
	}
}

void GameProfile::CreateGameProfile()
{
	auto Module = GetModuleHandleA("UnrealEngineModLoader.dll");
	std::string path = GetModuleFilePath(Module);
	path = path.substr(0, path.find_last_of("/\\"));
	SetupProfile(path);
}