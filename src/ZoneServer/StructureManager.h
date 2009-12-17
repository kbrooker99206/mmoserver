/*
---------------------------------------------------------------------------------------
This source file is part of swgANH (Star Wars Galaxies - A New Hope - Server Emulator)
For more information, see http://www.swganh.org


Copyright (c) 2006 - 2008 The swgANH Team

---------------------------------------------------------------------------------------
*/

#ifndef ANH_ZONESERVER_STRUCTUREMANAGER_H
#define ANH_ZONESERVER_STRUCTUREMANAGER_H

#include <vector>
#include <list>

#include "DatabaseManager/DatabaseCallback.h"
#include "MathLib/Vector3.h"
#include "ObjectFactoryCallback.h"
#include "TangibleEnums.h"
#include "WorldManager.h"
#include "Utils/Scheduler.h"

#define 	gStructureManager	StructureManager::getSingletonPtr()

//======================================================================================================================

class Message;
class Database;
class MessageDispatch;
class PlayerObject;
class UIWindow;

namespace Anh_Utils
{
   // class Clock;
    class Scheduler;
    //class VariableTimeScheduler;
}
//======================================================================================================================

typedef std::list<uint64>				ObjectIDList;

enum Structure_QueryType
{
	Structure_Query_NULL						=	0,
	Structure_Query_LoadDeedData				=	1,
	Structure_Query_LoadstructureItem			=	2,

	Structure_Query_delete						=	3,
	Structure_Query_Admin_Data					=	4,
	Structure_Query_Hopper_Data					=	5,
	Structure_Query_Add_Permission				=	6,
	Structure_Query_Remove_Permission			=	7,
	Structure_Query_Check_Permission			=	8,
	Structure_StructureTransfer_Lots_Recipient	=	9,
	Structure_StructureTransfer_Lots_Donor		=	10,
	Structure_HopperUpdate						=	11,
	Structure_HopperDiscard						=	12,
	Structure_GetResourceData					=	13,
	Structure_ResourceDiscardUpdateHopper		=	14,
	Structure_ResourceDiscard					=	15,
	Structure_ResourceRetrieve					=	16,
	Structure_GetDepositPowerData				=	17,
	Structure_GetDepositMaintenanceData			=	18,
	Structure_GetOwnersName			 			=	19,
	
	Structure_GetInactiveHarvesters	 			=	20,
	Structure_GetDestructionStructures			=	21,
	Structure_UpdateStructureDeed				=	22,
	Structure_UpdateCharacterLots				=	23,

};

enum Structure_Async_CommandEnum
{
	Structure_Command_NULL				=	0,
	Structure_Command_AddPermission		=	1,
	Structure_Command_RemovePermission	=	2,
	Structure_Command_Destroy			=	3,
	Structure_Command_PermissionAdmin	=	4,
	Structure_Command_PermissionHopper	=	5,
	Structure_Command_TransferStructure	=	6,
	Structure_Command_RenameStructure	=	7,
	Structure_Command_DiscardHopper		=	8,
	Structure_Command_GetResourceData	=	9,
	Structure_Command_DiscardResource	=	10,
	Structure_Command_RetrieveResource	=	11,
	Structure_Command_PayMaintenance	=	12,
	Structure_Command_DepositPower		=	13,
	Structure_Command_ViewStatus		=	14,
	Structure_Command_ViewStatus_Att2	=	15,
	

};

//======================================================================================================================

struct StructureAsyncCommand
{
	uint64						StructureId;
	uint64						PlayerId;
	uint64						RecipientId;
	uint64						ResourceId;
	uint32						Amount;
	uint8						b1;
	uint8						b2;
	string						CommandString;
	string						PlayerStr;
	string						List;
	string						Name;
	Structure_Async_CommandEnum	Command;
};

struct attributeDetail
{
	string	value;
	uint32	attributeId;
};



//links deeds to structuredata
//TODO still needs to be updated to support several structure types for some placeables
//depending on customization

struct StructureDeedLink
{
	uint32	structure_type;
	uint32	skill_Requirement;
	uint32	item_type;
	string	structureObjectString;
	uint8	requiredLots;
	string	stf_file;
	string	stf_name;
	float	healing_modifier;
};

//templated items that need to be at certain spots on/in the structure
//like signs / campfires/elevator buttons and stuff
struct StructureItemTemplate
{
	uint32				CellNr;
	uint32				structure_id;
	uint32				item_type;
	string				structureObjectString;

	TangibleGroup		tanType;

	string				name;
	string				file;

	Anh_Math::Vector3	mPosition;
	Anh_Math::Vector3	mDirection;

	float			dw;
};

typedef		std::vector<StructureDeedLink*>		DeedLinkList;
typedef		std::vector<StructureItemTemplate*>	StructureItemList;

//======================================================================================================================



//======================================================================================================================


//======================================================================================================================

class StructureManagerAsyncContainer
{
public:

	StructureManagerAsyncContainer(Structure_QueryType qt,DispatchClient* client){ mQueryType = qt; mClient = client; }
	~StructureManagerAsyncContainer(){}

	Structure_QueryType			mQueryType;
	DispatchClient*				mClient;

	uint64						mStructureId;
	uint64						mPlayerId;
	uint64						mTargetId;
	
	uint8						b1;
	uint8						b2;

	int8						name[64];

	PlayerObject*				builder;
	StructureAsyncCommand		command;

};

//======================================================================================================================

class StructureManager : public DatabaseCallback,public ObjectFactoryCallback
{
	friend class ObjectFactory;


	public:
		//System

		static StructureManager*	getSingletonPtr() { return mSingleton; }
		static StructureManager*	Init(Database* database,MessageDispatch* dispatch);

		~StructureManager();

		void					Shutdown();

		virtual void			handleDatabaseJobComplete(void* ref,DatabaseResult* result);
		void					handleObjectReady(Object* object,DispatchClient* client);
		void					handleUIEvent(uint32 action,int32 element,string inputStr,UIWindow* window);

		//=========================================================

		StructureDeedLink*		getDeedData(uint32 type);	//returns the data associated with a certain deed

		StructureItemList*		getStructureItemList(){return(&mItemTemplate);}

		//=========================================================
		//get db data

		//camps

		bool					checkCampRadius(PlayerObject* player);
		bool					checkCityRadius(PlayerObject* player);
		bool					checkinCamp(PlayerObject* player);

		//PlayerStructures
		void					getDeleteStructureMaintenanceData(uint64 structureId, uint64 playerId);

		void					addNametoPermissionList(uint64 structureId, uint64 playerId, string name, string list);
		void					removeNamefromPermissionList(uint64 structureId, uint64 playerId, string name, string list);
		void					checkNameOnPermissionList(uint64 structureId, uint64 playerId, string name, string list, StructureAsyncCommand command);
		void					processVerification(StructureAsyncCommand command, bool owner);
		void					TransferStructureOwnership(StructureAsyncCommand command);

		//returns a confirmatioon code for structure destruction
		string					getCode();

		//
		ObjectIDList*			getStrucureDeleteList(){return &mStructureDeleteList;}
		void					addStructureforDestruction(uint64 iD)
		{
			mStructureDeleteList.push_back(iD);
			gWorldManager->getPlayerScheduler()->addTask(fastdelegate::MakeDelegate(this,&StructureManager::_handleStructureObjectTimers),7,1000,NULL);
		}

		void					addStructureforConstruction(uint64 iD)
		{
			mStructureDeleteList.push_back(iD);
			gWorldManager->getPlayerScheduler()->addTask(fastdelegate::MakeDelegate(this,&StructureManager::_handleStructureObjectTimers),7,mBuildingFenceInterval,NULL);
		}

		void					addStructureforHopperUpdate(uint64 iD)
		{
			mStructureDeleteList.insert(mStructureDeleteList.begin(),iD);// .push_back(iD);
			gWorldManager->getPlayerScheduler()->addTask(fastdelegate::MakeDelegate(this,&StructureManager::_handleStructureObjectTimers),7,5000,NULL);
		}

		//check all active harvesters once in a while they might have been turned off by the db
		bool					_handleStructureDBCheck(uint64 callTime, void* ref);

		void					OpenStructureAdminList(uint64 structureId, uint64 playerId);
		void					OpenStructureHopperList(uint64 structureId, uint64 playerId);


		uint32					getCurrentPower(PlayerObject* player);
		uint32					deductPower(PlayerObject* player, uint32 amount);

		//asynchronously updates the lot count of a player
		void					UpdateCharacterLots(uint64 charId);

	private:


		StructureManager(Database* database,MessageDispatch* dispatch);

		bool	_handleStructureObjectTimers(uint64 callTime, void* ref);

		static StructureManager*	mSingleton;
		static bool					mInsFlag;

		Database*					mDatabase;
		MessageDispatch*			mMessageDispatch;

		DeedLinkList				mDeedLinkList;
		StructureItemList			mItemTemplate;
		ObjectIDList				mStructureDeleteList;
		uint32						mBuildingFenceInterval;

};

#endif

