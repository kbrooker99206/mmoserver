/*
---------------------------------------------------------------------------------------
This source file is part of swgANH (Star Wars Galaxies - A New Hope - Server Emulator)
For more information, see http://www.swganh.org


Copyright (c) 2006 - 2008 The swgANH Team

---------------------------------------------------------------------------------------
*/

#include "ConnectionServer.h"
#include "ClientManager.h"
#include "ServerManager.h"
#include "MessageRouter.h"
#include "ConnectionDispatch.h"
#include "NetworkManager/NetworkManager.h"
#include "NetworkManager/Service.h"
#include "DatabaseManager/DatabaseManager.h"
#include "DatabaseManager/Database.h"
#include "LogManager/LogManager.h"
#include "ConfigManager/ConfigManager.h"

#include "Common/MessageFactory.h"

//#include "ZoneServer/Stackwalker.h" // removing so that we can get back proper debugging
#include <windows.h>

#include <stdio.h>
#include <conio.h>
#include <string.h>

//======================================================================================================================

ConnectionServer* gConnectionServer = 0;

//======================================================================================================================

ConnectionServer::ConnectionServer(void) :
mDatabaseManager(0),
mDatabase(0),
mNetworkManager(0),
mMessageRouter(0),
mClientManager(0),
mServerManager(0),
mConnectionDispatch(0),
mClusterId(0),
mClientService(0),
mServerService(0)
{

}

//======================================================================================================================

ConnectionServer::~ConnectionServer(void)
{

}

//======================================================================================================================

void ConnectionServer::Startup(void)
{
	// log msg to default log
	//gLogger->printSmallLogo();
	gLogger->logMsg("ConnectionServer Startup", FOREGROUND_GREEN | FOREGROUND_RED);
	// gLogger->logMsg(GetBuildString());
	gLogger->logMsg(ConfigManager::getBuildString());
	
	// Startup our core modules
	mNetworkManager = new NetworkManager();
	mNetworkManager->Startup();

	// Create our status service
	//clientservice
	mClientService = mNetworkManager->CreateService((char*)gConfig->read<std::string>("BindAddress").c_str(), gConfig->read<uint16>("BindPort"),gConfig->read<uint32>("ClientServiceMessageHeap")*1024, false);//,5);
	//serverservice
	mServerService = mNetworkManager->CreateService((char*)gConfig->read<std::string>("ClusterBindAddress").c_str(), gConfig->read<uint16>("ClusterBindPort"),gConfig->read<uint32>("ServerServiceMessageHeap")*1024, true);//,15);

	mDatabaseManager = new DatabaseManager();
	mDatabaseManager->Startup();

	mDatabase = mDatabaseManager->Connect(DBTYPE_MYSQL,
									   (char*)(gConfig->read<std::string>("DBServer")).c_str(),
									   gConfig->read<int>("DBPort"),
									   (char*)(gConfig->read<std::string>("DBUser")).c_str(),
									   (char*)(gConfig->read<std::string>("DBPass")).c_str(),
									   (char*)(gConfig->read<std::string>("DBName")).c_str());

	mClusterId = gConfig->read<uint32>("ClusterId");
	
	mDatabase->ExecuteSqlAsync(0, 0, "UPDATE galaxy SET status=1, last_update=NOW() WHERE galaxy_id=%u;", mClusterId);

	gLogger->connecttoDB(mDatabaseManager);
	gLogger->createErrorLog("connection.log",(LogLevel)(gConfig->read<int>("LogLevel",2)),
										(bool)(gConfig->read<bool>("LogToFile", true)),
										(bool)(gConfig->read<bool>("ConsoleOut",true)),
										(bool)(gConfig->read<bool>("LogAppend",true)));

	mDatabase->ExecuteSqlAsync(0,0,"UPDATE config_process_list SET serverstartID = serverstartID+1 WHERE name like 'connection'");
	// In case of a crash, we need to cleanup the DB a little.
	DatabaseResult* result = mDatabase->ExecuteSynchSql("UPDATE account SET loggedin=0 WHERE loggedin=%u;", mClusterId);
	mDatabase->DestroyResult(result);

	// Status:  0=offline, 1=loading, 2=online
	_updateDBServerList(1);

	// Instant the messageFactory. It will also run the Startup ().
	(void)MessageFactory::getSingleton();		// Use this a marker of where the factory is instanced. 
												// The code itself here is not needed, since it will instance itself at first use.

	// Startup our router modules.
	mConnectionDispatch = new ConnectionDispatch();
	mConnectionDispatch->Startup();

	mMessageRouter = new MessageRouter();
	mMessageRouter->Startup(mDatabase, mConnectionDispatch);

	mClientManager = new ClientManager();
	mClientManager->Startup(mClientService, mDatabase, mMessageRouter, mConnectionDispatch);

	mServerManager = new ServerManager();
	mServerManager->Startup(mServerService, mDatabase, mMessageRouter, mConnectionDispatch,mClientManager);

	// We're done initiailizing.
	_updateDBServerList(2);
	gLogger->logMsg("ConnectionServer::Server Boot Complete", FOREGROUND_GREEN);
	//gLogger->printLogo();
	// std::string BuildString(GetBuildString());	
	std::string BuildString(ConfigManager::getBuildString());	
	gLogger->logMsgF("ConnectionServer %s",MSG_NORMAL,BuildString.substr(11,BuildString.size()).c_str());
	gLogger->logMsg("Welcome to your SWGANH Experience!");
	
	
}

//======================================================================================================================

void ConnectionServer::Shutdown(void)
{
	gLogger->logMsg("ConnectionServer Shutting down...");

	// Update our status for the LoginServer
	mDatabase->DestroyResult(mDatabase->ExecuteSynchSql("UPDATE galaxy SET status=0 WHERE galaxy_id=%u;",mClusterId));

	// We're shuttind down, so update the DB again.
	_updateDBServerList(0);

	mClientManager->Shutdown();
	mServerManager->Shutdown();
	mMessageRouter->Shutdown();

	mConnectionDispatch->Shutdown();

	// Destroy our network services.
	mNetworkManager->DestroyService(mServerService);
	mNetworkManager->DestroyService(mClientService);

	// Shutdown our core modules
	mDatabaseManager->Shutdown();
	mNetworkManager->Shutdown();

	// Delete our objects
	delete mClientManager;
	delete mServerManager;
	delete mMessageRouter;
	delete mConnectionDispatch;

	MessageFactory::getSingleton()->destroySingleton();	// Delete message factory and call shutdown();

	delete mDatabaseManager;
	delete mNetworkManager;

	gLogger->logMsg("ConnectionServer Shutdown Complete");
}

//======================================================================================================================

void ConnectionServer::Process(void)
{
	// Process our core services first.
	//mNetworkManager->Process();
	mDatabaseManager->Process();
	
	//we dont want this stalled by the clients!!!
	mServerService->Process(10);
	mClientService->Process(30);

	// Now process our sub modules
	gMessageFactory->Process();
	mClientManager->Process();
	mServerManager->Process();
	mMessageRouter->Process();
}

//======================================================================================================================

void ConnectionServer::_updateDBServerList(uint32 status)
{
	// Execute our query
	mDatabase->DestroyResult(mDatabase->ExecuteSynchSql("UPDATE config_process_list SET address='%s', port=%u, status=%u WHERE name='connection';",mServerService->getLocalAddress(), mServerService->getLocalPort(), status));
}

//======================================================================================================================

int main(int argc, char* argv)
{
	//OnlyInstallUnhandeldExceptionFilter();

	// init our logmanager singleton,set global level normal, create the default log with normal priority, output to file + console, also truncate
	LogManager::Init(G_LEVEL_NORMAL, "ConnectionServer.log", LEVEL_NORMAL, true, true);

	// init out configmanager singleton (access configvariables with gConfig Macro,like: gConfig->readInto(test,"test");)
	ConfigManager::Init("ConnectionServer.cfg");

	gConnectionServer = new ConnectionServer();

	// Startup things
	gConnectionServer->Startup();

	// Main loop
	while(1)
	{
		if(kbhit())
			break;

		gConnectionServer->Process();
		msleep(1);
	}

	// Shutdown things
	gConnectionServer->Shutdown();
	delete gConnectionServer;

	return 0;
}

//======================================================================================================================







