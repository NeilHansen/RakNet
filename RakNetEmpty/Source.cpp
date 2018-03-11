#include "MessageIdentifiers.h"
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


//server port 
#define SERVER_PORT 65000

//max connections allowed to server
#define SERVER_MAX_CONNECTIONS 4

//client port
#define CLIENT_PORT_START 65001
#define CLIENT_PORT_END CLIENT_PORT_START + SERVER_MAX_CONNECTIONS

bool isRunning = false;
bool isServer = false;

//threads
std::thread inputThread;
std::thread networkThread;

RakNet::RakPeerInterface *rakPeerInterface = nullptr;
RakNet::SystemAddress clientID;

std::string clientName;
char message[2048];

//form lab for assignment
enum EPlayerClass
{
	Mage = 0,
	Rogue,
	Barbarian
};

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
		//server can talk here
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

	//client talks here
	std::string message = clientName + ": " + line;
	const char *data = message.c_str();
	rakPeerInterface->Send(data, (const int)strlen(data) + 1, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);

}

// Handles input
void InputHandler()
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
		std::this_thread::sleep_for(std::chrono::milliseconds(1));

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

// Main
int main(int argc, char **argv)
{
	inputThread = std::thread(InputHandler);
	networkThread = std::thread(HandleNetwork);

	inputThread.join();
	networkThread.join();

	printf("Press enter to quit... ");
	getchar();

	return 0;
}