// Fill out your copyright notice in the Description page of Project Settings.


#include "LobbyGameMode.h"
#include "GameLiftServerSDK.h"
#include "Runtime/Engine/Classes/GameFramework/Actor.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY(LogMyServerLobby);

ALobbyGameMode::ALobbyGameMode()
{
	bUseSeamlessTravel = true;
	GameReady = false;
	SomeoneJoined = false;
	TimePassed = 0;
	static ConstructorHelpers::FClassFinder<APawn> LobbyPawnBPClass(TEXT("/Game/ThirdPersonCPP/Blueprints/BP_LobbyCharacter"));
	static ConstructorHelpers::FClassFinder<APawn> SpectatorPawnBPClass(TEXT("/Game/ThirdPersonCPP/Blueprints/BP_SpectatorCharacter"));
	if (LobbyPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = LobbyPawnBPClass.Class;
		//HUDClass = AMainMenuHUD::StaticClass();
	}
#if WITH_GAMELIFT
	//Getting the module first.
	FGameLiftServerSDKModule* gameLiftSdkModule = &FModuleManager::LoadModuleChecked<FGameLiftServerSDKModule>(FName("GameLiftServerSDK"));

	//InitSDK establishes a local connection with GameLift's agent to enable communication.
	gameLiftSdkModule->InitSDK();

	//Respond to new game session activation request. GameLift sends activation request 
	//to the game server along with a game session object containing game properties 
	//and other settings. Once the game server is ready to receive player connections, 
	//invoke GameLiftServerAPI.ActivateGameSession()
	auto onGameSession = [=](Aws::GameLift::Server::Model::GameSession gameSession)
	{
		gameLiftSdkModule->ActivateGameSession();
	};

	FProcessParameters* params = new FProcessParameters();
	params->OnStartGameSession.BindLambda(onGameSession);

	//OnProcessTerminate callback. GameLift invokes this before shutting down the instance 
	//that is hosting this game server to give it time to gracefully shut down on its own. 
	//In this example, we simply tell GameLift we are indeed going to shut down.
	params->OnTerminate.BindLambda([=]() {gameLiftSdkModule->ProcessEnding(); });

	//HealthCheck callback. GameLift invokes this callback about every 60 seconds. By default, 
	//GameLift API automatically responds 'true'. A game can optionally perform checks on 
	//dependencies and such and report status based on this info. If no response is received  
	//within 60 seconds, health status is recorded as 'false'. 
	//In this example, we're always healthy!
	params->OnHealthCheck.BindLambda([]() {return true; });

	//Here, the game server tells GameLift what port it is listening on for incoming player 
	//connections. In this example, the port is hardcoded for simplicity. Since active game
	//that are on the same instance must have unique ports, you may want to assign port values
	//from a range, such as:
	//const int32 port = FURL::UrlConfig.DefaultPort;
	//params->port;
	params->port = GetWorld()->URL.Port;

	//Here, the game server tells GameLift what set of files to upload when the game session 
	//ends. GameLift uploads everything specified here for the developers to fetch later.
	TArray<FString> logfiles;
	logfiles.Add(TEXT("aLogFile.txt"));
	params->logParameters = logfiles;

	//Call ProcessReady to tell GameLift this game server is ready to receive game sessions!
	gameLiftSdkModule->ProcessReady(*params);

#endif
}

void ALobbyGameMode::BeginPlay() {
	Super::BeginPlay();
	FTimerHandle InactivityTimerHandle;
	GetWorldTimerManager().SetTimer(InactivityTimerHandle, this, &ALobbyGameMode::ServerCheckInactivity, 1.0f, true);
}

void ALobbyGameMode::StartPlay() {
	Super::StartPlay();
	//UGameplayStatics::OpenLevel((UObject*)GetWorld(), FName(TEXT("Level_BattleRoyale")));
}

void ALobbyGameMode::PostLogin(APlayerController* NewPlayer) {
	Super::PostLogin(NewPlayer);
	SomeoneJoined = true;
	if (!GameReady && GetNumPlayers() >= 4) {
		GameReady = true;
		// start the game (server travel) in 60 seconds once there's at least 4 players in the lobby
		FTimerHandle ServerTravelTimerHandle;
		GetWorldTimerManager().SetTimer(ServerTravelTimerHandle, this, &ALobbyGameMode::ServerStartGame, 60.0f, false);
	}
}

void ALobbyGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) {
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	#if WITH_GAMELIFT
		if (*Options && Options.Len() > 0) {
			const FString PlayerSessionId = UGameplayStatics::ParseOption(Options, "PlayerSessionId");
			if (PlayerSessionId.Len() > 0) {
				FGameLiftServerSDKModule* gameLiftSdkModule = &FModuleManager::LoadModuleChecked<FGameLiftServerSDKModule>(FName("GameLiftServerSDK"));
				FGameLiftGenericOutcome outcome = gameLiftSdkModule->AcceptPlayerSession(PlayerSessionId);
				UE_LOG(LogMyServerLobby, Log, TEXT("LobbyGameMode::PreLogin: Client connecting with attempting to connect with GameLift PlayerSessionId: %s"), *PlayerSessionId);
				if (!outcome.IsSuccess())
				{
					ErrorMessage = outcome.GetError().m_errorMessage;
					UE_LOG(LogMyServerLobby, Log, TEXT("LobbyGameMode::PreLogin: Client connecting with invalid GameLift PlayerSessionId: %s, Error: %s"), *PlayerSessionId, **ErrorMessage);
				}
			}
		}
		else {
			UE_LOG(LogMyServerLobby, Log, TEXT("Options does not exist"));
		}
	#endif
}

FString ALobbyGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal) {
	return Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
/*#if WITH_GAMELIFT
	if (*Options && Options.Len() > 0) {
		const FString PlayerSessionId = UGameplayStatics::ParseOption(Options, "PlayerSessionId");
		if (PlayerSessionId.Len() > 0) {
			FGameLiftServerSDKModule* gameLiftSdkModule = &FModuleManager::LoadModuleChecked<FGameLiftServerSDKModule>(FName("GameLiftServerSDK"));
			FGameLiftGenericOutcome outcome = gameLiftSdkModule->AcceptPlayerSession(PlayerSessionId);
			UE_LOG(LogMyServerLobby, Log, TEXT("LobbyGameMode::PreLogin: Client connecting with attempting to connect with GameLift PlayerSessionId: %s"), *PlayerSessionId);
			if (!outcome.IsSuccess())
			{
				ErrorMessage = outcome.GetError().m_errorMessage;
				UE_LOG(LogMyServerLobby, Log, TEXT("LobbyGameMode::PreLogin: Client connecting with invalid GameLift PlayerSessionId: %s, Error: %s"), *PlayerSessionId, **ErrorMessage);
			}
		}
	}
	else {
		UE_LOG(LogMyServerLobby, Log, TEXT("Options does not exist"));
	}
#endif*/

}

void ALobbyGameMode::Logout(AController* Exiting) {
	Super::Logout(Exiting);
	/*if (GetNumPlayers() < 1) {
#if WITH_GAMELIFT
		FGameLiftServerSDKModule* gameLiftSdkModule = &FModuleManager::LoadModuleChecked<FGameLiftServerSDKModule>(FName("GameLiftServerSDK"));
		FGameLiftGenericOutcome outcome = gameLiftSdkModule->TerminateGameSession();
		UE_LOG(LogMyServerLobby, Log, TEXT("LobbyGameMode::EndGame"));
		if (!outcome.IsSuccess())
		{
			const FString ErrorMessage = outcome.GetError().m_errorMessage;
			UE_LOG(LogMyServerLobby, Log, TEXT("LobbyGameMode::EndGame: Error: %s"), *ErrorMessage);
		}
#endif
	}*/
}

void ALobbyGameMode::ServerStartGame_Implementation() {
	// make the game unjoinable
	#if WITH_GAMELIFT
	FGameLiftServerSDKModule* gameLiftSdkModule = &FModuleManager::LoadModuleChecked<FGameLiftServerSDKModule>(FName("GameLiftServerSDK"));
	FGameLiftGenericOutcome outcome = gameLiftSdkModule->UpdatePlayerSessionCreationPolicy(EPlayerSessionCreationPolicy::DENY_ALL);
	UE_LOG(LogMyServerLobby, Log, TEXT("LobbyGameMode::UpdatePlayerSessionCreationPolicy: deny all new player sessions"));
	if (!outcome.IsSuccess())
	{
		const FString ErrorMessage = outcome.GetError().m_errorMessage;
		UE_LOG(LogMyServerLobby, Log, TEXT("LobbyGameMode::UpdatePlayerSessionCreationPolicy: Error: %s"), *ErrorMessage);
	}
	#endif
	FString contentDir = FPaths::ProjectContentDir();
	FString mapPath = contentDir + "/Maps/Level_BattleRoyale_2.umap";
	GetWorld()->ServerTravel(mapPath);
}

bool ALobbyGameMode::ServerStartGame_Validate() {
	return true;
}

void ALobbyGameMode::ServerCheckInactivity_Implementation() {
	// make the game unjoinable
	if (SomeoneJoined && GetNumPlayers() < 1) {
		TimePassed += 1;
		if (TimePassed == 10) {
#if WITH_GAMELIFT
			/*FGameLiftServerSDKModule* gameLiftSdkModule = &FModuleManager::LoadModuleChecked<FGameLiftServerSDKModule>(FName("GameLiftServerSDK"));
			FGameLiftGenericOutcome outcome = gameLiftSdkModule->TerminateGameSession();
			UE_LOG(LogMyServerLobby, Log, TEXT("LobbyGameMode::EndGame"));
			if (!outcome.IsSuccess())
			{
				const FString ErrorMessage = outcome.GetError().m_errorMessage;
				UE_LOG(LogMyServerLobby, Log, TEXT("LobbyGameMode::EndGame: Error: %s"), *ErrorMessage);
			}*/
#endif
			TimePassed = 0;
		}
	}
	else {
		TimePassed = 0;
	}
}

bool ALobbyGameMode::ServerCheckInactivity_Validate() {
	return true;
}