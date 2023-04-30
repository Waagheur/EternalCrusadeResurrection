// DemoMotion.cpp
// Copyright (C) 2010 Audiokinetic Inc
/// \file
/// Defines all methods declared in DemoMotion.h.

#include "stdafx.h"

#include <AK/SoundEngine/Common/AkSoundEngine.h>    // Sound engine
#include <AK/MotionEngine/Common/AkMotionEngine.h>	// Motion Engine (required only for playback of Motion objects)

#include "../WwiseProject/GeneratedSoundBanks/Wwise_IDs.h"		// IDs generated by Wwise
#include "Menu.h"
#include "DemoMotion.h"
#include "IntegrationDemo.h"
#include "InputMgr.h"


/////////////////////////////////////////////////////////////////////
// DemoMotion Constant Member Initialization
/////////////////////////////////////////////////////////////////////

#ifdef AK_WIIU
const AkGameObjectID DemoMotion::GAME_OBJECT_PLAYERS[] = { 200, 300, 400, 500, 600, 700 };
#else
const AkGameObjectID DemoMotion::GAME_OBJECT_PLAYERS[] = { 200, 300, 400, 500 };
#endif


//These defines are there as default for all platforms.
//Some platforms have a different way of specifying the player ID or the controller output type.  Check in ..\[PlatformName]\Plarform.h
#ifndef GET_PLAYER_ID
#define GET_PLAYER_ID(_index) _index
#endif

/////////////////////////////////////////////////////////////////////
// DemoMotion Public Methods
/////////////////////////////////////////////////////////////////////

DemoMotion::DemoMotion( Menu& in_ParentMenu ):MultiplayerPage( in_ParentMenu, "Motion Demo" )
{
	m_szHelp  = "This multiplayer demonstration shows Wwise's capabilities to also emit output "
				"as motion, in this case through a controller's force feedback mechanism.\n\n"
				"Select and press \"Close Door\" to simulate a door closing in the environment. "
				"This should emit a force feedback reaction felt by all players.\n\n"
				"Select and press \"Shoot Gun\" to simulate firing a weapon. This should emit a "
				"force feedback reaction only for the player who shot the weapon.\n\n"
				"NOTE: A player using a keyboard (Windows) should plug in a gamepad to participate.";
}

bool DemoMotion::Init()
{
	// ###### NOTE ######
	// The Motion plugin and Motion device type (eg. Rumble capable gamepads) have previously
	// been registered with the engine. See IntegrationDemo::InitWwise for details.

	// Load the Motion sound bank
	AkBankID bankID; // not used
	if ( AK::SoundEngine::LoadBank( "Motion.bnk", AK_DEFAULT_POOL_ID, bankID ) != AK_Success )
	{
		SetLoadFileErrorMessage( "Motion.bnk" );
		return false;
	}

	// Register the "door" game object and set listeners 0-6 to respond to it
	AK::SoundEngine::RegisterGameObj( GAME_OBJECT_DOOR );
	AK::SoundEngine::SetActiveListeners( GAME_OBJECT_DOOR, 0x0000003F );

	// Setup the motion devices and objects for all the players
	if ( ! SetupMotionDevices() )
	{
		SetErrorMessage( "Could not setup the motion devices" );
		return false;
	}

#ifdef AK_WII_FAMILY
	//On the Wii, the gun sound will also be on the Wiimote
	AK::SoundEngine::Wii::ActivateRemoteSpeakers();
#endif

	// Initialize the page
	return MultiplayerPage::Init();
}

void DemoMotion::Release()
{
	// Unregister the "Door" game object
	AK::SoundEngine::UnregisterGameObj( GAME_OBJECT_DOOR );

#ifdef AK_WII_FAMILY
	//On the Wii, the gun sound will also be on the Wiimote
	AK::SoundEngine::Wii::DeactivateRemoteSpeakers(true);
#endif
	
	// Unregister the players
	for ( int i = 0; i < MAX_PLAYERS; i++ )
	{
		if ( m_bPlayerConnected[i] )
		{
			// Remove the motion device for the player
			AK::MotionEngine::RemovePlayerMotionDevice( (AkUInt8) i, AKCOMPANYID_AUDIOKINETIC, AKMOTIONDEVICEID_RUMBLE );

#ifdef CONTROLLER_OUTPUT	//See Platform.h
			AK::SoundEngine::RemoveSecondaryOutput(GET_PLAYER_ID(i), CONTROLLER_OUTPUT(i));
#endif

			// Unregister the player's game object
			AK::SoundEngine::UnregisterGameObj( GAME_OBJECT_PLAYERS[i] );
		}
	}

	// Unload the soundbank
	AK::SoundEngine::UnloadBank( "Motion.bnk", NULL );

	// Release the Page
	MultiplayerPage::Release();
}


/////////////////////////////////////////////////////////////////////
// DemoMotion Private Methods
/////////////////////////////////////////////////////////////////////

void DemoMotion::InitControls()
{
	ButtonControl* newBtn;

	// Create the controls
	for ( int i = 0; i < MAX_PLAYERS; i++ )
	{
		// Add player i's Door Slide button
		newBtn = new ButtonControl( *this );
		newBtn->SetLabel( "Close Door" );
		newBtn->SetDelegate( (PageMFP)&DemoMotion::DoorSlide_Pressed );
		m_Controls[i].push_back( newBtn );

		// Add player i's Gun Shot button
		newBtn = new ButtonControl( *this );
		newBtn->SetLabel( "Shoot Gun" );
		newBtn->SetDelegate( (PageMFP)&DemoMotion::GunShot_Pressed );
		m_Controls[i].push_back( newBtn );
	}
}

void DemoMotion::OnPlayerConnect( int in_iPlayerIndex )
{
	const UniversalGamepad* gp = m_pParentMenu->Input()->GetGamepad( (AkUInt16) in_iPlayerIndex + 1 ); // UniversalInput is 1-based.

	// Register an object for this player
	AK::SoundEngine::RegisterGameObj( GAME_OBJECT_PLAYERS[in_iPlayerIndex] );

	// Register the player's motion device
#ifdef AK_WII_FAMILY
	if ( AK::MotionEngine::AddPlayerMotionDevice( in_iPlayerIndex, AKCOMPANYID_AUDIOKINETIC, 
		AKMOTIONDEVICEID_RUMBLE, in_iPlayerIndex > 1 ? AK_MOTION_WIIMOTE_DEVICE : AK_MOTION_DRC_DEVICE) != AK_Success )
	{
		SetErrorMessage( "Could not set up the motion device for new player" );
	}
#ifdef AK_WII
	AK::SoundEngine::Wii::SetActiveControllers( GAME_OBJECT_PLAYERS[in_iPlayerIndex], (1 << in_iPlayerIndex) | AK_WII_MAIN_AUDIO_OUTPUT );
#endif //AK_WIIU
#else // AK_WII_FAMILY
	if ( AK::MotionEngine::AddPlayerMotionDevice( (AkUInt8) in_iPlayerIndex, AKCOMPANYID_AUDIOKINETIC, AKMOTIONDEVICEID_RUMBLE, gp->GetExtraParam() ) != AK_Success )
	{
		SetErrorMessage( "Could not set up the motion device for new player" );
	}
#endif	// AK_WII_FAMILY

	// The player i is represented by listener i, make sure he's listening to himself too
#ifdef CONTROLLER_OUTPUT	//See Platform.h
	AK::SoundEngine::AddSecondaryOutput(GET_PLAYER_ID(in_iPlayerIndex), CONTROLLER_OUTPUT(in_iPlayerIndex), 1 << (in_iPlayerIndex+1) );
#endif
	AK::SoundEngine::SetActiveListeners( GAME_OBJECT_PLAYERS[in_iPlayerIndex], 1 << (in_iPlayerIndex+1) );
	AK::MotionEngine::SetPlayerListener( (AkUInt8) in_iPlayerIndex, (AkUInt8) in_iPlayerIndex+1 );

	// The listener i receives audio and motion events
	if ( AK::SoundEngine::SetListenerPipeline( in_iPlayerIndex+1, true, true ) != AK_Success )
	{
		return;
	}
}

void DemoMotion::OnPlayerDisconnect( int in_iPlayerIndex )
{
	// The player's listener no longer receives motion data..
	AK::SoundEngine::SetListenerPipeline( in_iPlayerIndex, true, false );

	// Remove the motion device for the player
	AK::MotionEngine::RemovePlayerMotionDevice( (AkUInt8) in_iPlayerIndex, AKCOMPANYID_AUDIOKINETIC, AKMOTIONDEVICEID_RUMBLE );

#ifdef CONTROLLER_OUTPUT	//See Platform.h
	AK::SoundEngine::RemoveSecondaryOutput(GET_PLAYER_ID(in_iPlayerIndex), CONTROLLER_OUTPUT(in_iPlayerIndex));
#endif


	// Unregister the player's game object
	AK::SoundEngine::UnregisterGameObj( GAME_OBJECT_PLAYERS[in_iPlayerIndex] );
}

bool DemoMotion::SetupMotionDevices()
{
	for ( int i = 0; i < MAX_PLAYERS && m_bPlayerConnected[i]; i++ )
	{
		const UniversalGamepad* gp = m_pParentMenu->Input()->GetGamepad( (AkUInt16) i + 1 ); // UniversalInput is 1-based.

		// Register an object for this player
		AK::SoundEngine::RegisterGameObj( GAME_OBJECT_PLAYERS[i] );

		// Register the player's motion device
#ifdef AK_WIIU
		void* device = i < 2 ? AK_MOTION_DRC_DEVICE : AK_MOTION_WIIMOTE_DEVICE;
		if ( AK::MotionEngine::AddPlayerMotionDevice( i, AKCOMPANYID_AUDIOKINETIC, AKMOTIONDEVICEID_RUMBLE, device) != AK_Success )
		{
			SetErrorMessage( "Could not set up the motion device for new player" );
		}
#else
		if ( AK::MotionEngine::AddPlayerMotionDevice( (AkUInt8) i, AKCOMPANYID_AUDIOKINETIC, 
			AKMOTIONDEVICEID_RUMBLE, gp->GetExtraParam() ) != AK_Success )
		{
			SetErrorMessage( "Could not set up the motion device for new player" );
		}
#endif

		// The player i is represented by listener i, make sure he's listening to himself too
#ifdef CONTROLLER_OUTPUT	//See Platform.h
		AK::SoundEngine::AddSecondaryOutput(GET_PLAYER_ID(i), CONTROLLER_OUTPUT(i), 1 << (i+1) );
#endif
		AK::SoundEngine::SetActiveListeners( GAME_OBJECT_PLAYERS[i], 1 << (i+1) );
		AK::MotionEngine::SetPlayerListener( (AkUInt8) i, (AkUInt8) i+1 );

		// The listener i receives audio and motion events
		if ( AK::SoundEngine::SetListenerPipeline( i+1, true, true ) != AK_Success )
		{
			return false;
		}
	}

	return true;
}

void DemoMotion::DoorSlide_Pressed( void*, ControlEvent* )
{
	AK::SoundEngine::PostEvent( AK::EVENTS::DOORSLIDING, GAME_OBJECT_DOOR );
}

void DemoMotion::GunShot_Pressed( void*, ControlEvent* in_pEvent )
{
	// Fire the gun for the player who shot
	if( in_pEvent->iPlayerIndex >= 1 && in_pEvent->iPlayerIndex <= MAX_PLAYERS )
		AK::SoundEngine::PostEvent( AK::EVENTS::GUNFIRE, GAME_OBJECT_PLAYERS[in_pEvent->iPlayerIndex - 1] );
}
