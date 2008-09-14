/***************************************************************************
 *  Copyright 2007, 2008 Huy Phan  <huyphan@playxiangqi.com>               *
 *                                                                         * 
 *  This file is part of HOXChess.                                         *
 *                                                                         *
 *  HOXChess is free software: you can redistribute it and/or modify       *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation, either version 3 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 *  HOXChess is distributed in the hope that it will be useful,            *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *  You should have received a copy of the GNU General Public License      *
 *  along with HOXChess.  If not, see <http://www.gnu.org/licenses/>.      *
 ***************************************************************************/

/////////////////////////////////////////////////////////////////////////////
// Name:            hoxSite.cpp
// Created:         11/24/2007
//
// Description:     The Site.
/////////////////////////////////////////////////////////////////////////////

#include "hoxSite.h"
#include "MyApp.h"
#include "hoxUtil.h"
#include "hoxMyPlayer.h"
#include "hoxSocketConnection.h"
#include "hoxChesscapeConnection.h"
#include "hoxChesscapePlayer.h"
#include "hoxAIPlayer.h"
#include "MyFrame.h"
#include "MyChild.h"
#include "hoxNetworkAPI.h"
#include "hoxTablesDialog.h"
#include "hoxBoard.h"
#include "hoxSitesUI.h"

// --------------------------------------------------------------------------
// hoxSite
// --------------------------------------------------------------------------


hoxSite::hoxSite( hoxSiteType             type,
                  const hoxServerAddress& address )
        : m_type( type )
        , m_address( address)
        , m_dlgProgress( NULL )
		, m_siteDisconnecting( false )
        , m_player( NULL )
{
}

hoxResult 
hoxSite::CloseTable( hoxTable_SPtr pTable )
{
    hoxSiteManager::GetInstance()->OnTableUIRemoved( this, pTable );
    m_tableMgr.RemoveTable( pTable );
    return hoxRC_OK;
}

hoxPlayer*
hoxSite::GetPlayerById( const wxString& sPlayerId,
                        const int       nScore )
{
    hoxPlayer* player = this->FindPlayer( sPlayerId );
    if ( player == NULL )
    {
	    player = m_playerMgr.CreateDummyPlayer( sPlayerId, nScore );
    }
    wxASSERT( player != NULL );
    return player;
}

unsigned int
hoxSite::GetBoardFeatureFlags() const
{
	unsigned int flags = hoxBoard::hoxBOARD_FEATURE_ALL;

	return flags;
}

void
hoxSite::ShowProgressDialog( bool bShow /* = true */ )
{
    const char* FNAME = __FUNCTION__;

    if ( bShow )
    {
        if ( m_dlgProgress != NULL ) 
        {
            m_dlgProgress->Destroy();  // NOTE: ... see wxWidgets' documentation.
            m_dlgProgress = NULL;
        }

        m_dlgProgress = new wxProgressDialog(
            "Progress dialog",
            "Wait until connnection is established or press [Cancel]",
            100,
            wxGetApp().GetFrame(),  // parent
            wxPD_AUTO_HIDE | wxPD_CAN_ABORT
            );
        m_dlgProgress->SetSize( wxSize(500, 150) );
        m_dlgProgress->Pulse();
    }
    else /* Hide */
    {
        if ( m_dlgProgress != NULL )
        {
            bool wasCanceled = !m_dlgProgress->Pulse();
            m_dlgProgress->Update(100);  // make sure to close the dialog.
            if ( wasCanceled )
            {
                wxLogDebug("%s: Connection has been canceled.", FNAME);
                return;
            }
        }
    }
}

hoxTable_SPtr
hoxSite::CreateNewTableWithGUI( const hoxNetworkTableInfo& tableInfo )
{
    const char* FNAME = __FUNCTION__;
    hoxTable_SPtr pTable;
    const wxString tableId = tableInfo.id;

    /* Create a GUI Frame for the new Table. */
    MyChild* childFrame = wxGetApp().GetFrame()->CreateFrameForTable( tableId );

    /* Create a new table with the newly created Frame. */
    pTable = m_tableMgr.CreateTable( tableId, 
                                     this,
                                     tableInfo.gameType );
	pTable->SetInitialTime( tableInfo.initialTime );
    pTable->SetBlackTime( tableInfo.blackTime );
    pTable->SetRedTime( tableInfo.redTime );
	
	wxLogDebug("%s: Creating a new Board...", FNAME);
    unsigned int boardFeatureFlags = this->GetBoardFeatureFlags();
	hoxBoard* pBoard = new hoxBoard( childFrame, 
		                             PIECES_PATH, 
		                             pTable->GetReferee(),
                                     pTable,
                                     m_player->GetId(),
        					         wxDefaultPosition,
							         childFrame->GetSize(),
                                     boardFeatureFlags );
    pTable->ViewBoard( pBoard );
    
    childFrame->SetTable( pTable );
    childFrame->Show( true );

    hoxSiteManager::GetInstance()->OnTableUICreated( this, pTable );

    return pTable;
}

// --------------------------------------------------------------------------
// hoxLocalSite
// --------------------------------------------------------------------------

hoxLocalPlayer* 
hoxLocalSite::CreateLocalPlayer( const wxString& playerName )
{
	wxCHECK_MSG(m_player == NULL, NULL, "The player has already been set.");

	m_player = m_playerMgr.CreateLocalPlayer( playerName );
	return m_player;
}

unsigned int 
hoxLocalSite::GetCurrentActionFlags() const
{
	unsigned int flags = 0;

    flags |= hoxSITE_ACTION_PRACTICE;

	return flags;
}

void
hoxLocalSite::OnLocalRequest_PRACTICE()
{
    const char* FNAME = __FUNCTION__;
    wxCHECK_RET( m_player != NULL, "Player is NULL" );

    /* Generate new unique IDs for:
     *   (1) This PRACTICE-Table, and
     *   (2) The new AI Player.
     */
    const wxString sTableId = hoxUtil::GenerateRandomString("PRACTICE_");
    const wxString sAIId    = hoxUtil::GenerateRandomString("AI_");

    /* Set the default Table's attributes. */

    hoxNetworkTableInfo tableInfo;

    tableInfo.id = sTableId;
    tableInfo.gameType = hoxGAME_TYPE_PRACTICE;

    const hoxTimeInfo timeInfo( 1500, 300, 20 );
	tableInfo.initialTime = timeInfo;
    tableInfo.redTime     = tableInfo.initialTime;
    tableInfo.blackTime   = tableInfo.initialTime;

    /* Create an "empty" PRACTICE Table. */

    wxLogDebug("%s: Create a PRACTICE Table [%s]...", FNAME, sTableId.c_str());
    hoxTable_SPtr pTable = this->CreateNewTableWithGUI( tableInfo );

    /* Assign Players to the Table.
     *
     * NOTE: Hard-coded player-roles:
     *     + LOCAL player - play RED
     *     + AI player    - play BLACK
     */
    
    hoxResult result;

    result = m_player->JoinTableAs( pTable, hoxCOLOR_RED );
    wxASSERT( result == hoxRC_OK );

    hoxPlayer* pAIPlayer = m_playerMgr.CreateTSITOPlayer( sAIId );
    //hoxPlayer* pAIPlayer = m_playerMgr.CreateAIPlayer( sAIId );
    pAIPlayer->StartConnection(); // TODO: Need to do something about this!

    result = pAIPlayer->JoinTableAs( pTable, hoxCOLOR_BLACK );
    wxASSERT( result == hoxRC_OK );
}

// --------------------------------------------------------------------------
// hoxRemoteSite
// --------------------------------------------------------------------------

hoxRemoteSite::hoxRemoteSite(const hoxServerAddress& address,
                             hoxSiteType             type /*= hoxSITE_TYPE_REMOTE*/)
        : hoxSite( type, address )
{
    const char* FNAME = __FUNCTION__;
    wxLogDebug("%s: ENTER.", FNAME);
}

hoxRemoteSite::~hoxRemoteSite()
{
    const char* FNAME = __FUNCTION__;
    wxLogDebug("%s: ENTER.", FNAME);
}

hoxLocalPlayer* 
hoxRemoteSite::CreateLocalPlayer( const wxString& playerName )
{
	wxCHECK_MSG(m_player == NULL, NULL, "The player has already been set.");

	m_player = m_playerMgr.CreateMyPlayer( playerName );
	return m_player;
}

void 
hoxRemoteSite::OnResponse_LOGIN( const hoxResponse_APtr& response )
{
    this->ShowProgressDialog( false );

    /* If error, then close the Site. */

    if ( response->code != hoxRC_OK )
    {
        wxLogError("The response's code for [%s] is ERROR [%s: %s].", 
            hoxUtil::RequestTypeToString(response->type).c_str(), 
            hoxUtil::ResultToStr(response->code),
            response->content.c_str());
        this->Handle_ShutdownReadyFromPlayer();
    }
}

void 
hoxRemoteSite::OnResponse_LOGOUT( const hoxResponse_APtr& response )
{
    const char* FNAME = __FUNCTION__;

	wxLogDebug("%s: ENTER. (%d: %s).",
        FNAME, response->code, response->content.c_str());
    if ( m_player != NULL )
    {
        m_player->OnClosing_FromSite();
    }
}

void
hoxRemoteSite::OnPlayerLoggedIn( const wxString& sPlayerId,
                                 const int       nPlayerScore )
{
    hoxPlayersUI* playersUI = wxGetApp().GetFrame()->GetSitePlayersUI();
    playersUI->AddPlayer( sPlayerId, nPlayerScore );
}

void
hoxRemoteSite::OnPlayerLoggedOut( const wxString& sPlayerId )
{
    hoxPlayersUI* playersUI = wxGetApp().GetFrame()->GetSitePlayersUI();
    playersUI->RemovePlayer( sPlayerId );
}

hoxResult
hoxRemoteSite::OnPlayerJoined( const wxString&  tableId,
                               const wxString&  playerId,
                               const int        playerScore,
				 			   const hoxColor   requestColor)
{
    const char* FNAME = __FUNCTION__;
    hoxTable_SPtr pTable;
    hoxPlayer*    player = NULL;

	/* Lookup the Table.
     * Make sure that it must be already created.
     */
	pTable = this->FindTable( tableId );
	if ( pTable.get() == NULL )
	{
        wxLogDebug("%s: *** WARN *** Table [%s] NOT exist.", FNAME, tableId.c_str());
		return hoxRC_ERR;
	}

	/* Lookup the Player (create a new "dummy" player if necessary).
     */
    player = this->GetPlayerById( playerId, playerScore );
    wxASSERT( player != NULL );

    /* Attempt to join the table with the requested color.
     */
    return player->JoinTableAs( pTable, requestColor );
}

hoxResult 
hoxRemoteSite::JoinLocalPlayerToTable( const hoxNetworkTableInfo& tableInfo )
{
	const char* FNAME = __FUNCTION__;
    hoxResult      result;
    hoxTable_SPtr  pTable;
    const wxString tableId = tableInfo.id;
    const wxString redId   = tableInfo.redId;
    const wxString blackId = tableInfo.blackId;
    hoxPlayer*     player  = NULL;  // Just a player holder.

	/* Create a table if necessary. */

    pTable = this->FindTable( tableId );
	if ( pTable.get() == NULL )
	{
        wxLogDebug("%s: Create a new Table [%s].", FNAME, tableId.c_str());
        pTable = this->CreateNewTableWithGUI( tableInfo );
	}

	/* Determine which color (or role) my player will have. */
	
	hoxColor myColor = hoxCOLOR_UNKNOWN;

	if      ( redId == m_player->GetId() )   myColor = hoxCOLOR_RED;
	else if ( blackId == m_player->GetId() ) myColor = hoxCOLOR_BLACK;
    else 	                                   myColor = hoxCOLOR_NONE;

	/****************************
	 * Assign players to table.
     ****************************/

    result = m_player->JoinTableAs( pTable, myColor );
    wxCHECK( result == hoxRC_OK, hoxRC_ERR  );

	/* Create additional "dummy" player(s) if required.
     */

    if ( !redId.empty() && pTable->GetRedPlayer() == NULL )
    {
        player = this->GetPlayerById( redId, ::atoi(tableInfo.redScore) );
        wxASSERT( player != NULL );
        result = player->JoinTableAs( pTable, hoxCOLOR_RED );
    }
    if ( !blackId.empty() && pTable->GetBlackPlayer() == NULL )
    {
        player = this->GetPlayerById( blackId, ::atoi(tableInfo.blackScore) );
	    wxASSERT( player != NULL );
        result = player->JoinTableAs( pTable, hoxCOLOR_BLACK );
    }

	return result;
}

hoxResult
hoxRemoteSite::DisplayListOfTables( const hoxNetworkTableInfoList& tableList )
{
    const char* FNAME = __FUNCTION__;

    /* Show tables. */
    MyFrame* frame = wxGetApp().GetFrame();
	const unsigned int actionFlags = this->GetCurrentActionFlags();
    
    hoxTablesDialog tablesDlg( frame, wxID_ANY, "Tables", tableList, actionFlags );
    tablesDlg.ShowModal();
    
    hoxTablesDialog::CommandId selectedCommand = tablesDlg.GetSelectedCommand();
    const wxString selectedId = tablesDlg.GetSelectedId();

    /* Find out which command the use wants to execute... */

    switch( selectedCommand )
    {
        case hoxTablesDialog::COMMAND_ID_JOIN:
        {
            this->OnLocalRequest_JOIN( selectedId );
            break;
        }

        case hoxTablesDialog::COMMAND_ID_NEW:
        {
            this->OnLocalRequest_NEW();
            break;
        }

		case hoxTablesDialog::COMMAND_ID_REFRESH:
        {
            wxLogDebug("%s: Get the latest list of tables...", FNAME);
            if ( hoxRC_OK != this->QueryForNetworkTables() )
            {
                wxLogError("%s: Failed to get the list of tables.", FNAME);
            }
            break;
        }

        default:
            wxLogDebug("%s: No command is selected. Fine.", FNAME);
            break;
    }

    return hoxRC_OK;
}

hoxResult 
hoxRemoteSite::Connect()
{
    const char* FNAME = __FUNCTION__;

    if ( this->IsConnected() )
    {
        wxLogDebug("%s: This site has been connected. END.", FNAME);
        return hoxRC_OK;
    }

    /* Start connecting... */

    this->ShowProgressDialog( true );

    return m_player->ConnectToNetworkServer();
}

hoxResult 
hoxRemoteSite::Disconnect()
{
    const char* FNAME = __FUNCTION__;
	hoxResult result = hoxRC_OK;
    wxLogDebug("%s: ENTER.", FNAME);

	if ( m_siteDisconnecting )
	{
		wxLogDebug("%s: Site [%s] is already being disconnected. END.",
            FNAME, this->GetName().c_str());
		return hoxRC_OK;
	}
	m_siteDisconnecting = true;

	if ( m_player != NULL )
	{
		result = m_player->DisconnectFromNetworkServer();
	}

    return result;
}

hoxResult 
hoxRemoteSite::QueryForNetworkTables()
{
    const char* FNAME = __FUNCTION__;

    if ( ! this->IsConnected() )
    {
        wxLogDebug("%s: This site has NOT been connected.", FNAME);
        return hoxRC_ERR;
    }

    return m_player->QueryForNetworkTables();
}

bool 
hoxRemoteSite::IsConnected() const
{
    return (    m_player != NULL
		     && m_player->GetConnection() != NULL
             && m_player->GetConnection()->IsConnected() );
}

void 
hoxRemoteSite::Handle_ShutdownReadyFromPlayer()
{
    const char* FNAME = __FUNCTION__;
    wxLogDebug("%s: ENTER.", FNAME);

	if ( m_player == NULL )
	{
		wxLogDebug("%s: Player is NULL. Shutdown must have already been processed.", FNAME);
		return;
	}

    /* Close all the Frames of Tables. */
    const hoxTableList& tables = this->GetTables();
    while ( !tables.empty() )
    {
        const wxString sTableId = tables.front()->GetId();
        wxLogDebug("%s: Delete Frame of Table [%s]...", FNAME, sTableId.c_str());
        wxGetApp().GetFrame()->DeleteFrameOfTable( sTableId );
            // NOTE: The call above already delete the Table.
    }

	/* Must set the local player to NULL immediately to handle "re-entrance"
	 * because the DISCONNECT call below can go to sleep...
	 */
	hoxLocalPlayer* localPlayer = m_player;
	m_player = NULL;

	localPlayer->ResetConnection();

    /* Inform the App. */
    wxCommandEvent event( hoxEVT_APP_SITE_CLOSE_READY );
    event.SetEventObject( this );
    wxPostEvent( &(wxGetApp()), event );
}

unsigned int 
hoxRemoteSite::GetCurrentActionFlags() const
{
	unsigned int flags = 0;

    if ( ! this->IsConnected() )
	{
		flags |= hoxSITE_ACTION_CONNECT;
	}
	else
    {
		flags |= hoxSITE_ACTION_DISCONNECT;
		flags |= hoxSITE_ACTION_LIST;
		flags |= hoxSITE_ACTION_NEW;
		flags |= hoxSITE_ACTION_JOIN;
    }

	return flags;
}

void
hoxRemoteSite::OnLocalRequest_JOIN( const wxString& sTableId )
{
    const char* FNAME = __FUNCTION__;
    wxCHECK_RET( m_player != NULL, "Player is NULL" );

    wxLogDebug("%s: Ask the server to allow me to JOIN table = [%s]",
        FNAME, sTableId.c_str());

    if ( hoxRC_OK != m_player->JoinNetworkTable( sTableId ) )
    {
        wxLogError("%s: Failed to JOIN a network table [%s].", FNAME, sTableId.c_str());
    }
}

void
hoxRemoteSite::OnLocalRequest_NEW()
{
    const char* FNAME = __FUNCTION__;
    wxCHECK_RET( m_player != NULL, "Player is NULL" );

    wxLogDebug("%s: Ask the server to open a new table.", FNAME);

    if ( hoxRC_OK != m_player->OpenNewNetworkTable() )
    {
        wxLogError("%s: Failed to open a NEW network table.", FNAME);
    }
}

void
hoxRemoteSite::OnLocalRequest_PRACTICE()
{
    const char* FNAME = __FUNCTION__;
    wxCHECK_RET( m_player != NULL, "Player is NULL" );

    wxLogDebug("%s: Create a new PRACTICE Table...", FNAME);

    /* FIXME: Do nothing for now. */
}

// --------------------------------------------------------------------------
// hoxChesscapeSite
// --------------------------------------------------------------------------

hoxChesscapeSite::hoxChesscapeSite( const hoxServerAddress& address )
        : hoxRemoteSite( address, hoxSITE_TYPE_CHESSCAPE )
{
    const char* FNAME = __FUNCTION__;
    wxLogDebug("%s: ENTER.", FNAME);

    hoxPlayersUI* playersUI = wxGetApp().GetFrame()->GetSitePlayersUI();
    playersUI->SetOwner( this );
    playersUI->Enable();
}

hoxChesscapeSite::~hoxChesscapeSite()
{
    const char* FNAME = __FUNCTION__;
    wxLogDebug("%s: ENTER.", FNAME);

    hoxPlayersUI* playersUI = wxGetApp().GetFrame()->GetSitePlayersUI();
    playersUI->RemoveAllPlayers();
    playersUI->Disable();
    playersUI->SetOwner( NULL );
}

hoxLocalPlayer* 
hoxChesscapeSite::CreateLocalPlayer( const wxString& playerName )
{
	wxCHECK_MSG(m_player == NULL, NULL, "The player has already been set.");

	m_player = m_playerMgr.CreateChesscapePlayer( playerName );
	return m_player;
}

void 
hoxChesscapeSite::OnResponse_LOGIN( const hoxResponse_APtr& response )
{
    this->ShowProgressDialog( false );

    /* If error, then close the Site. */

    if ( response->code != hoxRC_OK )
    {
        wxLogError("The response's code for [%s] is ERROR [%s: %s].", 
            hoxUtil::RequestTypeToString(response->type).c_str(), 
            hoxUtil::ResultToStr(response->code),
            response->content.c_str());
        
        /* NOTE:
         *   Explicitly send a LOGOUT command to close the connection because
         *   Chesscape server does not automatically close the connection
         *   after a login-failure.
         */
        
        if ( m_player != NULL )
        {
            m_player->DisconnectFromNetworkServer();
        }
    }
}

unsigned int 
hoxChesscapeSite::GetCurrentActionFlags() const
{
	unsigned int flags = 0;

	/* Get flags from the parent-class. */
	flags = this->hoxRemoteSite::GetCurrentActionFlags();

    if ( this->IsConnected() )
    {
		/* Chesscape can only support 1-table-at-a-time.
		 * If the Player is actively playing (RED or BLACK),
         * then disable NEW and JOIN actions.
		 */
        wxString sTableId_NOT_USED;
        const hoxColor myRole = m_player->GetFrontRole( sTableId_NOT_USED );
        if ( myRole == hoxCOLOR_RED || myRole == hoxCOLOR_BLACK ) // playing?
	    {
		    flags &= ~hoxSITE_ACTION_NEW;
		    flags &= ~hoxSITE_ACTION_JOIN;
	    }
    }

	return flags;
}

unsigned int
hoxChesscapeSite::GetBoardFeatureFlags() const
{
	unsigned int flags = hoxBoard::hoxBOARD_FEATURE_ALL;

    /* Disable the RESET feature. */
    flags &= ~hoxBoard::hoxBOARD_FEATURE_RESET;

	return flags;
}

void
hoxChesscapeSite::OnPlayersUIEvent( hoxPlayersUI::EventType eventType,
                                    const wxString&         sPlayerId )
{
    const char* FNAME = __FUNCTION__;

    if ( m_player == NULL ) return;

    switch ( eventType )
    {
    case hoxPlayersUI::EVENT_TYPE_INFO:
    {
        const wxString sInfo = "Player: " + sPlayerId;
        ::wxMessageBox( sInfo,
                        _("Player Information"),
                        wxOK | wxICON_INFORMATION,
                        wxGetApp().GetFrame() );
        break;
    }
    case hoxPlayersUI::EVENT_TYPE_INVITE:
    {
        m_player->InvitePlayer( sPlayerId );
        break;
    }
    default:
        wxLogDebug("%s: Unsupported eventType [%d].", FNAME, eventType);
        break;
    }
}

void
hoxChesscapeSite::OnLocalRequest_JOIN( const wxString& sTableId )
{
    const char* FNAME = __FUNCTION__;

	/* Chesscape can only support 1-table-at-a-time.
	 * Thus, we need to close the Table that is observed (if any)
     * before joining another Table.
	 */

    wxString       sObservedTableId;
    const hoxColor myRole = m_player->GetFrontRole( sObservedTableId );

    if ( myRole == hoxCOLOR_RED || myRole == hoxCOLOR_BLACK ) // playing?
    {
        wxLogWarning("Action not allowed: Cannot join a Table while playing at another.");
        return;   // *** Exit immediately!
    }

    if ( myRole == hoxCOLOR_NONE )  // observing?
    {
        wxLogDebug("%s: Close the observed Table [%s] before joining another...",
            FNAME, sObservedTableId.c_str());
        wxGetApp().GetFrame()->DeleteFrameOfTable( sObservedTableId );
            /* NOTE: The call above already delete the Table.
             *       It also triggers the TABLE-CLOSING process.
             */
    }

    this->hoxRemoteSite::OnLocalRequest_JOIN( sTableId );
}

void
hoxChesscapeSite::OnLocalRequest_NEW()
{
    const char* FNAME = __FUNCTION__;

	/* Chesscape can only support 1-table-at-a-time.
	 * Thus, we need to close the Table that is observed (if any)
     * before asking to create a new Table.
	 */

    wxString       sObservedTableId;
    const hoxColor myRole = m_player->GetFrontRole( sObservedTableId );

    if ( myRole == hoxCOLOR_RED || myRole == hoxCOLOR_BLACK ) // playing?
    {
        wxLogWarning("Action not allowed: Cannot create a new Table while playing at another.");
        return;   // *** Exit immediately!
    }

    if ( myRole == hoxCOLOR_NONE )  // observing?
    {
        wxLogDebug("%s: Close the observed Table [%s] before creating a new one...",
            FNAME, sObservedTableId.c_str());
        wxGetApp().GetFrame()->DeleteFrameOfTable( sObservedTableId );
            /* NOTE: The call above already delete the Table.
             *       It also triggers the TABLE-CLOSING process.
             */
    }

    this->hoxRemoteSite::OnLocalRequest_NEW();
}

///////////////////////////////////////////////////////////////////////////////

// --------------------------------------------------------------------------
// hoxSiteManager
// --------------------------------------------------------------------------

/* Define (initialize) the single instance */
hoxSiteManager* 
hoxSiteManager::m_instance = NULL;

/* static */
hoxSiteManager* 
hoxSiteManager::GetInstance()
{
	if ( m_instance == NULL )
		m_instance = new hoxSiteManager();

	return m_instance;
}

/* private */
hoxSiteManager::hoxSiteManager()
        : m_sitesUI( NULL )
{
}

hoxSite* 
hoxSiteManager::CreateSite( hoxSiteType             siteType,
						    const hoxServerAddress& address,
				            const wxString&         userName,
						    const wxString&         password )
{
	const char* FNAME = __FUNCTION__;
	hoxSite*           site = NULL;
    hoxLocalPlayer*    localPlayer = NULL;
    hoxConnection_APtr connection;

	switch ( siteType )
	{
	case hoxSITE_TYPE_LOCAL:
	{
		site = new hoxLocalSite( address );
		localPlayer = site->CreateLocalPlayer( userName );
        connection.reset( new hoxLocalConnection( localPlayer ) );
		break;
	}
	case hoxSITE_TYPE_REMOTE:
	{
		site = new hoxRemoteSite( address );
		localPlayer = site->CreateLocalPlayer( userName );
        connection.reset( new hoxSocketConnection( address, localPlayer ) );
		break;
	}
	case hoxSITE_TYPE_CHESSCAPE:
	{
		site = new hoxChesscapeSite( address );
		localPlayer = site->CreateLocalPlayer( userName );
        connection.reset( new hoxChesscapeConnection( address, localPlayer ) );
		break;
	}
	default:
        wxLogError("%s: Unsupported Site-Type [%d].", FNAME, siteType);
		return NULL;   // *** Exit with error immediately.
	}

	localPlayer->SetPassword( password );
    localPlayer->SetSite( site );
	localPlayer->SetConnection( connection );

    m_sites.push_back( site );
    if ( m_sitesUI != NULL ) m_sitesUI->AddSite( site );
	return site;
}

void
hoxSiteManager::CreateLocalSite()
{
    const hoxServerAddress localAddress("127.0.0.1", 0);
    const wxString         localUserName = "LOCAL_USER";
    const wxString         localPassword;
    this->CreateSite( hoxSITE_TYPE_LOCAL, 
            		  localAddress,
					  localUserName,
					  localPassword );
}

hoxSite*
hoxSiteManager::FindSite( const hoxServerAddress& address ) const
{
    for ( hoxSiteList::const_iterator it = m_sites.begin();
                                      it != m_sites.end(); ++it )
    {
        if ( (*it)->GetAddress() == address )
        {
            return (*it);
        }
    }

	return NULL;
}

void
hoxSiteManager::DeleteSite( hoxSite* site )
{
    const char* FNAME = __FUNCTION__;

    wxCHECK_RET( site != NULL, "The Site must not be NULL." );
    
    wxLogDebug("%s: Deleting site [%s]...", FNAME, site->GetName().c_str());

    if ( m_sitesUI != NULL ) m_sitesUI->RemoveSite( site );
    delete site;
    m_sites.remove( site );
}

void
hoxSiteManager::DeleteLocalSite()
{
    const char* FNAME = __FUNCTION__;

    hoxSite* localSite = NULL;

    for ( hoxSiteList::iterator it = m_sites.begin();
                                it != m_sites.end(); ++it )
    {
		if ( (*it)->GetType() == hoxSITE_TYPE_LOCAL )
        {
            localSite = (*it);
            break;
        }
    }

    if ( localSite != NULL )
    {
        this->DeleteSite( localSite );
    }
}

void 
hoxSiteManager::Close()
{
    const char* FNAME = __FUNCTION__;
    wxLogDebug("%s: ENTER.", FNAME);

    for ( hoxSiteList::iterator it = m_sites.begin();
                                it != m_sites.end(); ++it )
    {
		(*it)->Disconnect();
    }
}

void
hoxSiteManager::OnTableUICreated( hoxSite*      site,
                                  hoxTable_SPtr pTable )
{
    if ( m_sitesUI != NULL ) m_sitesUI->AddTableToSite( site, pTable );
}

void
hoxSiteManager::OnTableUIRemoved( hoxSite*      site,
                                  hoxTable_SPtr pTable )
{
    if ( m_sitesUI != NULL ) m_sitesUI->RemoveTableFromSite( site, pTable );
}

/************************* END OF FILE ***************************************/
