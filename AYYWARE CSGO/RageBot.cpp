/*
ApocalypseCheats
------------------------------
Contributors:
XBoom
Ma$$A$
madlifer
control1337
CyclesPrograming
FZCat1337
UC Community <3

*/

#include "RageBot.h"
#include "RenderManager.h"
#include "Resolver.h"
#include "Autowall.h"
#include <iostream>
#include "UTIL Functions.h"
#include "Backtrack.h"


#define TICK_INTERVAL			( Interfaces::Globals->interval_per_tick )
#define TIME_TO_TICKS( dt )		( (int)( 0.5f + (float)(dt) / TICK_INTERVAL ) )

static int firedShotsEz;



void CRageBot::Init()
{
	IsAimStepping = false;
	IsLocked = false;
	TargetID = -1;
}

void CRageBot::Draw()
{

}

bool IsAbleToShoot(IClientEntity* pLocal)
{
	CBaseCombatWeapon* pWeapon = (CBaseCombatWeapon*)Interfaces::EntList->GetClientEntityFromHandle(pLocal->GetActiveWeaponHandle());

	if (!pLocal)
		return false;

	if (!pWeapon)
		return false;

	float flServerTime = pLocal->GetTickBase() * Interfaces::Globals->interval_per_tick;

	return (!(pWeapon->GetNextPrimaryAttack() > flServerTime));
}

float hitchance(IClientEntity* pLocal, CBaseCombatWeapon* pWeapon)
{
	//	CBaseCombatWeapon* pWeapon = (CBaseCombatWeapon*)Interfaces::EntList->GetClientEntityFromHandle(pLocal->GetActiveWeaponHandle());
	float hitchance = 101;
	if (!pWeapon) return 0;
	if (Menu::Window.RageBotTab.AccuracyHitchance.GetValue() > 1)
	{//Inaccuracy method
		float inaccuracy = pWeapon->GetInaccuracy();
		if (inaccuracy == 0) inaccuracy = 0.0000001;
		inaccuracy = 1 / inaccuracy;
		hitchance = inaccuracy;

	}
	return hitchance;
}

// (DWORD)g_pNetVars->GetOffset("DT_BaseCombatWeapon", "m_flNextPrimaryAttack");
// You need something like this
bool CanOpenFire() // Creds to untrusted guy
{
	IClientEntity* pLocalEntity = (IClientEntity*)Interfaces::EntList->GetClientEntity(Interfaces::Engine->GetLocalPlayer());
	if (!pLocalEntity)
		return false;

	CBaseCombatWeapon* entwep = (CBaseCombatWeapon*)Interfaces::EntList->GetClientEntityFromHandle(pLocalEntity->GetActiveWeaponHandle());

	float flServerTime = (float)pLocalEntity->GetTickBase() * Interfaces::Globals->interval_per_tick;
	float flNextPrimaryAttack = entwep->GetNextPrimaryAttack();

	std::cout << flServerTime << " " << flNextPrimaryAttack << std::endl;

	return !(flNextPrimaryAttack > flServerTime);
}

void CRageBot::Move(CUserCmd *pCmd, bool &bSendPacket)
{
	IClientEntity* pLocalEntity = (IClientEntity*)Interfaces::EntList->GetClientEntity(Interfaces::Engine->GetLocalPlayer());
	if (!pLocalEntity)
		return;

	// Master switch
	if (!Menu::Window.RageBotTab.Active.GetState())
		return;

	// Anti Aim 
	if (Menu::Window.RageBotTab.AntiAimEnable.GetState())
	{
		static int ChokedPackets = -1;

		CBaseCombatWeapon* pWeapon = (CBaseCombatWeapon*)Interfaces::EntList->GetClientEntityFromHandle(hackManager.pLocal()->GetActiveWeaponHandle());
		if (!pWeapon)
			return;

		if (ChokedPackets < 1 && pLocalEntity->GetLifeState() == LIFE_ALIVE && pCmd->buttons & IN_ATTACK && CanOpenFire() && GameUtils::IsBallisticWeapon(pWeapon))
		{
			bSendPacket = false;
		}
		else
		{
			if (pLocalEntity->GetLifeState() == LIFE_ALIVE)
			{
				DoAntiAim(pCmd, bSendPacket);
			}
			ChokedPackets = -1;
		}
	}

	// Position Adjustment
	if (Menu::Window.RageBotTab.AccuracyPositionAdjustment.GetState())
		PositionAdjustment(pCmd);

	// Aimbot
	if (Menu::Window.RageBotTab.AimbotEnable.GetState())
		DoAimbot(pCmd, bSendPacket);

	// Recoil
	if (Menu::Window.RageBotTab.AccuracyRecoil.GetState())
		DoNoRecoil(pCmd);

	// Aimstep
	if (Menu::Window.RageBotTab.AimbotAimStep.GetState())
	{
		Vector AddAngs = pCmd->viewangles - LastAngle;
		if (AddAngs.Length2D() > 25.f)
		{
			Normalize(AddAngs, AddAngs);
			AddAngs *= 25;
			pCmd->viewangles = LastAngle + AddAngs;
			GameUtils::NormaliseViewAngle(pCmd->viewangles);
		}
	}

	LastAngle = pCmd->viewangles;
}

Vector BestPoint(IClientEntity *targetPlayer, Vector &final)
{
	IClientEntity* pLocal = hackManager.pLocal();

	trace_t tr;
	Ray_t ray;
	CTraceFilter filter;

	filter.pSkip = targetPlayer;
	ray.Init(final + Vector(0, 0, 10), final);
	Interfaces::Trace->TraceRay(ray, MASK_SHOT, &filter, &tr);

	final = tr.endpos;
	return final;
}



// Functionality
void CRageBot::DoAimbot(CUserCmd *pCmd, bool &bSendPacket) // Creds to encore1337 for getting it to work
{
	IClientEntity* pTarget = nullptr;
	IClientEntity* pLocal = hackManager.pLocal();
	Vector Start = pLocal->GetViewOffset() + pLocal->GetOrigin();
	bool FindNewTarget = true;
	//IsLocked = false;

	CSWeaponInfo* weapInfo = ((CBaseCombatWeapon*)Interfaces::EntList->GetClientEntityFromHandle(pLocal->GetActiveWeaponHandle()))->GetCSWpnData();

	// Don't aimbot with the knife..
	CBaseCombatWeapon* pWeapon = (CBaseCombatWeapon*)Interfaces::EntList->GetClientEntityFromHandle(pLocal->GetActiveWeaponHandle());
	if (pWeapon)
	{
		if (pWeapon->GetAmmoInClip() == 0 || !GameUtils::IsBallisticWeapon(pWeapon))
		{
			//TargetID = 0;
			//pTarget = nullptr;
			//HitBox = -1;
			return;
		}
	}
	else
		return;

	if (GameUtils::IsRevolver(pWeapon))
	{
		static int delay = 0;
		delay++;

		if (delay <= 15)
			pCmd->buttons |= IN_ATTACK;
		else
			delay = 0;
	}

	// Make sure we have a good target
	if (IsLocked && TargetID >= 0 && HitBox >= 0)
	{
		pTarget = Interfaces::EntList->GetClientEntity(TargetID);
		if (pTarget  && TargetMeetsRequirements(pTarget))
		{
			HitBox = HitScan(pTarget);
			if (HitBox >= 0)
			{
				Vector ViewOffset = pLocal->GetOrigin() + pLocal->GetViewOffset();
				Vector View;
				Interfaces::Engine->GetViewAngles(View);
				float FoV = FovToPlayer(ViewOffset, View, pTarget, HitBox);
				if (FoV < Menu::Window.RageBotTab.AimbotFov.GetValue())
					FindNewTarget = false;
			}
		}
	}

	// Find a new target, apparently we need to
	if (FindNewTarget)
	{
		TargetID = 0;
		pTarget = nullptr;
		HitBox = -1;

		// Target selection type
		switch (Menu::Window.RageBotTab.TargetSelection.GetIndex())
		{
		case 0:
			TargetID = GetTargetCrosshair();
			break;
		case 1:
			TargetID = GetTargetDistance();
			break;
		case 2:
			TargetID = GetTargetHealth();
			break;
		}

		// Memes
		if (TargetID >= 0)
		{
			pTarget = Interfaces::EntList->GetClientEntity(TargetID);
		}
		else
		{
			pTarget = nullptr;
			HitBox = -1;
		}
	}

	Globals::Target = pTarget;
	Globals::TargetID = TargetID;

	// If we finally have a good target
	if (TargetID >= 0 && pTarget)
	{
		// Get the hitbox to shoot at
		HitBox = HitScan(pTarget);

		if (!CanOpenFire())
			return;

		// Key
		if (Menu::Window.RageBotTab.AimbotKeyPress.GetState())
		{
			int Key = Menu::Window.RageBotTab.AimbotKeyBind.GetKey();
			if (Key >= 0 && !GUI.GetKeyState(Key))
			{
				TargetID = -1;
				pTarget = nullptr;
				HitBox = -1;
				return;
			}
		}

		// Stop key
		int StopKey = Menu::Window.RageBotTab.AimbotStopKey.GetKey();
		if (StopKey >= 0 && GUI.GetKeyState(StopKey))
		{
			TargetID = -1;
			pTarget = nullptr;
			HitBox = -1;
			return;
		}

		float pointscale = Menu::Window.RageBotTab.TargetPointscale.GetValue() - 5.f; // Aim height
																					  //		float value = Menu::Window.RageBotTab.AccuracyHitchance.GetValue(); // Hitchance
		//pCmd->tick_count -= 12;
		Vector Point;
		Vector AimPoint = GetHitboxPosition(pTarget, HitBox) + Vector(0, 0, pointscale);
		//pCmd->tick_count -= 12;
		//CBacktracking::GetBacktrackData;
		
		
		//int teste = (int)floorf(TIME_TO_TICKS(pTarget->GetSimulationTime())) ;


		if (Menu::Window.RageBotTab.TargetMultipoint.GetState())
		{
			Point = BestPoint(pTarget, AimPoint);
		}
		else
		{
			Point = AimPoint;
		}
		
		// Lets open fire
		if (GameUtils::IsScopedWeapon(pWeapon) && !pWeapon->IsScoped() && Menu::Window.RageBotTab.AccuracyAutoScope.GetState()) // Autoscope
		{
			pCmd->buttons |= IN_ATTACK2;
		}
		else
		{
			if ((Menu::Window.RageBotTab.AccuracyHitchance.GetValue() * 1.5 <= hitchance(pLocal, pWeapon)) || Menu::Window.RageBotTab.AccuracyHitchance.GetValue() == 0 || *pWeapon->m_AttributeManager()->m_Item()->ItemDefinitionIndex() == 64)
			{
				if (AimAtPoint(pLocal, Point, pCmd, bSendPacket))
				{
					if (Menu::Window.RageBotTab.AimbotAutoFire.GetState() && !(pCmd->buttons & IN_ATTACK))
					{

						pCmd->buttons |= IN_ATTACK;
					}
					else
					{
						return;
					}
				}
				else if (Menu::Window.RageBotTab.AimbotAutoFire.GetState() && !(pCmd->buttons & IN_ATTACK))
				{

					pCmd->buttons |= IN_ATTACK;
				}
			}
		}

		if (IsAbleToShoot(pLocal) && pCmd->buttons & IN_ATTACK)
			Globals::Shots += 1;

		// SMARTAIM


		static int bulletstart;
		static bool first;
		static int currenttarget = TargetID;




		if (currenttarget != TargetID && TargetID != -1) {
			first = true;
			shotsfired = 0;
		}
		else {
			first = false;
		}


		if (pCmd->buttons & IN_ATTACK) {

			if (first) {

				bulletstart = Globals::Shots;
				first = false;
				currenttarget = TargetID;
			}
			else if (currenttarget == TargetID) {
				shotsfired = (Globals::Shots - bulletstart);
			}
			
		}
		else {
			shotsfired = 0;
		}

		if (TargetID == -1) {
			shotsfired = 0;
		}

		/*
		if (pCmd->buttons & IN_ATTACK && TargetID >= 0 && pTarget) {
			shotsfired += 1;
		}
		else {
			shotsfired = 0;
		} */

		// Stop and Crouch
		if (TargetID >= 0 && pTarget)
		{
			if (Menu::Window.RageBotTab.AccuracyAutoStop.GetState())
			{
				pCmd->forwardmove = 0.f;
				pCmd->sidemove = 0.f;	
			}
			if (Menu::Window.RageBotTab.AccuracyAutoCrouch.GetState())
			{
				pCmd->buttons = IN_DUCK;
			}

		}
	}

	static bool WasFiring = false;
	// Auto Pistol
	if (GameUtils::IsPistol(pWeapon))
	{
		if (pCmd->buttons & IN_ATTACK)
		{
			static bool WasFiring = false;
			WasFiring = !WasFiring;

			if (WasFiring)
			{
				pCmd->buttons &= ~IN_ATTACK;
			}
		}
	}
}



bool CRageBot::TargetMeetsRequirements(IClientEntity* pEntity)
{
	// Is a valid player
	if (pEntity && pEntity->IsDormant() == false && pEntity->IsAlive() && pEntity->GetIndex() != hackManager.pLocal()->GetIndex())
	{
		// Entity Type checks
		ClientClass *pClientClass = pEntity->GetClientClass();
		player_info_t pinfo;
		if (pClientClass->m_ClassID == (int)CSGOClassID::CCSPlayer && Interfaces::Engine->GetPlayerInfo(pEntity->GetIndex(), &pinfo))
		{
			// Team Check
			if (pEntity->GetTeamNum() != hackManager.pLocal()->GetTeamNum() || Menu::Window.RageBotTab.TargetFriendlyFire.GetState())
			{
				// Spawn Check
				if (!pEntity->HasGunGameImmunity())
				{
					return true;
				}
			}
		}
	}

	// They must have failed a requirement
	return false;
}

float CRageBot::FovToPlayer(Vector ViewOffSet, Vector View, IClientEntity* pEntity, int aHitBox)
{
	// Anything past 180 degrees is just going to wrap around
	CONST FLOAT MaxDegrees = 180.0f;

	// Get local angles
	Vector Angles = View;

	// Get local view / eye position
	Vector Origin = ViewOffSet;

	// Create and intiialize vectors for calculations below
	Vector Delta(0, 0, 0);
	//Vector Origin(0, 0, 0);
	Vector Forward(0, 0, 0);

	// Convert angles to normalized directional forward vector
	AngleVectors(Angles, &Forward);
	Vector AimPos = GetHitboxPosition(pEntity, aHitBox);
	// Get delta vector between our local eye position and passed vector
	VectorSubtract(AimPos, Origin, Delta);
	//Delta = AimPos - Origin;

	// Normalize our delta vector
	Normalize(Delta, Delta);

	// Get dot product between delta position and directional forward vectors
	FLOAT DotProduct = Forward.Dot(Delta);

	// Time to calculate the field of view
	return (acos(DotProduct) * (MaxDegrees / PI));
}

int CRageBot::GetTargetCrosshair()
{
	// Target selection
	int target = -1;
	float minFoV = Menu::Window.RageBotTab.AimbotFov.GetValue();

	IClientEntity* pLocal = hackManager.pLocal();
	Vector ViewOffset = pLocal->GetOrigin() + pLocal->GetViewOffset();
	Vector View; Interfaces::Engine->GetViewAngles(View);

	for (int i = 0; i < Interfaces::EntList->GetMaxEntities(); i++) //GetHighestEntityIndex()
	{
		IClientEntity *pEntity = Interfaces::EntList->GetClientEntity(i);
		if (TargetMeetsRequirements(pEntity))
		{
			int NewHitBox = HitScan(pEntity);
			if (NewHitBox >= 0)
			{
				float fov = FovToPlayer(ViewOffset, View, pEntity, 0);
				if (fov < minFoV)
				{
					minFoV = fov;
					target = i;
				}
			}
		}
	}

	return target;
}

int CRageBot::GetTargetDistance()
{
	// Target selection
	int target = -1;
	int minDist = 99999;

	IClientEntity* pLocal = hackManager.pLocal();
	Vector ViewOffset = pLocal->GetOrigin() + pLocal->GetViewOffset();
	Vector View; Interfaces::Engine->GetViewAngles(View);

	for (int i = 0; i < Interfaces::EntList->GetMaxEntities(); i++)
	{
		IClientEntity *pEntity = Interfaces::EntList->GetClientEntity(i);
		if (TargetMeetsRequirements(pEntity))
		{
			int NewHitBox = HitScan(pEntity);
			if (NewHitBox >= 0)
			{
				Vector Difference = pLocal->GetOrigin() - pEntity->GetOrigin();
				int Distance = Difference.Length();
				float fov = FovToPlayer(ViewOffset, View, pEntity, 0);
				if (Distance < minDist && fov < Menu::Window.RageBotTab.AimbotFov.GetValue())
				{
					minDist = Distance;
					target = i;
				}
			}
		}
	}

	return target;
}

int CRageBot::GetTargetHealth()
{
	// Target selection
	int target = -1;
	int minHealth = 101;

	IClientEntity* pLocal = hackManager.pLocal();
	Vector ViewOffset = pLocal->GetOrigin() + pLocal->GetViewOffset();
	Vector View; Interfaces::Engine->GetViewAngles(View);

	for (int i = 0; i < Interfaces::EntList->GetMaxEntities(); i++)
	{
		IClientEntity *pEntity = Interfaces::EntList->GetClientEntity(i);
		if (TargetMeetsRequirements(pEntity))
		{
			int NewHitBox = HitScan(pEntity);
			if (NewHitBox >= 0)
			{
				int Health = pEntity->GetHealth();
				float fov = FovToPlayer(ViewOffset, View, pEntity, 0);
				if (Health < minHealth && fov < Menu::Window.RageBotTab.AimbotFov.GetValue())
				{
					minHealth = Health;
					target = i;
				}
			}
		}
	}

	return target;
}

void CRageBot::AtTarget(CUserCmd *pCmd) {

	IClientEntity* pLocal = hackManager.pLocal();
	CBaseCombatWeapon* pWeapon = (CBaseCombatWeapon*)Interfaces::EntList->GetClientEntityFromHandle(hackManager.pLocal()->GetActiveWeaponHandle());

	if (!pLocal || !pWeapon)
		return;

	Vector eye_position = pLocal->GetEyePosition();

	float best_dist = pWeapon->GetCSWpnData()->flRange;
	IClientEntity* target = nullptr;

	for (int i = 0; i < Interfaces::EntList->GetHighestEntityIndex(); i++)
	{
		IClientEntity *pEntity = Interfaces::EntList->GetClientEntity(i);
		if (TargetMeetsRequirements(pEntity))
		{
			if (Globals::TargetID != -1)
				target = Interfaces::EntList->GetClientEntity(Globals::TargetID);
			else
				target = pEntity;

			Vector target_position = target->GetEyePosition();

			float temp_dist = eye_position.DistTo(target_position);


			



			

			//float angle = atTargetAngles.y;
			//GameUtils::NormaliseViewAngle(angle);

			//float viewAngle = viewAngles.y;
			//GameUtils::NormaliseViewAngle(viewAngle);

			//if (viewAngle >= angle) do stuff
			//else do another stuff





			if (best_dist > temp_dist)
			{
				best_dist = temp_dist;
				//CalcAngle(eye_position, target_position, pCmd->viewangles);
				Vector viewAngles;
				Interfaces::Engine->GetViewAngles(viewAngles);

				//Vector LocalEye = pLocal->GetEyePosition();
				//Vector TargetEye = target->GetEyePosition();

				//Vector testHead = pLocal->GetHeadPos();

				Vector angle;
				VectorAngles(target->GetEyePosition() - pLocal->GetEyePosition(), angle);



				GameUtils::NormaliseViewAngle(angle);



			//CalcAngle(testHead, TargetEye, viewAngles);
			
			//if (pCmd->viewangles.y >= viewAngles.y)
			//	pCmd->viewangles.y = viewAngles.y + 180.0f;
			//else if (pCmd->viewangles.y < viewAngles.y)
			//	pCmd->viewangles.y = viewAngles.y - 180.0f;

			}
			
		}
	}


}

int CRageBot::HitScan(IClientEntity* pEntity)
{
	IClientEntity* pLocal = hackManager.pLocal();
	std::vector<int> HitBoxesToScan;

	// Get the hitboxes to scan
#pragma region GetHitboxesToScan
	int HitScanMode = Menu::Window.RageBotTab.TargetHitscan.GetState();
	int iSmart = Menu::Window.RageBotTab.AccuracySmart.GetValue();
	bool AWall = Menu::Window.RageBotTab.AccuracyAutoWall.GetState();
	bool Multipoint = Menu::Window.RageBotTab.TargetMultipoint.GetState();

	//HitBoxesToScan.push_back((int)CSGOHitboxID::Head);
	static bool enemyHP = false;
	
	if (pEntity->GetVelocity().Length2D() > 3 && pEntity->GetVelocity().Length2D() < 37) {
		HitBoxesToScan.push_back((int)CSGOHitboxID::NeckLower);
		HitBoxesToScan.push_back((int)CSGOHitboxID::Stomach); // 4
		HitBoxesToScan.push_back((int)CSGOHitboxID::Pelvis); // 3
		HitBoxesToScan.push_back((int)CSGOHitboxID::LowerChest); // 5
		HitBoxesToScan.push_back((int)CSGOHitboxID::LeftFoot); // 13
		HitBoxesToScan.push_back((int)CSGOHitboxID::RightFoot); // 12
		HitBoxesToScan.push_back((int)CSGOHitboxID::LeftThigh); // 9
		HitBoxesToScan.push_back((int)CSGOHitboxID::RightThigh); // 8
	}
	else {
	
		HitBoxesToScan.push_back((int)CSGOHitboxID::UpperChest);
		HitBoxesToScan.push_back((int)CSGOHitboxID::RightUpperArm);
		HitBoxesToScan.push_back((int)CSGOHitboxID::LeftUpperArm);
		HitBoxesToScan.push_back((int)CSGOHitboxID::Head);
		HitBoxesToScan.push_back((int)CSGOHitboxID::Neck);
		HitBoxesToScan.push_back((int)CSGOHitboxID::NeckLower);
		HitBoxesToScan.push_back((int)CSGOHitboxID::LowerChest); // 5
		HitBoxesToScan.push_back((int)CSGOHitboxID::LeftFoot); // 13
		HitBoxesToScan.push_back((int)CSGOHitboxID::RightFoot); // 12
		HitBoxesToScan.push_back((int)CSGOHitboxID::LeftThigh); // 9
		HitBoxesToScan.push_back((int)CSGOHitboxID::RightThigh); // 8
		HitBoxesToScan.push_back((int)CSGOHitboxID::Chest);
		HitBoxesToScan.push_back((int)CSGOHitboxID::Stomach); // 4
		HitBoxesToScan.push_back((int)CSGOHitboxID::Pelvis); // 3
		
																 // HEAD 0, // Neck 1, NeckLower 2
	}
	if (AWall == 100) {
		enemyHP = true;
	}
	else {
		enemyHP = false;
	}
	
	
#pragma endregion Get the list of shit to scan

	// check hits
	// check hits
	for (auto HitBoxID : HitBoxesToScan)
	{
		if (AWall <= 99)
		{


			Vector Point = GetHitboxPosition(pEntity, HitBoxID);
		

			float Damage = 0.f;
			Color c = Color(255, 255, 255, 255);
			if (CanHit(Point, &Damage))
			{
				autowalldmgtest[pEntity->GetIndex()] = Damage;

				c = Color(0, 255, 0, 255);


				if (Damage >= Menu::Window.RageBotTab.AccuracyMinimumDamage.GetValue())
				{
					
						return HitBoxID;
					
				}
			}
			else {
				autowalldmgtest[pEntity->GetIndex()] = 0;
			}
		}
		else if (enemyHP) {
			Vector Point = GetHitboxPosition(pEntity, HitBoxID);
			float Damage = 0.f;
			Color c = Color(255, 255, 255, 255);
			if (CanHit(Point, &Damage))
			{
				autowalldmgtest[pEntity->GetIndex()] = Damage;
				c = Color(0, 255, 0, 255);
				if (Damage >= pEntity->GetHealth())
				{

					return HitBoxID;

				}
			}
		}
		else
		{
			if (GameUtils::IsVisible(hackManager.pLocal(), pEntity, HitBoxID))
				return HitBoxID;
		}
	}

	return -1;
}

void CRageBot::PositionAdjustment(CUserCmd* pCmd)
{
	static ConVar* cvar_cl_interp = Interfaces::CVar->FindVar("cl_interp");
	static ConVar* cvar_cl_updaterate = Interfaces::CVar->FindVar("cl_updaterate");
	static ConVar* cvar_sv_maxupdaterate = Interfaces::CVar->FindVar("sv_maxupdaterate");
	static ConVar* cvar_sv_minupdaterate = Interfaces::CVar->FindVar("sv_minupdaterate");
	static ConVar* cvar_cl_interp_ratio = Interfaces::CVar->FindVar("cl_interp_ratio");

	IClientEntity* pLocal = hackManager.pLocal();

	if (!pLocal)
		return;

	CBaseCombatWeapon* pWeapon = (CBaseCombatWeapon*)Interfaces::EntList->GetClientEntityFromHandle(hackManager.pLocal()->GetActiveWeaponHandle());
	if (!pWeapon)
		return;

	float cl_interp = cvar_cl_interp->GetFloat();
	int cl_updaterate = cvar_cl_updaterate->GetInt();
	int sv_maxupdaterate = cvar_sv_maxupdaterate->GetInt();
	int sv_minupdaterate = cvar_sv_minupdaterate->GetInt();
	int cl_interp_ratio = cvar_cl_interp_ratio->GetInt();

	if (sv_maxupdaterate <= cl_updaterate)
		cl_updaterate = sv_maxupdaterate;

	if (sv_minupdaterate > cl_updaterate)
		cl_updaterate = sv_minupdaterate;

	float new_interp = (float)cl_interp_ratio / (float)cl_updaterate;

	if (new_interp > cl_interp)
		cl_interp = new_interp;

	float flSimTime = pLocal->GetSimulationTime();
	float flOldAnimTime = pLocal->GetAnimTime();

	int iTargetTickDiff = (int)(0.5f + (flSimTime - flOldAnimTime) / Interfaces::Globals->interval_per_tick);

	int result = (int)floorf(TIME_TO_TICKS(cl_interp)) + (int)floorf(TIME_TO_TICKS(pLocal->GetSimulationTime()));

	if ((result - pCmd->tick_count) >= -50)
	{
		pCmd->tick_count = result;
	}
}

void CRageBot::DoNoRecoil(CUserCmd *pCmd)
{
	// Ghetto rcs shit, implement properly later
	IClientEntity* pLocal = hackManager.pLocal();
	if (pLocal)
	{
		Vector AimPunch = pLocal->localPlayerExclusive()->GetAimPunchAngle();
		if (AimPunch.Length2D() > 0 && AimPunch.Length2D() < 150)
		{
			pCmd->viewangles -= AimPunch * 2;
			GameUtils::NormaliseViewAngle(pCmd->viewangles);
		}
	}
}

bool CRageBot::AimAtPoint(IClientEntity* pLocal, Vector point, CUserCmd *pCmd, bool &bSendPacket)
{
	bool ReturnValue = false;
	// Get the full angles
	if (point.Length() == 0) return ReturnValue;

	Vector angles;
	Vector src = pLocal->GetOrigin() + pLocal->GetViewOffset();

	CalcAngle(src, point, angles);
	GameUtils::NormaliseViewAngle(angles);

	if (angles[0] != angles[0] || angles[1] != angles[1])
	{
		return ReturnValue;
	}

	
	IsLocked = true;
	//-----------------------------------------------

	// Aim Step Calcs
	Vector ViewOffset = pLocal->GetOrigin() + pLocal->GetViewOffset();
	if (!IsAimStepping)
		LastAimstepAngle = LastAngle; // Don't just use the viewangs because you need to consider aa

	float fovLeft = FovToPlayer(ViewOffset, LastAimstepAngle, Interfaces::EntList->GetClientEntity(TargetID), 0);

	if (fovLeft > 25.0f && Menu::Window.RageBotTab.AimbotAimStep.GetState())
	{
		Vector AddAngs = angles - LastAimstepAngle;
		Normalize(AddAngs, AddAngs);
		AddAngs *= 25;
		LastAimstepAngle += AddAngs;
		GameUtils::NormaliseViewAngle(LastAimstepAngle);
		angles = LastAimstepAngle;
	}
	else
	{
		ReturnValue = true;
	}

	// Silent Aim
	if (Menu::Window.RageBotTab.AimbotSilentAim.GetState() && !Menu::Window.RageBotTab.AimbotPerfectSilentAim.GetState())
	{
		pCmd->viewangles = angles;
	}

	// Normal Aim
	if (!Menu::Window.RageBotTab.AimbotSilentAim.GetState() && !Menu::Window.RageBotTab.AimbotPerfectSilentAim.GetState())
	{
		Interfaces::Engine->SetViewAngles(angles);
	}

	// pSilent Aim 
	Vector Oldview = pCmd->viewangles;

	if (Menu::Window.RageBotTab.AimbotPerfectSilentAim.GetState())
	{
		static int ChokedPackets = -1;
		ChokedPackets++;

		if (ChokedPackets < 6)
		{
			bSendPacket = false;
			pCmd->viewangles = angles;
		}
		else
		{
			bSendPacket = true;
			pCmd->viewangles = Oldview;
			ChokedPackets = -1;
			ReturnValue = false;
		}

		//pCmd->viewangles.z = 0;
	}

	return ReturnValue;
}

namespace AntiAims // CanOpenFire checks for fake anti aims?
{
	// Pitches

	void JitterPitch(CUserCmd *pCmd)
	{
		static bool up = true;
		if (up)
		{
			pCmd->viewangles.x = 45;
			up = !up;
		}
		else
		{
			pCmd->viewangles.x = 89;
			up = !up;
		}
	}

	void FakePitch(CUserCmd *pCmd, bool &bSendPacket)
	{
		static int ChokedPackets = -1;
		ChokedPackets++;
		if (ChokedPackets < 1)
		{
			bSendPacket = false;
			pCmd->viewangles.x = 89;
		}
		else
		{
			bSendPacket = true;
			pCmd->viewangles.x = 51;
			ChokedPackets = -1;
		}
	}

	void StaticJitter(CUserCmd *pCmd)
	{
		static bool down = true;
		if (down)
		{
			pCmd->viewangles.x = 179.0f;
			down = !down;
		}
		else
		{
			pCmd->viewangles.x = 89.0f;
			down = !down;
		}
	}

	// Yaws

	void FastSpin(CUserCmd *pCmd)
	{
		static int y2 = -179;
		int spinBotSpeedFast = 100;

		y2 += spinBotSpeedFast;

		if (y2 >= 179)
			y2 = -179;

		pCmd->viewangles.y = y2;
	}

	void FakeEdge(CUserCmd *pCmd, bool &bSendPacket)
	{
		IClientEntity* pLocal = hackManager.pLocal();

		Vector vEyePos = pLocal->GetOrigin() + pLocal->GetViewOffset();

		CTraceFilter filter;
		filter.pSkip = Interfaces::EntList->GetClientEntity(Interfaces::Engine->GetLocalPlayer());

		for (int y = 0; y < 360; y++)
		{
			Vector qTmp(10.0f, pCmd->viewangles.y, 0.0f);
			qTmp.y += y;

			if (qTmp.y > 180.0)
				qTmp.y -= 360.0;
			else if (qTmp.y < -180.0)
				qTmp.y += 360.0;

			GameUtils::NormaliseViewAngle(qTmp);

			Vector vForward;

			VectorAngles(qTmp, vForward);

			float fLength = (19.0f + (19.0f * sinf(DEG2RAD(10.0f)))) + 7.0f;
			vForward *= fLength;

			trace_t tr;

			Vector vTraceEnd = vEyePos + vForward;

			Ray_t ray;

			ray.Init(vEyePos, vTraceEnd);
			Interfaces::Trace->TraceRay(ray, MASK_PLAYERSOLID_BRUSHONLY, &filter, &tr);

			if (tr.fraction != 1.0f)
			{
				Vector angles;

				Vector vNegative = Vector(tr.plane.normal.x * -1.0f, tr.plane.normal.y * -1.0f, tr.plane.normal.z * -1.0f);

				VectorAngles(vNegative, angles);

				GameUtils::NormaliseViewAngle(angles);

				qTmp.y = angles.y;

				GameUtils::NormaliseViewAngle(qTmp);

				trace_t trLeft, trRight;

				Vector vLeft, vRight;
				VectorAngles(qTmp + Vector(0.0f, 30.0f, 0.0f), vLeft);
				VectorAngles(qTmp + Vector(0.0f, 30.0f, 0.0f), vRight);

				vLeft *= (fLength + (fLength * sinf(DEG2RAD(30.0f))));
				vRight *= (fLength + (fLength * sinf(DEG2RAD(30.0f))));

				vTraceEnd = vEyePos + vLeft;

				ray.Init(vEyePos, vTraceEnd);
				Interfaces::Trace->TraceRay(ray, MASK_PLAYERSOLID_BRUSHONLY, &filter, &trLeft);

				vTraceEnd = vEyePos + vRight;

				ray.Init(vEyePos, vTraceEnd);
				Interfaces::Trace->TraceRay(ray, MASK_PLAYERSOLID_BRUSHONLY, &filter, &trRight);

				if ((trLeft.fraction == 1.0f) && (trRight.fraction != 1.0f))
					qTmp.y -= 90.f;
				else if ((trLeft.fraction != 1.0f) && (trRight.fraction == 1.0f))
					qTmp.y += 90.f;

				if (qTmp.y > 180.0)
					qTmp.y -= 360.0;
				else if (qTmp.y < -180.0)
					qTmp.y += 360.0;

				pCmd->viewangles.y = qTmp.y;

				int offset = Menu::Window.RageBotTab.AntiAimOffset.GetValue();

				static int ChokedPackets = -1;
				ChokedPackets++;
				if (ChokedPackets < 1)
				{
					bSendPacket = false; // +=180?
				}
				else
				{
					bSendPacket = true;
					pCmd->viewangles.y -= offset;
					ChokedPackets = -1;
				}
				return;
			}
		}
		pCmd->viewangles.y += 360.0f;
	}


	void Static(CUserCmd *pCmd)
	{
		pCmd->viewangles.y -= 180.0f;
	}

	void Forward(CUserCmd *pCmd)
	{
		pCmd->viewangles.y += 180.0f;
	}

	void SidewaysLeft(CUserCmd *pCmd)
	{
		pCmd->viewangles.y -= 90.0f;
	}

	void SidewaysRight(CUserCmd *pCmd)
	{
		pCmd->viewangles.y += 90.0f;
	}

	void LBYBasic(CUserCmd *pCmd, bool &bSendPacket)
	{
		static int StartLbyBreaker;
		static int testerino;

		if (GetAsyncKeyState(0xA4) && testerino == 4) // A -  0x41   // N - 0x4E
		{
			//pCmd->viewangles.y -= 90.0f;
			testerino = 3;
		}

		else if (GetAsyncKeyState(0xA4) && testerino == 3)
		{ // D - 0x44    H -0x48 
		  //pCmd->viewangles.y += 90.0f;
			testerino = 4;

		}

		else {

			if (testerino == 3) {

				testerino = 3;

				
					pCmd->viewangles.y += 90;
					antiAimSide = true; // RIGHT HEAD
					

				
			}
			else {

				testerino = 4;
				
				
					pCmd->viewangles.y -= 90;
					antiAimSide = false; // RIGHT HEAD
					
				

			}
		}

	
	}

	void fakeSidewaysLBY(CUserCmd *pCmd, bool &bSendPacket)
	{

		hackManager.pLocal()->GetLowerBodyYaw();
		static bool bHasCorrectTiming;
		static float flOldLBY;
		static float flNextUpdateTime;
		static bool bDidLBYUpdate;
		static float realAngleTest;
		INetChannelInfo *nci = Interfaces::Engine->GetNetChannelInfo();
		static int testerino;

		static int startlbybreaker;


		if (GetAsyncKeyState(0xA4) && testerino == 4) // A -  0x41   // N - 0x4E
		{
			//pCmd->viewangles.y -= 90.0f;
			testerino = 3;
		}

		else if (GetAsyncKeyState(0xA4) && testerino == 3)
		{ // D - 0x44    H -0x48 
		  //pCmd->viewangles.y += 90.0f;
			testerino = 4;

		}

		else {

			if (testerino == 3) {

				testerino = 3;


				if (hackManager.pLocal()->GetVelocity().Length2D() <= 1.0f && hackManager.pLocal()->GetFlags() & FL_ONGROUND)
				{
					if (!bHasCorrectTiming)
					{
						if (hackManager.pLocal()->GetLowerBodyYaw() != flOldLBY)
						{
							flNextUpdateTime = Interfaces::Globals->curtime + 1.1f - nci->GetAvgLatency(FLOW_OUTGOING);
							bHasCorrectTiming = true;
						}
					}
					else
					{
						if (Interfaces::Globals->curtime >= flNextUpdateTime)
						{
							bDidLBYUpdate = true;
							flNextUpdateTime = Interfaces::Globals->curtime + 1.1f - nci->GetAvgLatency(FLOW_OUTGOING);
						}

						if (startlbybreaker != LBYBreakerTimer)
						{
							startlbybreaker = LBYBreakerTimer;
							bDidLBYUpdate = false;
							pCmd->viewangles.y += 90.f;
							if (fabsf(hackManager.pLocal()->GetEyeAngles().y - hackManager.pLocal()->GetLowerBodyYaw()) < 35 || fabsf(hackManager.pLocal()->GetLowerBodyYaw() - pCmd->viewangles.y) < 35) {
								pCmd->viewangles.y += 36;
							}
						}
						else
						{
							if (pCmd->tick_count % 2 == 1)
							{
								pCmd->viewangles.y += 90.f;
								//realAngleTest = pCmd->viewangles.y;
								bSendPacket = true;
							}
							else
							{

								pCmd->viewangles.y += 270.f;
								

								bSendPacket = false;
							}
						}

						if (fabsf(hackManager.pLocal()->GetEyeAngles().y - hackManager.pLocal()->GetLowerBodyYaw()) < 35 || fabsf(hackManager.pLocal()->GetLowerBodyYaw() - pCmd->viewangles.y) < 35) {
							pCmd->viewangles.y += 36;
						}
						

					}
				}
				else {
					pCmd->viewangles.y -= 180.0f;
					bHasCorrectTiming = false;
				}


				antiAimSide = true; // RIGHT HEAD



			}
			else {

				testerino = 4;


				if (hackManager.pLocal()->GetVelocity().Length2D() <= 1.0f && hackManager.pLocal()->GetFlags() & FL_ONGROUND)
				{
					if (!bHasCorrectTiming)
					{
						if (hackManager.pLocal()->GetLowerBodyYaw() != flOldLBY)
						{
							flNextUpdateTime = Interfaces::Globals->curtime + 1.1f - nci->GetAvgLatency(FLOW_OUTGOING);
							bHasCorrectTiming = true;
						}
					}
					else
					{
						if (Interfaces::Globals->curtime >= flNextUpdateTime)
						{
							bDidLBYUpdate = true;
							flNextUpdateTime = Interfaces::Globals->curtime + 1.1f - nci->GetAvgLatency(FLOW_OUTGOING);
						}
						if (startlbybreaker != LBYBreakerTimer)
						{
							startlbybreaker = LBYBreakerTimer;
							bDidLBYUpdate = false;
							pCmd->viewangles.y -= 90.f;
							if (fabsf(hackManager.pLocal()->GetEyeAngles().y - hackManager.pLocal()->GetLowerBodyYaw()) < 35 || fabsf(hackManager.pLocal()->GetLowerBodyYaw() - pCmd->viewangles.y) < 35) {
								pCmd->viewangles.y += 36;
							}
						}
						else
						{
							if (pCmd->tick_count % 2 == 1)
							{
								pCmd->viewangles.y -= 90.f;
								//realAngleTest = pCmd->viewangles.y;
								bSendPacket = true;
							}
							else
							{
								pCmd->viewangles.y -= 270.f;
								
								bSendPacket = false;
							}
						}

						if (fabsf(hackManager.pLocal()->GetEyeAngles().y - hackManager.pLocal()->GetLowerBodyYaw()) < 35 || fabsf(hackManager.pLocal()->GetLowerBodyYaw() - pCmd->viewangles.y) < 35) {
							pCmd->viewangles.y += 36;
						}

					}
				}
				else {
					pCmd->viewangles.y -= 180.0f;
					bHasCorrectTiming = false;
				}
				antiAimSide = false; // RIGHT HEAD



			}
		}

		
		
		

		
	}



	void fakelowerbody2(CUserCmd *pCmd, bool &bSendPacket)
	{
		static int ChokedPackets = -1;
		ChokedPackets++;
        #define RandomInt(min, max) (rand() % (max - min + 1) + min)
		if (ChokedPackets < 1)
		{
			bSendPacket = false;
			pCmd->viewangles.y -= hackManager.pLocal()->GetLowerBodyYaw() + RandomInt(30, 61);
		}
		else
		{
			bSendPacket = true;
			pCmd->viewangles.y += hackManager.pLocal()->GetLowerBodyYaw() - RandomInt(180, 360);
			ChokedPackets = -1;
		}
	}

	void Jitter(CUserCmd *pCmd)
	{
		static int jitterangle = 0;

		if (jitterangle <= 1)
		{
			pCmd->viewangles.y += 90;
		}
		else if (jitterangle > 1 && jitterangle <= 3)
		{
			pCmd->viewangles.y -= 90;
		}

		int re = rand() % 4 + 1;


		if (jitterangle <= 1)
		{
			if (re == 4)
				pCmd->viewangles.y += 180;
			jitterangle += 1;
		}
		else if (jitterangle > 1 && jitterangle <= 3)
		{
			if (re == 4)
				pCmd->viewangles.y -= 180;
			jitterangle += 1;
		}
		else
		{
			jitterangle = 0;
		}
	}



	void FakeStatic(CUserCmd *pCmd, bool &bSendPacket)
	{
		static int ChokedPackets = -1;
		ChokedPackets++;
		if (ChokedPackets < 1)
		{
			bSendPacket = false;
			static int y2 = -179;
			int spinBotSpeedFast = 360.0f / 1.618033988749895f;;

			y2 += spinBotSpeedFast;

			if (y2 >= 179)
				y2 = -179;

			pCmd->viewangles.y = y2;
		}
		else
		{
			bSendPacket = true;
			pCmd->viewangles.y -= 180;
			ChokedPackets = -1;
		}
	}

	void TJitter(CUserCmd *pCmd)
	{
		static bool Turbo = true;
		if (Turbo)
		{
			pCmd->viewangles.y -= 90;
			Turbo = !Turbo;
		}
		else
		{
			pCmd->viewangles.y += 90;
			Turbo = !Turbo;
		}
	}

	void TFake(CUserCmd *pCmd, bool &bSendPacket)
	{
		static int ChokedPackets = -1;
		ChokedPackets++;
		if (ChokedPackets < 1)
		{
			bSendPacket = false;
			pCmd->viewangles.y = -90;
		}
		else
		{
			bSendPacket = true;
			pCmd->viewangles.y = 90;
			ChokedPackets = -1;
		}
	}

	void FakeJitter(CUserCmd* pCmd, bool &bSendPacket)
	{
		static int jitterangle = 0;

		if (jitterangle <= 1)
		{
			pCmd->viewangles.y += 135;
		}
		else if (jitterangle > 1 && jitterangle <= 3)
		{
			pCmd->viewangles.y += 225;
		}

		static int iChoked = -1;
		iChoked++;
		if (iChoked < 1)
		{
			bSendPacket = false;
			if (jitterangle <= 1)
			{
				pCmd->viewangles.y += 45;
				jitterangle += 1;
			}
			else if (jitterangle > 1 && jitterangle <= 3)
			{
				pCmd->viewangles.y -= 45;
				jitterangle += 1;
			}
			else
			{
				jitterangle = 0;
			}
		}
		else
		{
			bSendPacket = true;
			iChoked = -1;
		}
	}

	void Static4(CUserCmd *pCmd)
	{
		static bool aa1 = false;
		aa1 = !aa1;
		if (aa1)
		{
			static bool turbo = false;
			turbo = !turbo;
			if (turbo)
			{
				pCmd->viewangles.y -= 90;
			}
			else
			{
				pCmd->viewangles.y += 90;
			}
		}
		else
		{
			pCmd->viewangles.y -= 180;
		}
	}

	void fakelowerbody(CUserCmd *pCmd, bool &bSendPacket)
	{
		static bool f_flip = true;
		f_flip = !f_flip;

		if (f_flip)
		{
			pCmd->viewangles.y -= hackManager.pLocal()->GetLowerBodyYaw() + 90.00f;
			bSendPacket = false;
		}
		else if (!f_flip)
		{
			pCmd->viewangles.y += hackManager.pLocal()->GetLowerBodyYaw() - 90.00f;
			bSendPacket = true;
		}
	}

	void Up(CUserCmd *pCmd)
	{
		pCmd->viewangles.x = -89.0f;
	}

	void Zero(CUserCmd *pCmd)
	{
		pCmd->viewangles.x = 0.f;
	}


	void RealSpinBot(CUserCmd *pCmd)
	{
		int speed = Menu::Window.RageBotTab.AntiAimSpinbot.GetValue();
		float speedmulti = speed * 1000.0f;
		int yaw = Interfaces::Globals->curtime * speedmulti;
		pCmd->viewangles.y = yaw;
	}


	void RealSpinBot1(CUserCmd *pCmd, bool &bSendPacket)
	{
		bSendPacket = true;
		int speed = Menu::Window.RageBotTab.AntiAimSpinbot.GetValue();
		float speedmulti = speed * 1000.0f;
		int yaw = Interfaces::Globals->curtime * speedmulti;
		pCmd->viewangles.y = yaw;
	}

	void Backwards1(CUserCmd *pCmd, bool &bSendPacket)
	{
		bSendPacket = true; //fake angle
		pCmd->viewangles.y -= 180.0f;
	}

	void Forward1(CUserCmd *pCmd, bool &bSendPacket)
	{
		bSendPacket = true; //fake angle
		pCmd->viewangles.y += 180.0f;
	}

	void SidewaysLeft1(CUserCmd *pCmd, bool &bSendPacket)
	{
		bSendPacket = true; //fake angle
		pCmd->viewangles.y -= 90.0f;
	}

	void SidewaysRight1(CUserCmd *pCmd, bool &bSendPacket)
	{
		bSendPacket = true; //fake angle
		pCmd->viewangles.y += 90.0f;
	}


	void MoveFix(CUserCmd *cmd, Vector &realvec)
	{
		Vector vMove(cmd->forwardmove, cmd->sidemove, cmd->upmove);
		float flSpeed = sqrt(vMove.x * vMove.x + vMove.y * vMove.y), flYaw;
		Vector vMove2;
		VectorAngles(vMove, vMove2);

		flYaw = DEG2RAD(cmd->viewangles.y - realvec.y + vMove2.y);
		cmd->forwardmove = cos(flYaw) * flSpeed;
		cmd->sidemove = sin(flYaw) * flSpeed;

		if (cmd->viewangles.x < -90.f || cmd->viewangles.x > 90.f)
			cmd->forwardmove = -cmd->forwardmove;
	}

	void BackJitter(CUserCmd *pCmd)
	{
		
		int random;
		int maxJitter;
		float temp;
			
		pCmd->viewangles.y += 180.0f;
		random = rand() % 100;
		maxJitter = rand() % (85 - 70 + 1) + 70;
		temp = maxJitter - (rand() % maxJitter);
		if (random < 35 + (rand() % 15))
			pCmd->viewangles.y -= temp;
		else if (random < 85 + (rand() % 15))
			pCmd->viewangles.y += temp;
		

	}

	void AimAtTarget(CUserCmd *pCmd)
	{
		IClientEntity* pLocal = hackManager.pLocal();

		CBaseCombatWeapon* pWeapon = (CBaseCombatWeapon*)Interfaces::EntList->GetClientEntityFromHandle(hackManager.pLocal()->GetActiveWeaponHandle());

		if (!pLocal || !pWeapon)
			return;

		Vector eye_position = pLocal->GetEyePosition();

		float best_dist = pWeapon->GetCSWpnData()->flRange;

		IClientEntity* target = Interfaces::EntList->GetClientEntity(Globals::TargetID);

		if (target == NULL)
			return;

		if (target == pLocal)
			return;

		if ((target->GetTeamNum() == pLocal->GetTeamNum()) || target->IsDormant() || !target->IsAlive() || target->GetHealth() <= 0)
			return;

		Vector target_position = target->GetEyePosition();

		Vector targetHeadpos = target->GetHeadPos();
		
		float temp_dist = eye_position.DistTo(targetHeadpos);

		if (lineFakeAngle)

		if (best_dist > temp_dist)
		{
			best_dist = temp_dist;
			CalcAngle(eye_position, targetHeadpos, pCmd->viewangles);
		}
	}

	void JitterBackward1(CUserCmd *pCmd, bool &bSendPacket)
	{
		int random;
		int maxJitter;
		float temp;
		bSendPacket = true; //fake angle
		pCmd->viewangles.y += 180.0f;
		random = rand() % 100;
		maxJitter = rand() % (85 - 70 + 1) + 70;
		temp = maxJitter - (rand() % maxJitter);
		if (random < 35 + (rand() % 15))
			pCmd->viewangles.y -= temp;
		else if (random < 85 + (rand() % 15))
			pCmd->viewangles.y += temp;
	}


	void EdgeDetect(CUserCmd* pCmd, bool &bSendPacket)
	{
		//Ray_t ray;
		//trace_t tr;

		IClientEntity* pLocal = hackManager.pLocal();

		CTraceFilter traceFilter;
		traceFilter.pSkip = pLocal;

		bool bEdge = false;

		Vector angle;
		Vector eyePos = pLocal->GetOrigin() + pLocal->GetViewOffset();

		for (float i = 0; i < 360; i++)
		{
			Vector vecDummy(10.f, pCmd->viewangles.y, 0.f);
			vecDummy.y += i;

			Vector forward = vecDummy.Forward();

			//vecDummy.NormalizeInPlace();

			float flLength = ((16.f + 3.f) + ((16.f + 3.f) * sin(DEG2RAD(10.f)))) + 7.f;
			forward *= flLength;

			Ray_t ray;
			CGameTrace tr;

			ray.Init(eyePos, (eyePos + forward));
			Interfaces::Trace->EdgeTraceRay(ray, traceFilter, tr, true);

			if (tr.fraction != 1.0f)
			{
				Vector negate = tr.plane.normal;
				negate *= -1;

				Vector vecAng = negate.Angle();

				vecDummy.y = vecAng.y;

				//vecDummy.NormalizeInPlace();
				trace_t leftTrace, rightTrace;

				Vector left = (vecDummy + Vector(0, 45, 0)).Forward(); // or 45
				Vector right = (vecDummy - Vector(0, 45, 0)).Forward();

				left *= (flLength * cosf(rad(30)) * 2); //left *= (len * cosf(rad(30)) * 2);
				right *= (flLength * cosf(rad(30)) * 2); // right *= (len * cosf(rad(30)) * 2);

				ray.Init(eyePos, (eyePos + left));
				Interfaces::Trace->EdgeTraceRay(ray, traceFilter, leftTrace, true);

				ray.Init(eyePos, (eyePos + right));
				Interfaces::Trace->EdgeTraceRay(ray, traceFilter, rightTrace, true);

				if ((leftTrace.fraction == 2.f) && (rightTrace.fraction != 2.f))
				{
					vecDummy.y -= 45; // left
				}
				else if ((leftTrace.fraction != 2.f) && (rightTrace.fraction == 2.f))
				{
					vecDummy.y += 45; // right     
				}

				angle.y = vecDummy.y;
				angle.y += 360;
				bEdge = true;
			}
		}

		if (bEdge)
		{
			static bool turbo = true;
			bool aaEdge = true;

			if (aaEdge == false)
			{
				//Nothing
			}
			else if (aaEdge == true)
			{
				pCmd->viewangles.y = angle.y;
			}

		}
	}
}


void FakeWalk(CUserCmd * pCmd, bool & bSendPacket)
{
	IClientEntity* pLocal = hackManager.pLocal();
	if (GetAsyncKeyState(VK_SHIFT))
	{
		static int iChoked = -1;
		iChoked++;

		if (iChoked < 1)
		{
			bSendPacket = false;

			

			pCmd->tick_count += 10.95; // 10.95
			pCmd->command_number += 5.07 + pCmd->tick_count % 2 ? 0 : 1; // 5

			pCmd->buttons |= pLocal->GetMoveType() == IN_BACK;
			pCmd->forwardmove = pCmd->sidemove = 0.f;
		}
		else
		{
			bSendPacket = true;
			iChoked = -1;

			Interfaces::Globals->frametime *= (pLocal->GetVelocity().Length2D()) / 10; // 10
			pCmd->buttons |= pLocal->GetMoveType() == IN_FORWARD;
		}
	}
}

// AntiAim
void CRageBot::DoAntiAim(CUserCmd *pCmd, bool &bSendPacket) // pCmd->viewangles.y = 0xFFFFF INT_MAX or idk
{
	IClientEntity* pLocal = hackManager.pLocal();

	if ((pCmd->buttons & IN_USE) || pLocal->GetMoveType() == MOVETYPE_LADDER)
		return;

	// If the aimbot is doing something don't do anything
	if ((IsAimStepping || pCmd->buttons & IN_ATTACK) && !Menu::Window.RageBotTab.AimbotPerfectSilentAim.GetState())
		return;

	// Weapon shit
	CBaseCombatWeapon* pWeapon = (CBaseCombatWeapon*)Interfaces::EntList->GetClientEntityFromHandle(hackManager.pLocal()->GetActiveWeaponHandle());
	if (pWeapon)
	{
		CSWeaponInfo* pWeaponInfo = pWeapon->GetCSWpnData();
		// Knives or grenades
		if (!GameUtils::IsBallisticWeapon(pWeapon))
		{
			if (Menu::Window.RageBotTab.AntiAimKnife.GetState())
			{
				if (!CanOpenFire() || pCmd->buttons & IN_ATTACK2)
					return;
			}
			else
			{
				return;
			}
		}
	}


	static int LBYBreaker;

	//AntiAims::AimAtTarget(pCmd);
	
	FakeWalk(pCmd, bSendPacket);
	// Don't do antiaim
	// if (DoExit) return;

	// Anti-Aim Pitch
	switch (Menu::Window.RageBotTab.AntiAimPitch.GetIndex()) // Magic pitch is 69.69?
	{
	case 0:
		// No Pitch AA
		break;
	case 1:
		// Down
		//AntiAims::StaticPitch(pCmd, false);
		pCmd->viewangles.x = 89.0f; // EMOTION
		break;
	case 2:
		// Half Down
		AntiAims::Up(pCmd);
		break;
	case 3:
		// SMAC / Casual safe
		AntiAims::Zero(pCmd);
		break;
	}

	
	AtTarget(pCmd);
	
	//Anti-Aim Yaw
	switch (Menu::Window.RageBotTab.AntiAimYaw.GetIndex())
	{
	case 0:
		// No Yaw AA
		break;
	case 1:
		// Fake Inverse
		AntiAims::LBYBasic(pCmd, bSendPacket);
		//AntiAims::aimAA(pCmd);
		break;
	case 2:
		// Fake Sideways
		AntiAims::Static(pCmd);
		break;
	case 3:
		// Fake Static
		AntiAims::BackJitter(pCmd);
		break;
	case 4:
		AntiAims::RealSpinBot(pCmd);
		break;
	case 5:
		AntiAims::fakeSidewaysLBY(pCmd, bSendPacket);
		break;
	}




		//AtTarget(pCmd);
		// LBY BREAKER BASED ANTIAIM
		if (Menu::Window.RageBotTab.AntiAimLBY.GetState()) {
			if (LBYBreaker != LBYBreakerTimer)
			{
				LBYBreaker = LBYBreakerTimer;
				/*Fake YaW*/
				switch (Menu::Window.RageBotTab.AntiAimYawFake.GetIndex())
				{
				case 0:
					// No Yaw AA
					break;
				case 1:
					// SidewaysRight1         
					AntiAims::SidewaysRight1(pCmd, bSendPacket);
					break;
				case 2:
					// LegitTrollExtreme
					AntiAims::SidewaysLeft1(pCmd, bSendPacket);
					break;
				case 3:
					// LegitTrollExtreme
					AntiAims::Backwards1(pCmd, bSendPacket);
					break;
				case 4:
					// LegitTrollExtreme
					AntiAims::Forward1(pCmd, bSendPacket);
					break;
				case 5:
					// LegitTrollExtreme
					AntiAims::JitterBackward1(pCmd, bSendPacket);
					break;
				case 6:
					// LegitTrollExtreme
					AntiAims::RealSpinBot1(pCmd, bSendPacket);
					break;
				}

				if (fabsf(pLocal->GetEyeAngles().y - pLocal->GetLowerBodyYaw()) < 35 || fabsf(pLocal->GetLowerBodyYaw() - pCmd->viewangles.y) < 35) {
					pCmd->viewangles.y += 35;
				}


			}

		}
		else {

			/*Fake YaW*/
			switch (Menu::Window.RageBotTab.AntiAimYawFake.GetIndex())
			{
			case 0:
				// No Yaw AA
				break;
			case 1:
				// SidewaysRight1         
				AntiAims::SidewaysRight1(pCmd, bSendPacket);
				break;
			case 2:
				// LegitTrollExtreme
				AntiAims::SidewaysLeft1(pCmd, bSendPacket);
				break;
			case 3:
				// LegitTrollExtreme
				AntiAims::Backwards1(pCmd, bSendPacket);
				break;
			case 4:
				// LegitTrollExtreme
				AntiAims::Forward1(pCmd, bSendPacket);
				break;
			case 5:
				// LegitTrollExtreme
				AntiAims::JitterBackward1(pCmd, bSendPacket);
				break;
			case 6:
				// LegitTrollExtreme
				AntiAims::RealSpinBot1(pCmd, bSendPacket);
				break;
			}
		}


		/*
		if (islbyupdate == true)
		{
			// antiAimSide = true + 90     false = -90
			bSendPacket = true;

			if (antiAimSide == true) {
				pCmd->viewangles.y -= 180;
			}
			else {
				pCmd->viewangles.y += 180;
			}

			if (fabsf(pLocal->GetEyeAngles().y - pLocal->GetLowerBodyYaw()) < 35 || fabsf(pLocal->GetLowerBodyYaw() - pCmd->viewangles.y) < 35) {
				pCmd->viewangles.y += 36;
			}
		}
		else {
			
			if (fabsf(pLocal->GetEyeAngles().y - pLocal->GetLowerBodyYaw()) < 35 || fabsf(pLocal->GetLowerBodyYaw() - pCmd->viewangles.y) < 35) {
				pCmd->viewangles.y += 36;
			}
		} */

		
		

		//if (GetAsyncKeyState(0x56))
		//{ // IF V PRESSED RANDOM SHIT HERE
		//	pCmd->viewangles.y = pLocal->GetLowerBodyYaw() + std::rand() % (90 + 1 - 35) + 35;
		//}
		
		
		if (Menu::Window.RageBotTab.AimbotChangeSideAA.GetState()) {
			if (switchAntiAimSide == true) {
					pCmd->viewangles.y += 180;
			}
		}

		//if (fabsf(pLocal->GetEyeAngles().y - pLocal->GetLowerBodyYaw()) < 35 || fabsf(pLocal->GetLowerBodyYaw() - pCmd->viewangles.y) < 35) {
		//	pCmd->viewangles.y += 36;
		//}

		//pCmd->viewangles.y += 90;
		//antiAimSide = true; // RIGHT HEAD

	// Edge Anti Aim
	//AntiAims::EdgeDetect(pCmd, bSendPacket); 

	// Angle offset
	pCmd->viewangles.y += Menu::Window.RageBotTab.AntiAimOffset.GetValue();

	


	
}

