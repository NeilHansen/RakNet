
#include "MessageIdentifiers.h"
#include "RakPeerInterface.h"
#include "BitStream.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <map>
#include <mutex>

static unsigned int SERVER_PORT = 65000;
static unsigned int CLIENT_PORT = 65001;
static unsigned int MAX_CONNECTIONS = 3;

enum NetworkState
{
	NS_Init = 0,
	NS_PendingStart,
	NS_Started,
	NS_Lobby,
	NS_Pending,
	NS_Game,
};

enum GameState
{
	GS_Init = 0,
	GS_PLAYERSREADY,
	GS_CLASSCHOISE,

};

bool isServer = false;
bool isRunning = true;
int activePlayers = 0;
int playersReady = 0;

RakNet::RakPeerInterface *g_rakPeerInterface = nullptr;
RakNet::SystemAddress g_serverAddress;

std::mutex g_networkState_mutex;
NetworkState g_networkState = NS_Init;
GameState g_gameState = GS_Init;
using namespace RakNet;


enum {
	ID_THEGAME_LOBBY_READY = ID_USER_PACKET_ENUM,
	ID_PLAYER_READY,
	ID_THEGAME_CHARACTERSELECT,
	ID_THEGAME_INTRO,
	ID_THEGAME_CHARACTERCHOSEN,
	ID_THEGAME_PLAYING,
	ID_THEGAME_WINNER,
	ID_THEGAME_GAMEOVER
};

enum EPlayerClass
{
	Mage = 0,
	Rogue,
	Warrior,
	Count,
};

struct SPlayer
{
	//getters setters
	//SPlayer() : m_class(Count), m_health(0), m_name() {}
	bool IsAlive() const { return m_health > 0; }

	std::string  m_name;
	unsigned int m_health;
	unsigned int m_atk;
	unsigned int m_def;
	std::string m_class;
	// is active player bool
	bool isPlaying = false;
	

	//function to send a packet with name/health/class etc
	void SendName(RakNet::SystemAddress systemAddress, bool isBroadcast)
	{
		RakNet::BitStream writeBs;
		writeBs.Write((RakNet::MessageID)ID_PLAYER_READY);
		RakNet::RakString name(m_name.c_str());
		writeBs.Write(name);

		//returns 0 when something is wrong
		assert(g_rakPeerInterface->Send(&writeBs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, systemAddress, isBroadcast));
	}
};
SPlayer character_t;
SPlayer *  pcharacter_t;

std::map<unsigned long, SPlayer> m_players;

SPlayer& GetPlayer(RakNet::RakNetGUID raknetId)
{
	unsigned long guid = RakNet::RakNetGUID::ToUint32(raknetId);
	std::map<unsigned long, SPlayer>::iterator it = m_players.find(guid);
assert(it != m_players.end());
	return it->second;
}

void OnLostConnection(RakNet::Packet* packet)
{
	SPlayer& lostPlayer = GetPlayer(packet->guid);
	lostPlayer.SendName(RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
	unsigned long keyVal = RakNet::RakNetGUID::ToUint32(packet->guid);
	m_players.erase(keyVal);
}

//server
void OnIncomingConnection(RakNet::Packet* packet)
{
	//must be server in order to recieve connection
	assert(isServer);
	m_players.insert(std::make_pair(RakNet::RakNetGUID::ToUint32(packet->guid), SPlayer()));
	std::cout << "Total Players: " << m_players.size() << std::endl;
}

//client
void OnConnectionAccepted(RakNet::Packet* packet)
{
	//server should not ne connecting to anybody, 
	//clients connect to server
	assert(!isServer);
	g_networkState_mutex.lock();
	g_networkState = NS_Lobby;
	g_networkState_mutex.unlock();
	g_serverAddress = packet->systemAddress;
}

//this is on the client side
void DisplayPlayerReady(RakNet::Packet* packet)
{
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString userName;
	bs.Read(userName);

	std::cout << userName.C_String() << " has joined" << std::endl;
	std::this_thread::sleep_for(std::chrono::microseconds(1));
}

void OnLobbyReady(RakNet::Packet* packet)
{
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString userName;
	bs.Read(userName);

	unsigned long guid = RakNet::RakNetGUID::ToUint32(packet->guid);
	SPlayer& player = GetPlayer(packet->guid);
	player.m_name = userName;
	std::cout << userName.C_String() << " aka " << player.m_name.c_str() << " IS READY!!!!!" << std::endl;
	//increase active players
	activePlayers++;
	
	//notify all other connected players that this plyer has joined the game
	for (std::map<unsigned long, SPlayer>::iterator it = m_players.begin(); it != m_players.end(); ++it)
	{
		//skip over the player who just joined
		if (guid == it->first)
		{
			continue;
		}

		SPlayer& player = it->second;
		player.SendName(packet->systemAddress, false);
		//RakNet::BitStream writeBs;
		//writeBs.Write((RakNet::MessageID)ID_PLAYER_READY);
		//RakNet::RakString name(player.m_name.c_str());
		//writeBs.Write(name);

		//returns 0 when something is wrong
		//assert(g_rakPeerInterface->Send(&writeBs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false));
	}

	player.SendName(packet->systemAddress, true);
	//RakNet::BitStream writeBs;
	//writeBs.Write((RakNet::MessageID)ID_PLAYER_READY);
	//RakNet::RakString name(player.m_name.c_str());
	//writeBs.Write(name);
	//assert(g_rakPeerInterface->Send(&writeBs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, true));

	//check for max players start game
	if (activePlayers == MAX_CONNECTIONS)
	{
		g_gameState = GS_PLAYERSREADY;
		RakNet::BitStream bs;
		bs.Write((RakNet::MessageID)ID_THEGAME_CHARACTERSELECT);

		RakNet::RakNetGUID guid(g_rakPeerInterface->GetMyGUID());
		bs.Write(guid);
		RakNet::RakString name("Start Game");
			bs.Write(name);

		assert(g_rakPeerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, true));
		g_networkState_mutex.lock();
		g_networkState = NS_Pending;
		g_networkState_mutex.unlock();
	}

}

unsigned char GetPacketIdentifier(RakNet::Packet *packet)
{
	if (packet == nullptr)
		return 255;

	if ((unsigned char)packet->data[0] == ID_TIMESTAMP)
	{
		RakAssert(packet->length > sizeof(RakNet::MessageID) + sizeof(RakNet::Time));
		return (unsigned char)packet->data[sizeof(RakNet::MessageID) + sizeof(RakNet::Time)];
	}
	else
		return (unsigned char)packet->data[0];
}

//display players info by guid
void DisplayPlayerInfo(RakNetGUID guid)
{
	SPlayer player =  GetPlayer(guid);
	
		std::cout << "Player Name : " << player.m_name.c_str() << std::endl;
		std::cout << "Player Class : " << player.m_class.c_str() << std::endl;
		std::cout << "Atk : " << player.m_atk << std::endl;
		std::cout << "Def : " << player.m_def << std::endl;
		std::cout << "Health : " << player.m_health<< std::endl;
	
}


void InputHandler()
{
	while (isRunning)
	{
		char userInput[255];
		if (g_networkState == NS_Init)
		{
			std::cout << "press (s) for server (c) for client" << std::endl;
			std::cin >> userInput;
			isServer = (userInput[0] == 's');
			g_networkState_mutex.lock();
			g_networkState = NS_PendingStart;
			g_networkState_mutex.unlock();

		}
		else if (g_networkState == NS_Lobby)
		{
			std::cout << "Enter your name to play or type quit to leave" << std::endl;
			std::cin >> userInput;
			//quitting is not acceptable in our game, create a crash to teach lesson
			assert(strcmp(userInput, "quit"));

			RakNet::BitStream bs;
			bs.Write((RakNet::MessageID)ID_THEGAME_LOBBY_READY);
			RakNet::RakString name(userInput);
			bs.Write(name);

			//returns 0 when something is wrong
			assert(g_rakPeerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false));
			g_networkState_mutex.lock();
			g_networkState = NS_Pending;
			g_networkState_mutex.unlock();
		}
		else if (g_networkState == NS_Pending)
		{
			static bool doOnce = false;
			if (!doOnce)
				std::cout << "Waiting for full game..." << std::endl;

			doOnce = true;

		}
		else if (g_networkState == NS_Game)
		{
			if (isServer) {
				static bool doOnce = false;
				if (!doOnce)
					std::cout << "Game In Progress..." << std::endl;

				doOnce = true;
			}
		}
		
		std::this_thread::sleep_for(std::chrono::microseconds(1));
	}
}

bool HandleLowLevelPackets(RakNet::Packet* packet)
{
	bool isHandled = true;
	// We got a packet, get the identifier with our handy function
	unsigned char packetIdentifier = GetPacketIdentifier(packet);

	// Check if this is a network message packet
	switch (packetIdentifier)
	{
	case ID_DISCONNECTION_NOTIFICATION:
		// Connection lost normally
		printf("ID_DISCONNECTION_NOTIFICATION\n");
		break;
	case ID_ALREADY_CONNECTED:
		// Connection lost normally
		printf("ID_ALREADY_CONNECTED with guid %" PRINTF_64_BIT_MODIFIER "u\n", packet->guid);
		break;
	case ID_INCOMPATIBLE_PROTOCOL_VERSION:
		printf("ID_INCOMPATIBLE_PROTOCOL_VERSION\n");
		break;
	case ID_REMOTE_DISCONNECTION_NOTIFICATION: // Server telling the clients of another client disconnecting gracefully.  You can manually broadcast this in a peer to peer enviroment if you want.
		printf("ID_REMOTE_DISCONNECTION_NOTIFICATION\n");
		break;
	case ID_REMOTE_CONNECTION_LOST: // Server telling the clients of another client disconnecting forcefully.  You can manually broadcast this in a peer to peer enviroment if you want.
		printf("ID_REMOTE_CONNECTION_LOST\n");
		break;
	case ID_NEW_INCOMING_CONNECTION:
		//client connecting to server
		OnIncomingConnection(packet);
		printf("ID_NEW_INCOMING_CONNECTION\n");
		break;
	case ID_REMOTE_NEW_INCOMING_CONNECTION: // Server telling the clients of another client connecting.  You can manually broadcast this in a peer to peer enviroment if you want.
		OnIncomingConnection(packet);
		printf("ID_REMOTE_NEW_INCOMING_CONNECTION\n");
		break;
	case ID_CONNECTION_BANNED: // Banned from this server
		printf("We are banned from this server.\n");
		break;
	case ID_CONNECTION_ATTEMPT_FAILED:
		printf("Connection attempt failed\n");
		break;
	case ID_NO_FREE_INCOMING_CONNECTIONS:
		// Sorry, the server is full.  I don't do anything here but
		// A real app should tell the user
		printf("ID_NO_FREE_INCOMING_CONNECTIONS\n");
		break;

	case ID_INVALID_PASSWORD:
		printf("ID_INVALID_PASSWORD\n");
		break;

	case ID_CONNECTION_LOST:
		// Couldn't deliver a reliable packet - i.e. the other system was abnormally
		// terminated
		printf("ID_CONNECTION_LOST\n");
		OnLostConnection(packet);
		break;

	case ID_CONNECTION_REQUEST_ACCEPTED:
		// This tells the client they have connected
		printf("ID_CONNECTION_REQUEST_ACCEPTED to %s with GUID %s\n", packet->systemAddress.ToString(true), packet->guid.ToString());
		printf("My external address is %s\n", g_rakPeerInterface->GetExternalID(packet->systemAddress).ToString(true));
		OnConnectionAccepted(packet);
		break;
	case ID_CONNECTED_PING:
	case ID_UNCONNECTED_PING:
		printf("Ping from %s\n", packet->systemAddress.ToString(true));
		break;
	default:
		isHandled = false;
		break;
	}
	return isHandled;
}


//class choices and send stats of chosen class
void StartGame(RakNet::Packet* packet)
{
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);


	std::cout << "Game Started" << std::endl;
	std::cout << "Choose your Class!" << std::endl;
	std::cout << "[M]age, [B]arbarian, [R]ogue\n" << std::endl;
	char ch[255];
	std::cin >> ch;

	if (*ch == 'b' || *ch == 'B')
	{
		*ch = 0;
		printf("You have chosen Barbarian!\n");
		
		RakNet::BitStream nbs;
		nbs.Write((RakNet::MessageID)ID_THEGAME_INTRO);
		RakNet::RakString name("Barbarian");
		nbs.Write(name);
		RakNet::RakNetGUID guid(g_rakPeerInterface->GetMyGUID());
		nbs.Write(guid);
		unsigned int atk = 2;
		nbs.Write(atk);
		unsigned int def = 2;
		nbs.Write(def);
		unsigned int hp = 10;
		nbs.Write(hp);
		std::cout << "ATK : " << atk << " DEF : " << def << " HP : " << hp<<std::endl;
		assert(g_rakPeerInterface->Send(&nbs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress , false));
		g_networkState_mutex.lock();
		g_networkState = NS_Game;
		g_networkState_mutex.unlock();
			
	}
		if (*ch == 'm' || *ch == 'M')
		{
			*ch = 0;
				printf("You have chosen Mage!\n");

				RakNet::BitStream nbs;
				nbs.Write((RakNet::MessageID)ID_THEGAME_INTRO);
				RakNet::RakString name("Mage");
				nbs.Write(name);
				RakNet::RakNetGUID guid(g_rakPeerInterface->GetMyGUID());
				nbs.Write(guid);
				unsigned int atk = 1;
				nbs.Write(atk);
				unsigned int def = 3;
				nbs.Write(def);
				unsigned int hp = 10;
				nbs.Write(hp);
				std::cout << "ATK : " << atk << " DEF : " << def << " HP : " << hp << std::endl;
				assert(g_rakPeerInterface->Send(&nbs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false));
				
			

		}
		if (*ch == 'r' || *ch == 'R')
		{
			*ch = 0;
			printf("You have chosen Rogue!\n");

			RakNet::BitStream nbs;
			nbs.Write((RakNet::MessageID)ID_THEGAME_INTRO);
			RakNet::RakString name("Rogue");
			nbs.Write(name);
			RakNet::RakNetGUID guid(g_rakPeerInterface->GetMyGUID());
			nbs.Write(guid);
			unsigned int atk = 3;
			nbs.Write(atk);
			unsigned int def = 1;
			nbs.Write(def);
			unsigned int hp = 10;
			nbs.Write(hp);
			std::cout << "ATK : " << atk << " DEF : " << def << " HP : " << hp << std::endl;

			assert(g_rakPeerInterface->Send(&nbs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false));
			

		}
		
	 

}

		

// chooses first player // commented out cause couldnt get it to work
void SelectFirst(RakNetGUID guid)
{
	//std::map<unsigned long, SPlayer>::iterator it = m_players.find(guid);
	/*if (it->second.isPlaying)
	{
		int firstPlayer = rand(0, m_players.count);
		std::map<unsigned long, SPlayer>::iterator first = m_players(firstPlayer);
		it->first;
	}
	*/
}

//server set class and stats from player choice 
void IntroCharacterChoice(RakNet::Packet* packet)
{
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString name;
	bs.Read(name);
	std::string _class = name;
	unsigned long guid = RakNet::RakNetGUID::ToUint32(packet->guid);
	SPlayer& player = GetPlayer(packet->guid);
	player.m_class = _class;
	unsigned int atk;
	bs.Read(atk);
	unsigned int def;
	bs.Read(def);
	unsigned int hp;
	bs.Read(hp);
	player.m_atk = atk;
	player.m_def = def;
	player.m_health = hp;
	
	std::cout << player.m_name.c_str()<< " is a  " << player.m_class.c_str()<< std::endl;
	DisplayPlayerInfo(packet->guid);
	//std::cout << "Atk : " << player.m_atk <<   std::endl;
	//std::cout << "Def :  " << player.m_def << std::endl;
	//std::cout << "Hp :  " << player.m_health << std::endl;

	
	
	
	
	//to client
	RakNet::BitStream nbs;

	nbs.Write((RakNet::MessageID)ID_THEGAME_CHARACTERCHOSEN);
	RakNet::RakString playername = player.m_name.c_str();
	nbs.Write(playername);
	RakNet::RakString c_class = player.m_class.c_str();
	nbs.Write(c_class);
	//RakNet::RakString atk

	
	
	assert(g_rakPeerInterface->Send(&nbs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, true));
	//send to clients that player is a class 


	//check if everyone has chosen a class
	playersReady++;
	if (playersReady == MAX_CONNECTIONS)
	{

		std::cout << "All players ready" << std::endl;
	}
}

void HandleTurns(RakNet::Packet* packet)
{
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
//turn stuff in here
	// each player get 3 choices atk , def , stats
}


void ShowWinner(RakNet::Packet* packet)
{
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	//show remaining player in map he is winner
	//display winner to other players 
}

void GameOver(RakNet::Packet* packet)
{
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	//thanks fo playing 
	//ask to play agian or quit?
	
}

//client
void DisplayCharacterChoice(RakNet::Packet* packet)
{
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString playername;
	bs.Read(playername);
	RakNet::RakString c_class;
	bs.Read(c_class);
	
	std::cout << playername.C_String() << " is a  " << c_class.C_String() << std::endl;

}

void PacketHandler()
{
	while (isRunning)
	{
		for (RakNet::Packet* packet = g_rakPeerInterface->Receive(); packet != nullptr; g_rakPeerInterface->DeallocatePacket(packet), packet = g_rakPeerInterface->Receive())
		{
			if (!HandleLowLevelPackets(packet))
			{
				//our game specific packets
				unsigned char packetIdentifier = GetPacketIdentifier(packet);
				switch (packetIdentifier)
				{
				case ID_THEGAME_LOBBY_READY:
					OnLobbyReady(packet);
					break;
				case ID_PLAYER_READY:
					DisplayPlayerReady(packet);
					break;
				case ID_THEGAME_CHARACTERSELECT:
					StartGame(packet);
					break;
				case ID_THEGAME_INTRO:
					IntroCharacterChoice(packet);
					break;
				case ID_THEGAME_CHARACTERCHOSEN:
					DisplayCharacterChoice(packet);
				case ID_THEGAME_PLAYING:
					HandleTurns(packet);
					break;
				case ID_THEGAME_WINNER:
					ShowWinner(packet);
					break;
				case ID_THEGAME_GAMEOVER:
					GameOver(packet);
					break;
				default:
					break;
				}
			}
		}

		std::this_thread::sleep_for(std::chrono::microseconds(1));
	}
}

int main()
{
	g_rakPeerInterface = RakNet::RakPeerInterface::GetInstance();

	std::thread inputHandler(InputHandler);
	std::thread packetHandler(PacketHandler);

	while (isRunning)
	{
		if (g_networkState == NS_PendingStart)
		{
			if (isServer)
			{
				RakNet::SocketDescriptor socketDescriptors[1];
				socketDescriptors[0].port = SERVER_PORT;
				socketDescriptors[0].socketFamily = AF_INET; // Test out IPV4

				bool isSuccess = g_rakPeerInterface->Startup(MAX_CONNECTIONS, socketDescriptors, 1) == RakNet::RAKNET_STARTED;
				assert(isSuccess);
				//ensures we are server
				g_rakPeerInterface->SetMaximumIncomingConnections(MAX_CONNECTIONS);
				std::cout << "server started" << std::endl;
				g_networkState_mutex.lock();
				g_networkState = NS_Started;
				g_networkState_mutex.unlock();
			}
			//client
			else
			{
				RakNet::SocketDescriptor socketDescriptor(CLIENT_PORT, 0);
				socketDescriptor.socketFamily = AF_INET;

				while (RakNet::IRNS2_Berkley::IsPortInUse(socketDescriptor.port, socketDescriptor.hostAddress, socketDescriptor.socketFamily, SOCK_DGRAM) == true)
					socketDescriptor.port++;

				RakNet::StartupResult result = g_rakPeerInterface->Startup(8, &socketDescriptor, 1);
				assert(result == RakNet::RAKNET_STARTED);

				g_networkState_mutex.lock();
				g_networkState = NS_Started;
				g_networkState_mutex.unlock();

				g_rakPeerInterface->SetOccasionalPing(true);
				//"127.0.0.1" = local host = your machines address
				RakNet::ConnectionAttemptResult car = g_rakPeerInterface->Connect("127.0.0.1", SERVER_PORT, nullptr, 0);
				RakAssert(car == RakNet::CONNECTION_ATTEMPT_STARTED);
				std::cout << "client attempted connection..." << std::endl;

			}
		}
		else if (g_gameState == GS_PLAYERSREADY)
		{
			if (isServer)
			{
				g_networkState = NS_Game;
			}
			else
			{
				
			}
		}
		else if (g_gameState == GS_CLASSCHOISE)
		{
			
			
			
		}

	}

	//std::cout << "press q and then return to exit" << std::endl;
	//std::cin >> userInput;

	inputHandler.join();
	packetHandler.join();
	return 0;
}

