/*$Id$*/
/*
    derot is the controller code for the Arduino MEGA2560
    Copyright (C) 2015  C.Y. Tan
    Contact: cytan299@yahoo.com

    This file is part of derot

    derot is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    derot is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with derot.  If not, see <http://www.gnu.org/licenses/>.

*/
/* operating system header files (use <> for make depend) */
#include <Adafruit_CC3000.h>
#include <ccspi.h>
#include <SPI.h>
#include <Arduino.h>
#include <stdlib.h>
#include <math.h>
#include "utility/debug.h"

/* general system header files (use "" for make depend) */

/* local include files (use "") */
#include "BaseServer.h"
#include "UserIO.h"

#define MECHANICAL_STEPSIZE	0.05970731707 // deg/step


/**********************************************************************
NAME

        BaseServer - Base class for both TCPServer and SerialServer so
		 that common functions used by both these classes can
		 be consolidated.

SYNOPSIS
	See BaseServer.hpp

                                                
PROTECTED FUNCTIONS

PRIVATE FUNCTIONS

LOCAL TYPES AND CLASSES

AUTHOR

        C.Y. Tan

SEE ALSO

REVISION
	$Revision$

**********************************************************************/


BaseServer::BaseServer(UserIO* userio, DeRotator* derotator)
{
  _userio = userio;
  _derotator = derotator;
}

BaseServer::~BaseServer()
{
}

int BaseServer::Connect()
{
  
  return 0;
}

int BaseServer::Disconnect()
{
  return 0;
}

int BaseServer::ServiceRequests(RequestPacket* const rq,
				ReplyPacket* const rp,
				StatusPacket* const sp)
{
  extern menu main_menu;
  extern menu control_menu;
  extern menu setup_menu;
  

  // do derotator control commands
  if((rq->_command >= 10) && (rq->_command <20)){ 
    main_menu.activeNode=&control_menu;
    // always return OK for these commands
    rp->_reply = REPLY_OK;
    
    switch(rq->_command){
      case DEROTATOR_START:
	_userio->SetInitDeRotatorFlag();
      break;
      case DEROTATOR_STOP:
	_userio->SetStopDeRotatorFlag();
	_derotator->Stop();
      break;
      case DEROTATOR_GOTO_HALL_HOME:
	_userio->SetGotoHallHomeFlag();
      break;
      case DEROTATOR_GOTO_USER_HOME:
	_userio->SetGotoUserHomeFlag();
      break;	    
    }
  }

  char buf[64];  
  // do setup  commands
  if((rq->_command >= 20) && (rq->_command <30)){ 
    main_menu.activeNode=&setup_menu;
    
    // always return OK for these commands
    rp->_reply = REPLY_OK;
    
    switch(rq->_command){
      case SETUP_SET_USER_HOME:
	_derotator->SetUserHome(static_cast<long>(rq->_fvalue[0]/MECHANICAL_STEPSIZE)); // steps
	sprintf(buf, ": %d", _derotator->GetUserHome()); // already in steps	
        _userio->Print("Setting Home to",
		       buf,
		       1000);
	_userio->ForceLCDPrintMenu(setup_menu, true);	    
      break;
      
      case SETUP_MAX_CW:
	_derotator->SetMaxCW(static_cast<long>(rq->_fvalue[0]/MECHANICAL_STEPSIZE)); // steps
	sprintf(buf, ": %d", _derotator->GetMaxCW()); // already in steps	
        _userio->Print("Setting CW to",
		       buf,
		       1000);	

	_userio->ForceLCDPrintMenu(setup_menu, true);	    
      break;
      case SETUP_MAX_CCW:
	_derotator->SetMaxCCW(static_cast<long>(rq->_fvalue[0]/MECHANICAL_STEPSIZE)); // steps
	sprintf(buf, ": %d", _derotator->GetMaxCCW()); // already in steps	
        _userio->Print("Setting CCW to",
		       buf,
		       1000);	

	_userio->ForceLCDPrintMenu(setup_menu, true);	    
      break;
      
      case SETUP_ENABLE_LIMITS:
	_userio->_is_enable_limits = rq->_ivalue != 0; // is enable limits if nonzero value is found
      break;
      
      case SETUP_IS_CLOCKWISE:
	_userio->_is_clockwise = rq->_ivalue != 0; // is clockwise if a nonzero value is found
	_userio->ForceLCDPrintMenu(setup_menu, true);
      break;
      
      case SETUP_SAVE_SETTINGS:
	_userio->SaveSettings();	    
      break;
      
      case SETUP_LOAD_SETTINGS:;
	_userio->LoadSavedSettings();
      break;
      
      case SETUP_DEF_SETTINGS:
	_userio->LoadDefaultSettings();
      break;
    }
  }

  // do other commands
  if(rq->_command >= 100){

    // rp->_reply not necessarily ok with these commands
    switch(rq->_command){
      case CMD_GET_ALTAZ_ZETA:
	double alt, az;
	_derotator->GetAltAz(&alt, &az);
	rp->_reply = _userio->_derotator_continue_status;
	rp->_fvalue[0] = alt;
	rp->_fvalue[1] = az;
	rp->_fvalue[2] = _derotator->GetAccumulatedAngle();
	rp->_fvalue[3] = _derotator->GetAngle();
      break;
      case CMD_GET_THETA:
	rp->_reply = REPLY_OK;	
	double theta;
	theta = _derotator->GetAngle();
	rp->_fvalue[0] = theta;
      break;
      case CMD_GOTO_THETA:
	rp->_reply = REPLY_OK;		
	theta = rq->_fvalue[0];
	_derotator->StartGoingToUserAngle(theta);
	_userio->_is_goto_user_angle = true;
      break;
      case CMD_QUERY_STATE:
	// send back the state of the derotator
	// whether it is rotating already
	if ((_userio->_is_start_derotator == false) &&
	    (_userio->_is_stop_derotator == false)){
	  sp->_reply = REPLY_IS_DEROTATING;
	}
	else {
	  sp->_reply = REPLY_OK;
	}
	sp->_is_clockwise_correction = _derotator->GetCorrectionDirection()? 1:0;
	sp->_is_enable_limits = _derotator->IsEnableLimits()? 1:0;

	sp->_home_pos = _derotator->GetUserHome();
	sp->_max_cw = _derotator->GetMaxCW();
	sp->_max_ccw = _derotator->GetMaxCCW();
	sp->_angle = _derotator->GetAngle();
	sp->_accumulated_angle = _derotator->GetAccumulatedAngle();

	// The filling in of WLAN_security and WLAN_security is
	// done in TCPServer.cpp
	uint8_t security;
	_userio->GetSSID(static_cast<char*>(sp->_WLAN_ssid), &security);
	sp->_WLAN_security = security;
	sp->_WLAN_password[0] = '\0'; // never send back the password

	sp->_omega = _derotator->GetOmega();

      break;

      case CMD_GET_USER_HOME_POS:
	rp->_reply = REPLY_OK;		
	rp->_fvalue[0] = _derotator->GetUserHome();
      break;

      case CMD_GET_MAX_CW_POS:
	rp->_reply = REPLY_OK;		
        rp->_fvalue[0] = _derotator->GetMaxCW();
      break;

      case CMD_GET_MAX_CCW_POS:
	rp->_reply = REPLY_OK;		
        rp->_fvalue[0] = _derotator->GetMaxCCW();
      break;

      case CMD_SET_WLAN_SSID:
	rp->_reply = REPLY_OK;
	_userio->SetSSID(rq->_buf);
      break;
	
      case CMD_SET_WLAN_PASS:
	rp->_reply = REPLY_OK;
	_userio->SetPass(rq->_buf);
      break;
	
      case CMD_SET_WLAN_SECURITY:
	rp->_reply = REPLY_OK;
	_userio->SetSecurity(rq->_ivalue);	
      break;

    case CMD_SET_OMEGA_VALUE:
      rp->_reply = REPLY_OK;
      double omega;
      omega = rq->_fvalue[0];
      _derotator->SetOmega(omega);
      break;

    case CMD_GET_OMEGA_VALUE:
	rp->_reply = REPLY_OK;		
        rp->_fvalue[0] = _derotator->GetOmega();      
      break;      
    }
  }
  return 0;  
}
