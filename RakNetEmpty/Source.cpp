/*#include "MessageIdentifiers.h"
#include "RakPeerInterface.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <functional>
#include <iostream>
#include <stack>
#include <string>
#include <thread>
#include <vector>

#define SERVER_PORT 65000
#define SERVER_MAX_CONNECTIONS 8

#define CLIENT_PORT_START SERVER_PORT + 1
#define CLIENT_PORT_END CLIENT_PORT_START + SERVER_MAX_CONNECTIONS

bool isRunning = false;
bool isServer = false;

std::thread inputThread;
std::thread networkThread;

RakNet::RakPeerInterface *rakPeerInterface = nullptr;
//RakNet::SystemAddress clientID;

std::string clientName;
char message[2048];

enum EPlayerClass
{
Mage = 0,
Rogue,
Barbarian
};

// Starts the client
void StartClient()
{
if (rakPeerInterface != nullptr) return;
isServer = false;

rakPeerInterface = RakNet::RakPeerInterface::GetInstance();

RakNet::SocketDescriptor socketDescriptor(0, 0);

socketDescriptor.socketFamily = AF_INET;

rakPeerInterface->Startup(SERVER_MAX_CONNECTIONS, &socketDescriptor, 1);
rakPeerInterface->SetOccasionalPing(true);

RakNet::ConnectionAttemptResult connectionAttemptResult = rakPeerInterface->Connect(rakPeerInterface->GetLocalIP(0), SERVER_PORT, "", 0);

if (connectionAttemptResult == RakNet::CONNECTION_ATTEMPT_STARTED)
{
isRunning = true;
}

}

// Starts the server
void StartServer()
{
if (rakPeerInterface != nullptr) return;
isServer = true;

rakPeerInterface = RakNet::RakPeerInterface::GetInstance();

const unsigned int nSockets = 1;
RakNet::SocketDescriptor socketDescriptors[nSockets];

socketDescriptors[0].port = SERVER_PORT;
socketDescriptors[0].socketFamily = AF_INET;

RakNet::StartupResult startupResult = rakPeerInterface->Startup(SERVER_MAX_CONNECTIONS, socketDescriptors, nSockets);

if (startupResult == RakNet::RAKNET_STARTED)
{
rakPeerInterface->SetMaximumIncomingConnections(SERVER_MAX_CONNECTIONS);
rakPeerInterface->SetOccasionalPing(true);
rakPeerInterface->SetUnreliableTimeout(1000);

isRunning = true;
}
}

// Gets the packet identifier
unsigned char GetPacketIdentifier(RakNet::Packet *p)
{
if (p == 0)
{
return 255;
}

if ((unsigned char)p->data[0] == ID_TIMESTAMP)
{
RakAssert(p->length > sizeof(RakNet::MessageID) + sizeof(RakNet::Time));
return (unsigned char)p->data[sizeof(RakNet::MessageID) + sizeof(RakNet::Time)];
}
else
{
return (unsigned char)p->data[0];
}
}

void HandleServerPackets(RakNet::Packet *p)
{
// We got a packet, get the identifier with our handy function
unsigned char packetIdentifier = GetPacketIdentifier(p);

// Check if this is a network message packet
switch (packetIdentifier)
{
case ID_DISCONNECTION_NOTIFICATION:
// Connection lost normally
printf("ID_DISCONNECTION_NOTIFICATION from %s\n", p->systemAddress.ToString(true));;
break;


case ID_NEW_INCOMING_CONNECTION:
// Somebody connected.  We have their IP now
printf("ID_NEW_INCOMING_CONNECTION from %s with GUID %s\n", p->systemAddress.ToString(true), p->guid.ToString());

printf("Remote internal IDs:\n");
for (int index = 0; index < MAXIMUM_NUMBER_OF_INTERNAL_IDS; index++)
{
RakNet::SystemAddress internalId = rakPeerInterface->GetInternalID(p->systemAddress, index);
if (internalId != RakNet::UNASSIGNED_SYSTEM_ADDRESS)
{
printf("%i. %s\n", index + 1, internalId.ToString(true));
}
}

break;

case ID_INCOMPATIBLE_PROTOCOL_VERSION:
printf("ID_INCOMPATIBLE_PROTOCOL_VERSION\n");
break;

case ID_CONNECTED_PING:
case ID_UNCONNECTED_PING:
printf("Ping from %s\n", p->systemAddress.ToString(true));
break;

case ID_CONNECTION_LOST:
// Couldn't deliver a reliable packet - i.e. the other system was abnormally
// terminated
printf("ID_CONNECTION_LOST from %s\n", p->systemAddress.ToString(true));;
break;

default:

printf("MESSAGE_RECIEVED\n");

// The server knows the static data of all clients, so we can prefix the message
// With the name data
printf("%s\n", p->data);

// Relay the message.  We prefix the name for other clients.  This demonstrates
// That messages can be changed on the server before being broadcast
// Sending is the same as before
sprintf_s(message, "%s", p->data);
rakPeerInterface->Send(message, (const int)strlen(message) + 1, HIGH_PRIORITY, RELIABLE_ORDERED, 0, p->systemAddress, true);

break;
}
}

void HandleClientPackets(RakNet::Packet *p)
{
// We got a packet, get the identifier with our handy function
unsigned char packetIdentifier = GetPacketIdentifier(p);

// Check if this is a network message packet
switch (packetIdentifier)
{
case ID_DISCONNECTION_NOTIFICATION:
// Connection lost normally
printf("ID_DISCONNECTION_NOTIFICATION\n");
break;
case ID_ALREADY_CONNECTED:
// Connection lost normally
printf("ID_ALREADY_CONNECTED with guid %" PRINTF_64_BIT_MODIFIER "u\n", p->guid);
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
case ID_REMOTE_NEW_INCOMING_CONNECTION: // Server telling the clients of another client connecting.  You can manually broadcast this in a peer to peer enviroment if you want.
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
break;

case ID_CONNECTION_REQUEST_ACCEPTED:
// This tells the client they have connected
printf("ID_CONNECTION_REQUEST_ACCEPTED to %s with GUID %s\n", p->systemAddress.ToString(true), p->guid.ToString());
printf("My external address is %s\n", rakPeerInterface->GetExternalID(p->systemAddress).ToString(true));
break;
case ID_CONNECTED_PING:
case ID_UNCONNECTED_PING:
printf("Ping from %s\n", p->systemAddress.ToString(true));
break;
default:
// It's a client, so just show the message
printf("%s\n", p->data);
break;
}
}

// Handles server input
void HandleServerInput(const std::string &line)
{
if (line == "quit")
{
std::cout << "Quitting application..." << std::endl;
isRunning = false;
return;
}
else
{
std::string message = "Server: " + line;
const char *data = message.c_str();
rakPeerInterface->Send(data, (const int)strlen(data) + 1, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
}
}

// Handles client input
void HandleClientInput(const std::string &line)
{
if (line == "quit")
{
std::cout << "Quitting application..." << std::endl;
isRunning = false;
return;
}

std::string message = clientName + ": " + line;
const char *data = message.c_str();
rakPeerInterface->Send(data, (const int)strlen(data) + 1, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);

}

// Handles input
void HandleInput()
{
printf("Would you like to be a client (c) or server (s)?\n");
char c = getchar();
if (c == 'c')
{
printf("Please enter your username.\n");
std::cin >> clientName;

StartClient();
}
else
{
StartServer();
}

while (isRunning && rakPeerInterface != nullptr)
{
std::this_thread::sleep_for(std::chrono::milliseconds(30));

std::string line;

std::getline(std::cin, line);
if (!line.empty())
{

if (isServer)
{
HandleServerInput(line);
}
else
{
HandleClientInput(line);
}
}

line.clear();
printf("> ");
}
}

// Handles the network
void HandleNetwork()
{
// prevent early networking attempts
while (!isRunning || rakPeerInterface == nullptr)
{
}

// start receiving packets once this node is connectable
while (isRunning && rakPeerInterface != nullptr)
{
for (RakNet::Packet *p = rakPeerInterface->Receive(); p; rakPeerInterface->DeallocatePacket(p), p = rakPeerInterface->Receive())
{
if (isServer) // is server
{
HandleServerPackets(p);
}
else // is client
{
HandleClientPackets(p);
}
}
}

rakPeerInterface->Shutdown(300);
RakNet::RakPeerInterface::DestroyInstance(rakPeerInterface);
}

void PrintStats()
{

}

// Main
int main(int argc, char **argv)
{
inputThread = std::thread(HandleInput);
networkThread = std::thread(HandleNetwork);

inputThread.join();
networkThread.join();

printf("Press enter to quit... ");
getchar();

return 0;
}*/


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
static unsigned int MAX_CONNECTIONS = 2;

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

RakNet::RakPeerInterface *g_rakPeerInterface = nullptr;
RakNet::SystemAddress g_serverAddress;

std::mutex g_networkState_mutex;
NetworkState g_networkState = NS_Init;
GameState g_gameState = GS_Init;
using namespace RakNet;
void SelectCharacter(RakNetGUID guid, int character);


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
	//get class from enum
/*	char* GetPlayerType()
	{
		switch (m_class)
		{
		case Mage:
			return "Mage";
		case Warrior:
			return "Warrior";
		case Rogue:
			return "Rogue";
		default:
			return "None";
		}
	}*/
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

//display players info
void DisplayPlayerInfo()
{
	for (int i=0; i < 3; i++)
	{
		//std::cout << "GUID : " << m_players._Getpfirst._Get_data->m_name.ToString() << std::endl;
		//std::cout << "Player Type" << m_players._Getpfirst._Get_data->m_playerType << std::endl;
		//std::cout << "Atk : " << m_players._Getpfirst._Get_data->m_atk << std::endl;
		//std::cout << "Health : " << m_players._Getpfirst._Get_data->m_health << std::endl;
	}
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

void StartGame(RakNet::Packet* packet)
{
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
//	RakNet::RakNetGUID guid(g_rakPeerInterface->GetMyGUID());
//	bs.Read(guid);

	//unsigned long guid = RakNet::RakNetGUID::ToUint32(packet->guid);
	//SPlayer& player = GetPlayer(packet->guid);
	


	std::cout << "Game Started" << std::endl;
	std::cout << "Choose your Class!" << std::endl;
	std::cout << "[M]age, [W]arrior, [R]ogue\n" << std::endl;
	//void DisplayPlayerInfo();
	char ch[255];
	std::cin >> ch;

	if (*ch == 'w' || *ch == 'W')
	{
		*ch = 0;
		printf("You have chosen Warrior!\n");
		//std::cout << player.m_name.c_str() << " is Warrior" << std::endl;
		//to server
		RakNet::BitStream nbs;
		nbs.Write((RakNet::MessageID)ID_THEGAME_INTRO);
		RakNet::RakString name("Warrior");
		nbs.Write(name);
		RakNet::RakNetGUID guid(g_rakPeerInterface->GetMyGUID());
		nbs.Write(guid);
		unsigned int atk = 2;
		unsigned int def = 2;
		unsigned int hp = 10;

		assert(g_rakPeerInterface->Send(&nbs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress , false));
		g_networkState_mutex.lock();
		g_networkState = NS_Game;
		g_networkState_mutex.unlock();
			//SelectCharacter(g_rakPeerInterface->GetMyGUID(), 2);
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


				assert(g_rakPeerInterface->Send(&nbs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false));
				g_networkState_mutex.lock();
				g_networkState = NS_Game;
				g_networkState_mutex.unlock();
			//SelectCharacter(g_rakPeerInterface->GetMyGUID(), 0);

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


			assert(g_rakPeerInterface->Send(&nbs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false));
			g_networkState_mutex.lock();
			g_networkState = NS_Game;
			g_networkState_mutex.unlock();

		}
		
		//void DisplayPlayerInfo();

}

		

//character select for each client
void SelectCharacter(RakNetGUID guid, int character)
{
	//get player
	std::map<unsigned long, SPlayer>::iterator it = m_players.begin();
	//select character
	if (character_t.isPlaying)
	{
	//	g_rakPeerInterface->Send(chosenCharacter, (int)strlen(chosenCharacter) + 1, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, false);
	}
	else {
		//it->second.GetPlayerType = character;
		//g_rakPeerInterface->Send(chosenCharacter, (int)strlen(chosenCharacter) + 1, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
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

//server
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
	std::cout << "Atk : " << player.m_atk << std::endl;
	std::cout << "Def :  " << player.m_def << std::endl;
	std::cout << "Hp :  " << player.m_health << std::endl;

	
	
	
	
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
}

void HandleTurns(RakNet::Packet* packet)
{
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
//turn stuff in here
}


void ShowWinner(RakNet::Packet* packet)
{
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	// display player with health left 
}

void GameOver(RakNet::Packet* packet)
{
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	//thank you , wanna play again? quit?
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
	//std::cout <<"here"<< std::endl;
	//thank you , wanna play again? quit?
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

