/////////////////////////////////////////////////////////////////////////////
// Name:            hoxMyPlayer.h
// Program's Name:  Huy's Open Xiangqi
// Created:         10/23/2007
//
// Description:     The new "advanced" LOCAL Player.
/////////////////////////////////////////////////////////////////////////////

#ifndef __INCLUDED_HOX_MY_PLAYER_H_
#define __INCLUDED_HOX_MY_PLAYER_H_

#include <wx/wx.h>
#include "hoxLocalPlayer.h"
#include "hoxEnums.h"
#include "hoxTypes.h"

/* Forward declarations */
class hoxSocketConnection;

/** 
 * Connection event-type for responses.
 */
DECLARE_EVENT_TYPE(hoxEVT_CONNECTION_RESPONSE, wxID_ANY)

/**
 * The MY player.
 */

class hoxMyPlayer :  public hoxLocalPlayer
{
public:
    hoxMyPlayer(); // DUMMY default constructor required for event handler.
    hoxMyPlayer( const wxString& name,
                 hoxPlayerType   type,
                 int             score );

    virtual ~hoxMyPlayer();

    /*******************************************
     * Override the parent's API
     *******************************************/

protected:
    virtual const wxString BuildRequestContent( const wxString& commandStr );

    virtual hoxConnection* CreateNewConnection( const wxString& sHostname, 
                                                int             nPort );

    /*******************************************
     * Override the parent's event-handler API
     *******************************************/

public:

    /*******************************
     * MY-specific Network API
     *******************************/

    /*******************************
     * Socket-event handlers
     *******************************/

    void OnIncomingNetworkData( wxSocketEvent& event );

    /*******************************
     * Other API
     *******************************/

    hoxResult StartListenForMoves();

private:
    void _StartConnection();

private:

    DECLARE_CLASS(hoxMyPlayer)
    DECLARE_EVENT_TABLE()
};


#endif /* __INCLUDED_HOX_MY_PLAYER_H_ */