/////////////////////////////////////////////////////////////////////////////
// Name:            MyFrame.h
// Program's Name:  Huy's Open Xiangqi
// Created:         10/02/2007
//
// Description:     The main Frame of the App.
/////////////////////////////////////////////////////////////////////////////

#ifndef __INCLUDED_MY_FRAME_H_
#define __INCLUDED_MY_FRAME_H_

#include <wx/wx.h>
#include <wx/laywin.h>   // wxSashLayoutWindow
#include <wx/progdlg.h>
#include <list>

/* Forward declarations */
class hoxNetworkTableInfo;
class hoxTable;
class MyChild;
class hoxLocalPlayer;

// menu items ids
enum
{
    MDI_QUIT = wxID_EXIT,
    MDI_NEW_TABLE = 101,
    MDI_CLOSE_TABLE,

    MDI_OPEN_SERVER,    // Open server
    MDI_CONNECT_SERVER, // Connect to server
    MDI_DISCONNECT_SERVER, // Disconnect from server

    MDI_CONNECT_HTTP_SERVER,
    MDI_SHOW_LOG_WINDOW,

    MDI_TOGGLE,   // toggle view
    MDI_CHILD_QUIT,
    MDI_ABOUT = wxID_ABOUT
};

/** 
 * Log events
 */
DECLARE_EVENT_TYPE(hoxEVT_FRAME_LOG_MSG, wxID_ANY)

/*
 * Define main (MDI) frame
 */
class MyFrame : public wxMDIParentFrame
{
    typedef std::list<MyChild*> MyChildList;

public:
    MyFrame( wxWindow*          parent, 
             const wxWindowID   id, 
             const wxString&    title,
             const wxPoint&     pos, 
             const wxSize&      size, 
             const long         style );

    ~MyFrame();

    // -----
    void SetupMenu();
    void SetupStatusBar();

    void InitToolBar(wxToolBar* toolBar);

    void OnSize(wxSizeEvent& event);
    void OnSashDrag(wxSashEvent& event);
    void OnAbout(wxCommandEvent& event);
    void OnNewTable(wxCommandEvent& event);
    void OnCloseTable(wxCommandEvent& event);
    void OnUpdateCloseTable(wxUpdateUIEvent& event);

    void OnOpenServer(wxCommandEvent& event);
    void OnConnectServer(wxCommandEvent& event);
    void OnDisconnectServer(wxCommandEvent& event);

    void OnConnectHTTPServer(wxCommandEvent& event);
    void OnShowLogWindow(wxCommandEvent& event);
    void OnUpdateLogWindow(wxUpdateUIEvent& event);

    void OnQuit(wxCommandEvent& event);
    void OnClose(wxCloseEvent& event);

    /**
     * Invoke by the Child window when it is being closed.
     * NOTE: This *special* API is used by the Child to ask the Parent
     *       for the permission to close.
     *       !!! BE CAREFUL with recursive calls. !!!
     */
    bool OnChildClose(MyChild* child, hoxTable* table);

    void OnHTTPResponse(wxCommandEvent& event);
    void OnMYResponse(wxCommandEvent& event);
    void DoJoinExistingTable(const hoxNetworkTableInfo& tableInfo, hoxLocalPlayer* localPlayer);
    void DoJoinNewTable(const wxString& tableId, hoxLocalPlayer* localPlayer);
    void OnFrameLogMsgEvent( wxCommandEvent &event );

private:
    void _Handle_OnResponse( wxCommandEvent& event, hoxLocalPlayer* localPlayer );

    void _OnResponse_Leave( const wxString& responseStr );
    void _OnResponse_Connect( const wxString& responseStr, hoxLocalPlayer* localPlayer );
    void _OnResponse_List( const wxString& responseStr, hoxLocalPlayer* localPlayer );
    void _OnResponse_Join( const wxString& responseStr, hoxLocalPlayer* localPlayer );
    void _OnResponse_New( const wxString& responseStr, hoxLocalPlayer* localPlayer );

    hoxTable* _CreateNewTable( const wxString& tableId );

public: /* Static API */
    static wxMenuBar* Create_Menu_Bar( bool hasTable = false );

private:
    // Logging.  
    wxSashLayoutWindow* m_logWindow; // To contain the log-text below.
    wxTextCtrl*         m_logText;   // Log window for debugging purpose.

    // the progress dialog which we show while worker thread is running
    wxProgressDialog*   m_dlgProgress;

    int                 m_nChildren;   // The number of child-frames.
    MyChildList         m_children;

    DECLARE_EVENT_TABLE()
};


#endif  /* __INCLUDED_MY_FRAME_H_ */