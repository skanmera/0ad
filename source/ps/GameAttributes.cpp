#include "precompiled.h"

#include "GameAttributes.h"
#include "Game.h"
#include "ConfigDB.h"
#include "Network/ServerSession.h"
#include "CLogger.h"

using namespace std;

CPlayerSlot::CPlayerSlot(int slotID, CPlayer *pPlayer):
	m_SlotID(slotID),
	m_Assignment(SLOT_OPEN),
	m_pSession(NULL),
	m_SessionID(0),
	m_pPlayer(pPlayer),
	m_Callback(NULL)
{
	ONCE(
		ScriptingInit();
	);
	
	//AddProperty(L"session", (GetFn)&CPlayerSlot::JSI_GetSession);
	AddReadOnlyProperty(L"session", &m_pSession);	
	AddReadOnlyProperty(L"player", &m_pPlayer);
}

CPlayerSlot::~CPlayerSlot()
{}

void CPlayerSlot::ScriptingInit()
{
	AddMethod<bool, &CPlayerSlot::JSI_AssignClosed>("assignClosed", 0);
	AddMethod<bool, &CPlayerSlot::JSI_AssignToSession>("assignToSession", 1);
	AddMethod<bool, &CPlayerSlot::JSI_AssignLocal>("assignLocal", 0);
	AddMethod<bool, &CPlayerSlot::JSI_AssignOpen>("assignOpen", 0);
	AddClassProperty(L"assignment", (GetFn)&CPlayerSlot::JSI_GetAssignment);
//	AddMethod<bool, &CPlayerSlot::JSI_AssignAI>("assignAI", <num_args>);

	CJSObject<CPlayerSlot>::ScriptingInit("PlayerSlot");
}

jsval CPlayerSlot::JSI_GetSession()
{
	if (m_pSession)
		return OBJECT_TO_JSVAL(m_pSession->GetScript());
	else
		return JSVAL_NULL;
}

jsval CPlayerSlot::JSI_GetAssignment()
{
	switch (m_Assignment)
	{
		case SLOT_CLOSED:
			return g_ScriptingHost.UCStringToValue(L"closed");
		case SLOT_OPEN:
			return g_ScriptingHost.UCStringToValue(L"open");
		case SLOT_SESSION:
			return g_ScriptingHost.UCStringToValue(L"session");
/*		case SLOT_AI:*/
		default:
			return INT_TO_JSVAL(m_Assignment);
	}
}

bool CPlayerSlot::JSI_AssignClosed(JSContext *cx, uintN argc, jsval *argv)
{
	AssignClosed();
	return true;
}

bool CPlayerSlot::JSI_AssignOpen(JSContext *cx, uintN argc, jsval *argv)
{
	AssignOpen();
	return true;
}

bool CPlayerSlot::JSI_AssignToSession(JSContext *cx, uintN argc, jsval *argv)
{
	if (argc != 1) return false;
	CNetServerSession *pSession=ToNative<CNetServerSession>(argv[0]);
	if (pSession)
	{
		AssignToSession(pSession);
		return true;
	}
	else
		return true;
}

bool CPlayerSlot::JSI_AssignLocal(JSContext *cx, uintN argc, jsval *argv)
{
	AssignToSessionID(1);
	return true;
}

void CPlayerSlot::CallCallback()
{
	if (m_Callback)
		m_Callback(m_CallbackData, this);
}

void CPlayerSlot::SetAssignment(EPlayerSlotAssignment assignment,
	CNetServerSession *pSession, int sessionID)
{
	m_Assignment=assignment;
	m_pSession=pSession;
	m_SessionID=sessionID;
	
	CallCallback();
}

void CPlayerSlot::AssignClosed()
{
	SetAssignment(SLOT_CLOSED, NULL, -1);
}

void CPlayerSlot::AssignOpen()
{
	SetAssignment(SLOT_OPEN, NULL, -1);
}

void CPlayerSlot::AssignToSession(CNetServerSession *pSession)
{
	SetAssignment(SLOT_SESSION, pSession, pSession->GetID());
	m_pPlayer->SetName(pSession->GetName());
}

void CPlayerSlot::AssignToSessionID(int id)
{
	SetAssignment(SLOT_SESSION, NULL, id);
}

namespace PlayerSlotArray_JS
{
	JSBool GetProperty( JSContext* cx, JSObject* obj, jsval id, jsval* vp )
	{
		CGameAttributes *pInstance=(CGameAttributes *)JS_GetPrivate(cx, obj);
		if (!JSVAL_IS_INT(id))
			return JS_FALSE;
		uint index=g_ScriptingHost.ValueToInt(id);
		
		if (index > pInstance->m_NumSlots)
			return JS_FALSE;

		*vp=OBJECT_TO_JSVAL(pInstance->m_PlayerSlots[index]->GetScript());
		return JS_TRUE;
	}

	JSBool SetProperty( JSContext* cx, JSObject* obj, jsval id, jsval* vp )
	{
		return JS_FALSE;
	}

	JSClass Class = {
		"PlayerSlotArray", JSCLASS_HAS_PRIVATE,
		JS_PropertyStub, JS_PropertyStub,
		GetProperty, SetProperty,
		JS_EnumerateStub, JS_ResolveStub,
		JS_ConvertStub, JS_FinalizeStub
	};

	JSBool Construct( JSContext* cx, JSObject* obj, uint argc, jsval* argv, jsval* rval )
	{
		if (argc != 0)
			return JS_FALSE;

		JSObject *newObj=JS_NewObject(cx, &Class, NULL, obj);
		*rval=OBJECT_TO_JSVAL(newObj);
		return JS_TRUE;
	}
};

CGameAttributes::CGameAttributes():
	m_MapFile("test01.pmp"),
	m_NumSlots(8),
	m_UpdateCB(NULL),
	m_PlayerUpdateCB(NULL),
	m_PlayerSlotAssignmentCB(NULL)
{
	ONCE(
		ScriptingInit();
	);

	m_PlayerSlotArrayJS=g_ScriptingHost.CreateCustomObject("PlayerSlotArray");
	JS_SetPrivate(g_ScriptingHost.GetContext(), m_PlayerSlotArrayJS, this);

	AddSynchedProperty(L"mapFile", &m_MapFile);
	AddSynchedProperty(L"numSlots", &m_NumSlots, &CGameAttributes::OnNumSlotsUpdate);

	m_Players.resize(9);
	for (int i=0;i<9;i++)
		m_Players[i]=new CPlayer(i);
		
	m_Players[0]->SetName(L"Gaia");
	m_Players[0]->SetColour(SPlayerColour(1.0f, 1.0f, 1.0f));

	m_Players[1]->SetName(L"Sally Jessie Rapheal");
	m_Players[1]->SetColour(SPlayerColour(1.0f, 0.0f, 0.0f));

	m_Players[2]->SetName(L"Acumen");
	m_Players[2]->SetColour(SPlayerColour(0.0f, 1.0f, 0.0f));

	m_Players[3]->SetName(L"Boco the Insignificant");
	m_Players[3]->SetColour(SPlayerColour(0.0f, 0.0f, 1.0f));

	m_Players[4]->SetName(L"NoMonkey the Magnificent");
	m_Players[4]->SetColour(SPlayerColour(1.0f, 1.0f, 0.0f));

	m_Players[5]->SetName(L"Wijit");
	m_Players[5]->SetColour(SPlayerColour(1.0f, 0.0f, 1.0f));

	m_Players[6]->SetName(L"Ykkrosh");
	m_Players[6]->SetColour(SPlayerColour(0.0f, 1.0f, 1.0f));

	m_Players[7]->SetName(L"Code Monkey");
	m_Players[7]->SetColour(SPlayerColour(1.0f, 0.5f, 1.0f));

	m_Players[8]->SetName(L"Ykkrosh");
	m_Players[8]->SetColour(SPlayerColour(1.0f, 0.8f, 0.5f));
	
	std::vector<CPlayer *>::iterator it=m_Players.begin();
	++it; // Skip Gaia - gaia doesn't account for a slot
	for (;it != m_Players.end();++it)
	{
		m_PlayerSlots.push_back(new CPlayerSlot((*it)->GetPlayerID()-1, *it));
	}
	// The first player is always the server player in MP, or the local player
	// in SP
	m_PlayerSlots[0]->AssignToSessionID(1);
}

CGameAttributes::~CGameAttributes()
{
	std::vector<CPlayerSlot *>::iterator it=m_PlayerSlots.begin();
	while (it != m_PlayerSlots.end())
	{
		delete (*it)->GetPlayer();
		delete *it;
		++it;
	}

	// PT: ??? - Gaia isn't a player slot, but still needs to be deleted; but
	// this feels rather unpleasantly inconsistent with the above code, so maybe
	// there's a better way to avoid the memory leak.
	delete m_Players[0];
}

void CGameAttributes::ScriptingInit()
{
	g_ScriptingHost.DefineCustomObjectType(&PlayerSlotArray_JS::Class,
		PlayerSlotArray_JS::Construct, 0, NULL, NULL, NULL, NULL);
	
	AddMethod<jsval, &CGameAttributes::JSI_GetOpenSlot>("getOpenSlot", 0);
	AddClassProperty(L"slots", (GetFn)&CGameAttributes::JSI_GetPlayerSlots);

	CJSObject<CGameAttributes>::ScriptingInit("GameAttributes");
}

jsval CGameAttributes::JSI_GetOpenSlot(JSContext *cx, uintN argc, jsval *argv)
{
	vector <CPlayerSlot *>::iterator it;
	for (it = m_PlayerSlots.begin();it != m_PlayerSlots.end();++it)
	{
		if ((*it)->GetAssignment() == SLOT_OPEN)
			return OBJECT_TO_JSVAL((*it)->GetScript());
	}
	return JSVAL_NULL;
}

jsval CGameAttributes::JSI_GetPlayerSlots()
{
	return OBJECT_TO_JSVAL(m_PlayerSlotArrayJS);
}

void CGameAttributes::OnNumSlotsUpdate(CSynchedJSObjectBase *owner)
{
	CGameAttributes *pInstance=(CGameAttributes*)owner;
	
	// Clamp to a preset upper bound.
	CConfigValue *val=g_ConfigDB.GetValue(CFG_MOD, "max_players");
	uint maxPlayers=PS_MAX_PLAYERS;
	if (val)
		val->GetUnsignedInt(maxPlayers);
	if (pInstance->m_NumSlots > maxPlayers)
		pInstance->m_NumSlots = maxPlayers;

	// Resize array according to new number of slots (note that the array
	// size will go up to maxPlayers (above), but will never be made smaller -
	// this to avoid losing player pointers and make sure they are all
	// reclaimed in the end - it's just simpler that way ;-) )
	if (pInstance->m_NumSlots > pInstance->m_PlayerSlots.size())
	{
		for (size_t i=pInstance->m_PlayerSlots.size();i<=pInstance->m_NumSlots;i++)
		{
			CPlayer *pNewPlayer=new CPlayer((uint)i+1);
			pNewPlayer->SetUpdateCallback(pInstance->m_PlayerUpdateCB,
										  pInstance->m_PlayerUpdateCBData);
			pInstance->m_Players.push_back(pNewPlayer);

			CPlayerSlot *pNewSlot=new CPlayerSlot((uint)i, pNewPlayer);
			pNewSlot->SetCallback(pInstance->m_PlayerSlotAssignmentCB,
								  pInstance->m_PlayerSlotAssignmentCBData);
			pInstance->m_PlayerSlots.push_back(pNewSlot);
		}
	}
}

CPlayer *CGameAttributes::GetPlayer(int id)
{
	if (id >= 0 && id < (int)m_Players.size())
	{
		return m_Players[id];
	}
	else
	{
		LOG(ERROR, "", "CGameAttributes::GetPlayer(): Attempt to get player %d (while there only are %d players)", id, m_Players.size());
		return m_Players[0];
	}
}

CPlayerSlot *CGameAttributes::GetSlot(int index)
{
	if (index >= 0 && index < (int)m_PlayerSlots.size())
		return m_PlayerSlots[index];
	else
	{
		LOG(ERROR, "", "CGameAttributes::GetSlot(): Attempt to get slot %d (while there only are %d slots)", index, m_PlayerSlots.size());
		return m_PlayerSlots[0];
	}
}

void CGameAttributes::FinalizeSlots()
{
	// Back up our old slots, and empty the resulting vector
	vector<CPlayerSlot *> oldSlots;
	oldSlots.swap(m_PlayerSlots);
	vector<CPlayer *> oldPlayers;
	oldPlayers.swap(m_Players);
	m_Players.push_back(oldPlayers[0]); // Gaia
	
	// Copy over the slots we want
	uint assignedSlots=0;
	for (size_t i=0;i<oldSlots.size();i++)
	{
		CPlayerSlot *slot=oldSlots[i];
		EPlayerSlotAssignment assignment=slot->GetAssignment();
		if (assignment != SLOT_OPEN && assignment != SLOT_CLOSED)
		{
			m_PlayerSlots.push_back(slot);
			slot->GetPlayer()->SetPlayerID(assignedSlots+1);
			m_Players.push_back(slot->GetPlayer());
			
			assignedSlots++;
		}
		else
		{
			LOG(ERROR, "", "CGameAttributes::FinalizeSlots(): slot %d deleted", i);
			delete slot->GetPlayer();
			delete slot;
		}
	}
	
	m_NumSlots=assignedSlots;
}

void CGameAttributes::SetValue(CStrW name, CStrW value)
{
	ISynchedJSProperty *prop=GetSynchedProperty(name);
	if (prop)
	{
		prop->FromString(value);
	}
}

void CGameAttributes::Update(CStrW name, ISynchedJSProperty *attrib)
{
	if (m_UpdateCB)
		m_UpdateCB(name, attrib->ToString(), m_UpdateCBData);
}

void CGameAttributes::SetPlayerUpdateCallback(CPlayer::UpdateCallback *cb, void *userdata)
{
	m_PlayerUpdateCB=cb;
	m_PlayerUpdateCBData=userdata;
	
	for (size_t i=0;i<m_Players.size();i++)
	{
		m_Players[i]->SetUpdateCallback(cb, userdata);
	}
}

void CGameAttributes::SetPlayerSlotAssignmentCallback(PlayerSlotAssignmentCB *cb, void *userdata)
{
	m_PlayerSlotAssignmentCB=cb;
	m_PlayerSlotAssignmentCBData=userdata;
	
	for (size_t i=0;i<m_PlayerSlots.size();i++)
	{
		m_PlayerSlots[i]->SetCallback(cb, userdata);
	}
}
