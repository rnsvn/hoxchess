/***************************************************************************
 *  Copyright 2007-2009 Huy Phan  <huyphan@playxiangqi.com>                *
 *                      Bharatendra Boddu (bharathendra at yahoo dot com)  *
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
// Name:            hoxCoreBoard.cpp
// Created:         10/05/2007
//
// Description:     The "core" Board.
/////////////////////////////////////////////////////////////////////////////

#include "hoxCoreBoard.h"
#include "hoxUtil.h"

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------
#define hoxBOARD_WORKSPACE_BRUSH   wxLIGHT_GREY_BRUSH

enum Constants
{
    // Dragging modes
    DRAG_MODE_NONE,
    DRAG_MODE_START,
    DRAG_MODE_DRAGGING,

    NUM_HORIZON_CELL  = 8,  // Do not change the value!!!
    NUM_VERTICAL_CELL = 9   // Do not change the value!!!
};

IMPLEMENT_DYNAMIC_CLASS(hoxCoreBoard, wxPanel)

/*
 * The event-table.
 */
BEGIN_EVENT_TABLE(hoxCoreBoard, wxPanel)
    EVT_PAINT            (hoxCoreBoard::OnPaint)
    EVT_MOUSE_EVENTS     (hoxCoreBoard::OnMouseEvent)
    EVT_ERASE_BACKGROUND (hoxCoreBoard::OnEraseBackground)
    EVT_IDLE             (hoxCoreBoard::OnIdle)
    EVT_SIZE(hoxCoreBoard::OnSize)

    // NOTE: We have to handle this. Otherwise, the program crashes.
    //   @see http://www.nabble.com/EVT_MOUSE_CAPTURE_LOST-t2872494.html
    EVT_MOUSE_CAPTURE_LOST (hoxCoreBoard::OnMouseCaptureLost)
END_EVENT_TABLE()

// ---------------------------------------------------------------------------
// hoxCoreBoard
// ---------------------------------------------------------------------------

/**
 * NOTE: wx_FULL_REPAINT_ON_RESIZE is used to have entire window included 
 *       in the update region.
 */
hoxCoreBoard::hoxCoreBoard( wxWindow*        parent, 
                            hoxIReferee_SPtr referee,
                            const wxString&  sBgImage,
                            const wxString&  piecesPath,
                            wxColor          bgColor,
                            wxColor          fgColor,
                            const wxPoint&   pos  /* = wxDefaultPosition */, 
                            const wxSize&    size /* = wxDefaultSize*/ )
        : wxPanel( parent, wxID_ANY, pos, size,
                   wxFULL_REPAINT_ON_RESIZE )
        , m_sImage( sBgImage )
        , m_background( NULL )
        , m_piecesPath( piecesPath )
        , m_bViewInverted( false )  // Normal view: RED is at bottom of the screen
        , m_referee( referee )
        , m_owner( NULL )
        , m_moveMode( hoxMOVE_MODE_DRAG_N_DROP )
        , m_dragMode( DRAG_MODE_NONE )
        , m_draggedPiece( NULL )
        , m_dragImage( NULL )
        , m_latestPiece( NULL )
        , m_historyIndex( HISTORY_INDEX_END )
        , m_isGameOver( false )
{
    wxASSERT_MSG(m_referee, "A Referee must be set");

    m_nBestHeightAdjustment = 0; // HACK!

    if ( m_sImage.empty() ) m_background = new hoxCustomBackground();
    else  m_background = new hoxImageBackground( m_sImage );
    m_background->OnBgColor( bgColor );
    m_background->OnFgColor( fgColor );
}

hoxCoreBoard::~hoxCoreBoard()
{
    _ClearPieces();
    delete m_dragImage;
    delete m_background;
}

void
hoxCoreBoard::SetBackgroundImage( const wxString& sImage )
{
    if ( m_sImage == sImage ) return;

    m_sImage = sImage;
    delete m_background;
    if ( m_sImage.empty() ) m_background = new hoxCustomBackground();
    else  m_background = new hoxImageBackground( m_sImage );
}

void
hoxCoreBoard::SetBgColor( wxColor color )
{
    m_background->OnBgColor( color );
}

void
hoxCoreBoard::SetFgColor( wxColor color )
{
    m_background->OnFgColor( color );
}

void
hoxCoreBoard::Repaint()
{
    wxClientDC dc(this);
    m_background->OnPaint( dc );
    _DrawAllPieces( dc );
}

void 
hoxCoreBoard::OnPaint( wxPaintEvent& event )
{
    wxPaintDC dc(this);
    PrepareDC(dc);   // ... for drawing a scrolled image

    _DoPaint(dc);
}

void 
hoxCoreBoard::_ErasePiece( hoxPiece* piece )
{
    wxClientDC dc(this);
    PrepareDC(dc);   // ... for drawing a scrolled image

    /* Erase the piece by repainting the piece's area with
     * the Board's background.
     */
    
    const wxRect rect( _GetPieceRect(piece) );

#ifdef __WXMAC__
    // Extra code added to remove the 'highlight'.
    dc.SetBrush( *wxTRANSPARENT_BRUSH );
    dc.DrawRectangle( rect );
#endif

    dc.SetClippingRegion( rect );
    {
        m_background->OnPaint(dc);
    }
    dc.DestroyClippingRegion();
}

void 
hoxCoreBoard::_ClearPieces()
{
    hoxPiece* piece = NULL;
    while ( ! m_pieces.empty() )
    {
        piece = m_pieces.front();
        m_pieces.pop_front();
        delete piece;
    }
}

void
hoxCoreBoard::_ReloadAllPieceBitmaps()
{
    // Reload the bitmaps of all Pieces, including inactive + hidden pieces.
    for (hoxPieceList::const_iterator it = m_pieces.begin();
                                      it != m_pieces.end(); ++it)
    {
        (*it)->LoadBitmap( m_piecesPath );
    }
}

hoxPiece* 
hoxCoreBoard::_FindPiece( const wxPoint& point ) const
{
    // Find only active piece
    for (hoxPieceList::const_iterator it = m_pieces.begin();
                                      it != m_pieces.end(); ++it)
    {
        hoxPiece* piece = *it;
        if ( piece->IsActive() && _PieceHitTest(piece, point))
        {
            return piece;
        }
    }

    return NULL;
}

void 
hoxCoreBoard::_DoPaint( wxDC& dc )
{
#ifdef __WXGTK__
    m_background->OnPaint( dc );
#endif
    _DrawAllPieces(dc);
}

void 
hoxCoreBoard::OnEraseBackground( wxEraseEvent& event )
{
    // *** Paint the entire working space with a background's color.
    if ( event.GetDC() )
    {
        m_background->OnPaint( *event.GetDC() );
    }
    else
    {
        wxClientDC dc(this);
        m_background->OnPaint( dc );
    }
}

void 
hoxCoreBoard::OnIdle( wxIdleEvent& event )
{
    // Do nothing for now.
}

void 
hoxCoreBoard::OnSize( wxSizeEvent& event )
{
    //wxLogDebug("%s: (%d, %d)", __FUNCTION__, event.GetSize().x, event.GetSize().y);
    m_background->OnResize( event.GetSize() );
    wxClientDC dc(this);
    m_background->OnPaint( dc );
#ifdef __WXGTK__
    _DrawAllPieces(dc);
#endif
}

void
hoxCoreBoard::SetPiecesPath(const wxString& piecesPath)
{
    m_piecesPath = piecesPath;
    _ReloadAllPieceBitmaps();
}

void 
hoxCoreBoard::LoadPiecesAndStatus()
{
    _ClearPieces();  // Clear old pieces.

    hoxGameState gameState;
    m_referee->GetGameState( gameState );

    for ( hoxPieceInfoList::const_iterator it = gameState.pieceList.begin();
                                           it != gameState.pieceList.end(); ++it )
    {
        hoxPiece* piece = new hoxPiece(*it, m_piecesPath);
        m_pieces.push_back( piece );
    }

    /* Display the game-status. */
    if ( hoxIReferee::IsGameOverStatus( gameState.gameStatus ) )
    {
        SetGameOver( true );
    }
}

void 
hoxCoreBoard::ResetBoard()
{
    /* If the Board is in GAME-REVIEW mode, leave it. */
    this->DoGameReview_BEGIN();
    m_historyMoves.clear();
    m_historyIndex = HISTORY_INDEX_END;

    /* Initialize other stated-info. */
    m_latestPiece = NULL;
    m_isGameOver = false;
    m_background->SetGameOver( false );

    /* Reload the Pieces according to the Referee. */
    this->LoadPiecesAndStatus();

     wxClientDC dc(this);
     m_background->OnPaint( dc );
    _DrawAllPieces( dc );
}

void 
hoxCoreBoard::_DrawAllPieces( wxDC& dc )
{
    for (hoxPieceList::const_iterator it = m_pieces.begin(); 
                                      it != m_pieces.end(); ++it)
    {
        const hoxPiece* piece = *it;
        if ( piece->IsActive() && piece->IsShown()) 
        {
            _DrawPieceWithDC( dc, piece );
        }
    }
}

void
hoxCoreBoard::_DrawAndHighlightPiece( hoxPiece*  piece,
                                      const bool bRefresh /* = true */ )
{
    // Un-highlight the "old" piece.
    if ( m_latestPiece != NULL )
    {
        m_latestPiece->SetLatest(false);

        /* Re-draw the "old" piece to undo the highlight.
         * TODO: Is there a better way?
         */
        if ( bRefresh )
        {
            _ErasePiece( m_latestPiece );
            _DrawPiece( m_latestPiece );
        }
    }

    piece->SetLatest( true );
    m_latestPiece = piece;
    if ( bRefresh ) _DrawPiece( piece );
}

/**
 * Draw a given piece using THIS window's Device-Context.
 */
void 
hoxCoreBoard::_DrawPiece( const hoxPiece* piece )
{
    wxClientDC dc(this);
    PrepareDC(dc);   // ... for drawing a scrolled image

    _DrawPieceWithDC( dc, piece );
}

void 
hoxCoreBoard::_DrawPieceWithDC( wxDC&           dc, 
                                const hoxPiece* piece )
{
    const wxPoint pos = _GetPieceLocation( piece );
    wxBitmap& bitmap = const_cast<wxBitmap&>( piece->GetBitmap() );
    if ( ! bitmap.Ok() )
      return;

    hoxUtil::DrawBitmapOnDC( dc, bitmap, pos.x, pos.y );

    /* Highlight the piece if it is the latest piece that moves. */
    if ( piece->IsLatest() )
    {
        _DrawHighlight( dc, wxRect(pos, bitmap.GetSize()) );
    }
}

wxRect 
hoxCoreBoard::_GetPieceRect( const hoxPiece* piece ) const
{
    const wxPoint pos = _GetPieceLocation(piece);
    const wxBitmap& bitmap = piece->GetBitmap();

    return wxRect( pos.x, pos.y, bitmap.GetWidth(), bitmap.GetHeight() ); 
}

bool 
hoxCoreBoard::_PieceHitTest( const hoxPiece* piece, 
                             const wxPoint&  pt ) const
{
    const wxRect& rect = _GetPieceRect(piece);
    return rect.Contains(pt.x, pt.y);
}

hoxPosition 
hoxCoreBoard::_PointToPosition( const hoxPiece* piece, 
                                const wxPoint&  p ) const
{
    const wxCoord borderX = m_background->BorderX();
    const wxCoord borderY = m_background->BorderY();
    const wxCoord cellS = m_background->CellS();

    const wxBitmap& bitmap = piece->GetBitmap();
    hoxPosition pos;

    // We will work on the center.
    wxPoint point(p.x + bitmap.GetWidth()/2, p.y + bitmap.GetHeight()/2);

    /* Get the 4 surrounding positions.
    *
    *    1 ------------ 2
    *    |      ^       |
    *    |      |       |
    *    |  <-- X -->   |
    *    |      |       |
    *    |      V       |
    *    4 ------------ 3
    *
    */

    wxPoint p1, p2, p3, p4;

    p1.x = point.x - ((point.x - borderX) % cellS);
    p1.y = point.y - ((point.y - borderY) % cellS);

    p2.x = p1.x + cellS; 
    p2.y = p1.y; 

    p3.x = p2.x; 
    p3.y = p2.y + cellS; 

    p4.x = p1.x; 
    p4.y = p3.y; 

    wxSize tolerance(cellS / 3, cellS / 3);
    wxRect  r1(p1 - tolerance, tolerance*2);
	wxRect  r2(p2 - tolerance, tolerance*2);
	wxRect  r3(p3 - tolerance, tolerance*2);
	wxRect  r4(p4 - tolerance, tolerance*2);

    if ( r1.Contains(point) ) {
        pos.x = (p1.x - borderX) / cellS;
        pos.y = (p1.y - borderY) / cellS;
    }
    else if ( r2.Contains(point) ) {
        pos.x = (p2.x - borderX) / cellS;
        pos.y = (p2.y - borderY) / cellS;
    }
    else if ( r3.Contains(point) ) {
        pos.x = (p3.x - borderX) / cellS;
        pos.y = (p3.y - borderY) / cellS;
    }
    else if ( r4.Contains(point) ) {
        pos.x = (p4.x - borderX) / cellS;
        pos.y = (p4.y - borderY) / cellS;
    }

    return pos;
}

void
hoxCoreBoard::_MovePieceToPoint( hoxPiece*      piece, 
                                 const wxPoint& point )
{
    hoxPosition newPos = _PointToPosition(piece, point);
    if ( newPos.IsValid() && m_bViewInverted )
    {
        newPos.x = 8 - newPos.x;
        newPos.y = 9 - newPos.y;
    }

    hoxMove           move;   // Make a new Move
    hoxGameStatus     gameStatus = hoxGAME_STATUS_UNKNOWN;

    /* Ask the referee to check if the move is valid. */

    move.piece       = piece->GetInfo();
    move.newPosition = newPos;

    if ( ! m_referee->ValidateMove( move, gameStatus ) )
    {
        _PrintDebug( _("Move is not valid.") );
        this->Refresh();
        return;
    }

    if ( hoxIReferee::IsGameOverStatus( gameStatus ) )
	{
        SetGameOver( true );
	}

    /* NOTE: Need to the following check. 
     * Otherwise, the piece would disappear.
     */
    if ( piece->GetPosition() != newPos )
    {
        _MovePieceTo(piece, newPos);
    }

    /* Keep track the list of all Moves. */
    _RecordMove( move );

    /* Inform the Board's Owner of the new Move. */
    if ( m_owner != NULL )
	{
        m_owner->OnBoardMove( move, gameStatus );
	}
}

bool 
hoxCoreBoard::_CanPieceMoveNext( hoxPiece* piece ) const
{
    if (   m_owner != NULL 
        && !m_owner->OnBoardAskMovePermission( piece->GetInfo() ) )
	{
        return false;
    }

    if (   m_isGameOver
        || _IsBoardInReviewMode() )
    {
        return false;
    }

    return true;
}

bool 
hoxCoreBoard::_IsBoardInReviewMode() const
{
    return ( m_historyIndex != HISTORY_INDEX_END );
}

bool 
hoxCoreBoard::DoMove( hoxMove&   move,
                      const bool bRefresh /* = true */ )
{
    hoxGameStatus gameStatus = hoxGAME_STATUS_UNKNOWN;
    if ( ! m_referee->ValidateMove( move, gameStatus ) )
    {
        _PrintDebug( wxString::Format("%s: Move is not valid.", __FUNCTION__) );
        return false;
    }

    if ( hoxIReferee::IsGameOverStatus( gameStatus ) )
    {
        SetGameOver( true );
    }

    _RecordMove( move ); // Keep track the list of all Moves.

    /* Do not update the Pieces on Board if we are in the review mode. */
    if ( _IsBoardInReviewMode() )
    {
        return true;
    }

    /* Ask the core Board to perform the Move. */

    hoxPiece* piece = _FindPieceAt( move.piece.position );
    wxCHECK_MSG( piece != NULL, false, "Piece is not found." );

    if ( ! _MovePieceTo( piece, move.newPosition, true, bRefresh ) )
    {
        _PrintDebug( wxString::Format("%s: Piece could not be moved.", __FUNCTION__) );
        return false;
    }

    return true;
}

/**
 * Set a piece's position without validation 
 * (without going through the referee).
 */
bool 
hoxCoreBoard::_MovePieceTo( hoxPiece*          piece, 
                            const hoxPosition& newPosition,
                            bool               hightlight /* = true */,
                            const bool         bRefresh /* = true */ )
{
    if ( ! newPosition.IsValid() )
        return false;

    _FindAndCapturePieceAt( newPosition, bRefresh );

    if ( bRefresh && piece->IsShown() )
        _ErasePiece( piece );

    piece->SetPosition( newPosition ); // ... without validation.

    if ( hightlight )   _DrawAndHighlightPiece( piece, bRefresh );
    else if( bRefresh ) _DrawPiece( piece );

    return true;
}

bool
hoxCoreBoard::DoGameReview_BEGIN()
{
    while ( this->DoGameReview_PREV(false /* bRefresh */) ) { }
    this->Refresh();
    return true;
}

bool 
hoxCoreBoard::DoGameReview_PREV( const bool bRefresh /* = true */ )
{
    if ( m_historyMoves.empty() )
    {
        //wxLogDebug("%s: No Moves made yet.", __FUNCTION__);
        return false;
    }

    if ( m_historyIndex == HISTORY_INDEX_END ) // at the END mark?
    {
        // Get the latest move.
        m_historyIndex = (int) (m_historyMoves.size() - 1);
    }
    else if ( m_historyIndex == HISTORY_INDEX_BEGIN )
    {
        //wxLogDebug("%s: The index is already at BEGIN. Do nothing. END.", __FUNCTION__);
        return false;
    }

    wxCHECK_MSG( m_historyIndex >= 0 && m_historyIndex < (int)m_historyMoves.size(), 
                 false, "Invalid index." );
    const hoxMove move = m_historyMoves[m_historyIndex];

    /* Move the piece back from NEW -> ORIGINAL position. */

    hoxPiece* piece = _FindPieceAt( move.newPosition );
    wxCHECK_MSG(piece, false, "No piece found at NEW position.");

    piece->SetLatest( false );
    if ( ! _MovePieceTo( piece, move.piece.position, false /* no highlight */, bRefresh ) )
    {
        wxLogDebug("%s: Failed to move Piece back to the ORIGINAL position.", __FUNCTION__);
        return false;
    }

    /* Putback the captured piece, if any. */

    if ( move.IsAPieceCaptured() )
    {
        hoxPiece* capturedPiece = _FindPieceAt( move.capturedPiece.position,
                                                true /* including Inactive pieces */ );
        wxCHECK_MSG(capturedPiece != NULL, false, "Unable to get the captured Piece.");
        wxCHECK_MSG(!capturedPiece->IsActive(), false, "Piece is already Active.");
        capturedPiece->SetActive( true );
        if ( bRefresh ) _DrawPiece( capturedPiece );
    }

    /* Highlight the Piece (if any) of the "next-PREV" Move. */

    --m_historyIndex;
    if ( m_historyIndex >= 0 )
    {
        const hoxMove prevMove = m_historyMoves[m_historyIndex];
        hoxPiece* prevPiece = _FindPieceAt( prevMove.newPosition );
        wxCHECK_MSG(prevPiece, false, "No next-PREV Piece found.");
        _DrawAndHighlightPiece( prevPiece, bRefresh );
    }

    return true;
}

bool 
hoxCoreBoard::DoGameReview_NEXT( const bool bRefresh /* = true */ )
{
    if ( m_historyMoves.empty() )
    {
        //wxLogDebug("%s: No Moves made yet.", __FUNCTION__);
        return false;
    }

    if ( m_historyIndex == HISTORY_INDEX_END ) // at the END mark?
    {
        //wxLogDebug("%s: No PREV done. Do nothing. END.", __FUNCTION__);
        return false;
    }

    ++m_historyIndex;

    wxCHECK_MSG( m_historyIndex >= 0 && m_historyIndex < (int)m_historyMoves.size(), 
                 false, "Invalid index." );
    const hoxMove move = m_historyMoves[m_historyIndex];

    if ( m_historyIndex == (int)m_historyMoves.size() - 1 )
    {
        m_historyIndex = HISTORY_INDEX_END;
    }

    /* Move the piece from ORIGINAL --> NEW position. */

    hoxPiece* piece = _FindPieceAt( move.piece.position );
    wxCHECK_MSG(piece, false, "No Piece found at the ORIGINAL position.");

    _MovePieceTo( piece, move.newPosition, true /* hightlight */, bRefresh );

	return true;
}

bool 
hoxCoreBoard::DoGameReview_END()
{
    while ( this->DoGameReview_NEXT(false /* bRefresh */) ) { }
    this->Refresh();
    return true;
}

void 
hoxCoreBoard::OnMouseEvent( wxMouseEvent& event )
{
    if ( m_moveMode == hoxMOVE_MODE_DRAG_N_DROP ) {
        _OnMouseEvent_DragNDrop(event);
    } else {
        _OnMouseEvent_ClickNClick(event);
    }
}

void 
hoxCoreBoard::_OnMouseEvent_ClickNClick( wxMouseEvent& event )
{
    wxClientDC dc(this);

    if ( event.LeftDown() && m_dragMode == DRAG_MODE_NONE )
    {
        hoxPiece* piece = _FindPiece(event.GetPosition());
        if ( !piece || !_CanPieceMoveNext( piece ) ) {
            return;
        }
        m_dragMode     = DRAG_MODE_START;
        m_dragStartPos = event.GetPosition();
        m_draggedPiece = piece;
        // Highlight the "from" piece.
        const wxPoint newPoint = _GetPieceLocation(m_draggedPiece)
                                + event.GetPosition() - m_dragStartPos;
        const hoxPosition newPos = _PointToPosition(m_draggedPiece, newPoint);
        const wxSize pieceSize = m_draggedPiece->GetBitmap().GetSize();
        const wxPoint newOrigin = _PositionToPieceOrigin(newPos, pieceSize);
        const wxRect rect( newOrigin, pieceSize );
        _DrawHighlight(dc, rect );
    }
    else if ( event.LeftDown() && m_dragMode == DRAG_MODE_START )
    {
        m_dragMode = DRAG_MODE_NONE;
        const wxPoint newPoint = _GetPieceLocation(m_draggedPiece) 
                         + event.GetPosition() - m_dragStartPos;
        _MovePieceToPoint( m_draggedPiece, newPoint );
        m_draggedPiece = NULL;
        m_dragHighlightRect = wxRect();
    }
    else if ( event.Moving() && m_dragMode == DRAG_MODE_START )
    {
        const wxPoint newPoint = _GetPieceLocation(m_draggedPiece)
                                + event.GetPosition() - m_dragStartPos;
        const hoxPosition newPos = _PointToPosition(m_draggedPiece, newPoint);
        const wxSize pieceSize = m_draggedPiece->GetBitmap().GetSize();
        const wxPoint newOrigin = _PositionToPieceOrigin(newPos, pieceSize);
        const wxRect rect( newOrigin, pieceSize );
        if ( rect != m_dragHighlightRect )
        {
            if ( m_dragHighlightPos != m_draggedPiece->GetPosition() )
            {
                _EraseHighlight(dc, m_dragHighlightRect, m_dragHighlightPos);
            }
            _DrawHighlight(dc, rect );
            m_dragHighlightRect = rect;
            m_dragHighlightPos = (m_bViewInverted ? hoxPosition(8 - newPos.x, 9 - newPos.y)
                                                  : newPos);
#ifdef __WXGTK__
            _DrawAllPieces(dc);
#endif
        }
    }
}

void 
hoxCoreBoard::_OnMouseEvent_DragNDrop( wxMouseEvent& event )
{
    if ( event.LeftDown() )
    {
        hoxPiece* piece = _FindPiece(event.GetPosition());
        if ( !piece || !_CanPieceMoveNext( piece ) ) {
            return;
        }
        // We tentatively start dragging, but wait for
        // mouse movement before dragging properly.
        m_dragMode     = DRAG_MODE_START;
        m_dragStartPos = event.GetPosition();
        m_draggedPiece = piece;
    }
    else if (event.Dragging() && m_dragMode == DRAG_MODE_START)
    {
        // We will start dragging if we've moved beyond a couple of pixels
        const int tolerance = 2;  // NOTE: Hard-coded value.
        int dx = abs(event.GetPosition().x - m_dragStartPos.x);
        int dy = abs(event.GetPosition().y - m_dragStartPos.y);
        if (dx <= tolerance && dy <= tolerance) {
            return;
        }

        // Start the drag.
        m_dragMode = DRAG_MODE_DRAGGING;
        delete m_dragImage;

        // Erase the dragged shape from the board
        m_draggedPiece->SetShow(false);
        _ErasePiece(m_draggedPiece);

        m_dragImage = new wxDragImage( m_draggedPiece->GetBitmap(), 
                                       wxCursor(wxCURSOR_HAND) );

        // The offset between the top-left of the shape image and the current shape position
        wxPoint beginDragHotSpot = m_dragStartPos - _GetPieceLocation(m_draggedPiece);

        // Now we do this inside the implementation: always assume
        // coordinates relative to the capture window (client coordinates)

        // Initiate the Drag.
        if ( m_dragImage->BeginDrag( beginDragHotSpot, this ) )
        {
            m_dragImage->Move(event.GetPosition());
            m_dragImage->Show();
        } 
        else  // Drag fails --> Cancel the Drag
        {
            delete m_dragImage;
            m_dragImage = NULL;
            m_dragMode = DRAG_MODE_NONE;
        }
    }
    else if (event.Dragging() && m_dragMode == DRAG_MODE_DRAGGING)
    {
#ifndef __WXMAC__
        m_dragImage->Hide();
#endif
        const wxPoint newPoint = _GetPieceLocation(m_draggedPiece)
                                + event.GetPosition() - m_dragStartPos;
        const hoxPosition newPos = _PointToPosition(m_draggedPiece, newPoint);
        const wxBitmap& bitmap = m_draggedPiece->GetBitmap();
        const wxPoint newOrigin = _PositionToPieceOrigin(newPos, bitmap.GetSize());
        const wxRect rect( newOrigin, bitmap.GetSize() );
        if ( rect != m_dragHighlightRect )
        {
            wxClientDC dc(this);
            _EraseHighlight(dc, m_dragHighlightRect, m_dragHighlightPos);
            _DrawHighlight(dc, rect );
            m_dragHighlightRect = rect;
            m_dragHighlightPos = (m_bViewInverted ? hoxPosition(8 - newPos.x, 9 - newPos.y)
                                                  : newPos);
#ifdef __WXGTK__
            _DrawAllPieces(dc);
#endif
        }
        m_dragImage->Move(event.GetPosition());
#ifndef __WXMAC__
        m_dragImage->Show();
#endif
    }
    else if (event.LeftUp() && m_dragMode != DRAG_MODE_NONE)
    {
        // Finish dragging
        m_dragMode = DRAG_MODE_NONE;

        if (!m_draggedPiece || !m_dragImage) {
            return;
        }

        // Hide the temporary Drag image.
        m_dragImage->Hide();
        m_dragImage->EndDrag();
        delete m_dragImage;
        m_dragImage = NULL;

        // Move the dragged piece to its new location.
        const wxPoint newPoint = _GetPieceLocation(m_draggedPiece) 
                         + event.GetPosition() - m_dragStartPos;
        m_draggedPiece->SetShow(true);
        _MovePieceToPoint( m_draggedPiece, newPoint );
        m_draggedPiece = NULL;
        m_dragHighlightRect = wxRect();
    }
}

void
hoxCoreBoard::_DrawHighlight( wxDC& dc,
                              const wxRect rect )
{
    const int delta = 0;  // TODO: Hard-coded value.
    dc.SetPen( *wxCYAN );
    dc.SetBrush( *wxTRANSPARENT_BRUSH );
    dc.DrawRectangle( rect.x + delta, rect.y + delta,
                      rect.width - 2*delta, rect.height - 2*delta );
}

void
hoxCoreBoard::_EraseHighlight( wxDC& dc,
                               const wxRect rect,
                               const hoxPosition position )
{
#ifdef __WXMAC__
    // Extra code added to remove the 'highlight'.
    dc.SetBrush( *wxTRANSPARENT_BRUSH );
    dc.DrawRectangle( rect );
#endif
    dc.SetClippingRegion( rect );
    m_background->OnPaint(dc);
    dc.DestroyClippingRegion();

    hoxPiece* existPiece = _FindPieceAt( position );
    if (   existPiece
        && (    m_moveMode == hoxMOVE_MODE_CLICK_N_CLICK
             || existPiece != m_draggedPiece) )
    {
        _DrawPieceWithDC(dc, existPiece);
    }
}

void 
hoxCoreBoard::OnMouseCaptureLost( wxMouseCaptureLostEvent& event )
{
    wxLogWarning("**** Receive MOUSE_CAPTURE_LOST event ****");
}

wxPoint 
hoxCoreBoard::_PositionToPieceOrigin( const hoxPosition position,
                                      const wxSize bitmapSize ) const
{
    const wxCoord borderX = m_background->BorderX();
    const wxCoord borderY = m_background->BorderY();
    const wxCoord cellS   = m_background->CellS();

    // Determine the piece's top-left point.
    const int posX = borderX + position.x * cellS - bitmapSize.GetWidth()/2;
    const int posY = borderY + position.y * cellS - bitmapSize.GetHeight()/2;

    return wxPoint(posX,posY);
}

wxPoint 
hoxCoreBoard::_GetPieceLocation( const hoxPiece* piece ) const
{
    hoxPosition pos = piece->GetPosition();
    if (m_bViewInverted)
    {
        pos.x = 8 - pos.x;
        pos.y = 9 - pos.y;
    }

    const wxBitmap& bitmap = piece->GetBitmap();
    return _PositionToPieceOrigin( pos, bitmap.GetSize() );
}

hoxPiece* 
hoxCoreBoard::_FindPieceAt( const hoxPosition& position,
                            bool               includeInactive /* = false */ ) const
{
    hoxPiece* piece = NULL;  // Just a piece holder.
    
    for ( hoxPieceList::const_iterator it = m_pieces.begin(); 
                                       it != m_pieces.end(); ++it )
    {
        piece = *it;
        if ( !includeInactive && !piece->IsActive() )
            continue;
        if ( piece->GetPosition() == position ) 
            return piece;
    }

    return NULL;
}

hoxPiece* 
hoxCoreBoard::_FindAndCapturePieceAt( const hoxPosition& position,
                                      const bool         bRefresh /* = true */ )
{
    hoxPiece* capturedPiece = _FindPieceAt( position );
    if ( capturedPiece == NULL )
        return NULL;

    capturedPiece->SetActive(false);
    if ( bRefresh ) _ErasePiece( capturedPiece );

    /* NOTE: To support GAME-REVIEW feature, use the following trick:
     *     + Make sure the "recent" captured Piece is near the top of
     *       the list so that it can be found in the case of "UN-capture".
     */
    m_pieces.remove( capturedPiece );
    m_pieces.push_front( capturedPiece );

    return capturedPiece;
}

bool
hoxCoreBoard::ReverseView()
{
    m_bViewInverted = !m_bViewInverted;

    wxClientDC dc(this);
    m_background->OnReverseView();
    m_background->OnPaint( dc );

    _DrawAllPieces( dc );
    this->Refresh();  // Somehow this is needed to show the pieces.

    return m_bViewInverted;
}

wxSize 
hoxCoreBoard::DoGetBestSize() const
{
    const wxSize availableSize = GetParent()->GetClientSize();    

    const int proposedHeight = availableSize.GetHeight()
        - m_nBestHeightAdjustment /* TODO: The two players' info + The command bar */;

    const wxSize proposedSize( availableSize.GetWidth(), proposedHeight );
    const wxSize bestSize = m_background->GetBestSize( proposedSize );

    //wxLogDebug("%s: (%d) --> (%d x %d)", __FUNCTION__,
    //    proposedHeight,
    //    bestSize.GetWidth(), bestSize.GetHeight());
    return bestSize;
}

void 
hoxCoreBoard::_RecordMove( const hoxMove& move )
{
    m_historyMoves.push_back( move );
}

void      
hoxCoreBoard::SetGameOver( bool isGameOver /* = true */ )
{
    m_isGameOver = isGameOver;
    m_background->SetGameOver( isGameOver );
    this->Refresh();
}

/**
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * NOTE: This API exists ONLY to help printing debug-message related to
 *       this Board.  wxLogXXX() is not used because its output can be
 *       hidden and may not be visible to the end-users.
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 */
void 
hoxCoreBoard::_PrintDebug( const wxString& debugMsg ) const
{
    if ( m_owner != NULL )
    {
        m_owner->OnBoardMsg( debugMsg );
    }
    wxLogDebug( debugMsg.c_str() );
}


/*****************************************************************************
 *
 *          hoxCoreBackground
 *
 *****************************************************************************/

hoxCoreBackground::hoxCoreBackground()
            : m_totalSize( 600, 700 )
            , m_borderX( 40 )
            , m_borderY( 40 )
            , m_cellS( 56 )
            , m_bViewInverted( false )
            , m_isGameOver( false )
{
}

void 
hoxCoreBackground::DrawGameOverText( wxDC& dc )
{
    dc.SetFont( wxFont( 24, wxFONTFAMILY_ROMAN, 
                        wxFONTSTYLE_ITALIC, wxFONTWEIGHT_NORMAL ) );
    dc.SetTextForeground( *wxRED );
    dc.DrawText( _("Game Over"), 
                 m_borderX + (int)(2.5*m_cellS), 
                 m_borderY + 4*m_cellS );
}

wxSize
hoxCoreBackground::GetBestSize( const wxSize proposedSize )
{
    // TODO: just ignore the proposed Size for now!
    return m_totalSize;
}

/*****************************************************************************
 *
 *          hoxCustomBackground
 *
 *****************************************************************************/

hoxCustomBackground::hoxCustomBackground()
{
    m_backgroundColor = DEFAULT_BOARD_BACKGROUND_COLOR;
    m_foregroundColor = DEFAULT_BOARD_FOREGROUND_COLOR;
}

void
hoxCustomBackground::OnPaint( wxDC& dc )
{
    _DrawWorkSpace( dc, m_totalSize );
    _DrawBoard( dc, m_totalSize );
}

void
hoxCustomBackground::OnResize( const wxSize totalSize )
{
    /* NOTE: Enfore some minimal size. */
    const wxSize minimumSize = wxSize( 400, 470 );

    if ( totalSize.x < minimumSize.x || totalSize.y < minimumSize.y )
    {
        // Do nothing.
    }
    else
    {
        m_totalSize = totalSize;

        // --- Get the board's max-dimension.
        const wxCoord borderW = m_totalSize.GetWidth() - 2*m_borderX;
        const wxCoord borderH = m_totalSize.GetHeight() - 2*m_borderY;
        // --- Calculate the cell's size (*** cell is a square ***)
        m_cellS = wxMin(borderW / NUM_HORIZON_CELL, borderH / NUM_VERTICAL_CELL);
    }
}

void
hoxCustomBackground::OnBgColor( wxColor color )
{
    m_backgroundColor = color;
}

void
hoxCustomBackground::OnFgColor( wxColor color )
{
    m_foregroundColor = color;
}

wxSize
hoxCustomBackground::GetBestSize( const wxSize proposedSize )
{
    const wxSize totalSize( proposedSize.GetWidth(), proposedSize.GetHeight() );

    // *** Recaculated!!!
    this->OnResize( totalSize );

    // Calculate the new "effective" board's size.
    const wxCoord boardW = m_cellS * NUM_HORIZON_CELL;
    const wxCoord boardH = m_cellS * NUM_VERTICAL_CELL;

    return wxSize( boardW + 2*m_borderX, boardH + 2*m_borderY );
}

void 
hoxCustomBackground::_DrawWorkSpace( wxDC&        dc,
                                     const wxSize totalSize )
{
    dc.SetBrush( *hoxBOARD_WORKSPACE_BRUSH );
    dc.DrawRectangle( 0, 0, totalSize.x, totalSize.y );
}

void 
hoxCustomBackground::_DrawBoard( wxDC&        dc,
                                 const wxSize totalSize )
{
    dc.SetBrush( wxBrush( m_backgroundColor ) );
    dc.SetPen( wxPen( m_foregroundColor ) );

    // --- Calculate the new "effective" board's size.
    const wxCoord boardW = m_cellS * NUM_HORIZON_CELL;
    const wxCoord boardH = m_cellS * NUM_VERTICAL_CELL;

    // *** Paint the board with a background's color.
    dc.DrawRectangle( m_borderX, m_borderY, boardW, boardH );

    // Draw vertial lines.
    int line;
    wxCoord x1, y1, x2, y2;
    y1 = m_borderY;
    y2 = m_borderY + boardH;
    const wxCoord leftLabelY = 0; // TODO: Hard-coded for now
    const wxCoord rightLabelY = y2 + m_borderY - dc.GetCharHeight() - leftLabelY;
    wxChar c = m_bViewInverted ? 'i' : 'a';
    for (line = 0; line < NUM_HORIZON_CELL+1; ++line)
    {
        x1 = m_borderX + line * m_cellS;
        x2 = x1;

        dc.DrawText(c, x1, leftLabelY);
        dc.DrawLine(x1, y1, x2, y2);
        dc.DrawText(c, x2, rightLabelY);
        if (m_bViewInverted) --c;
        else                 ++c;
    }

    // Draw horizontal lines.
    x1 = m_borderX;
    x2 = m_borderX + boardW;
    const wxCoord leftLabelX = 5; // TODO: Hard-coded for now
    const wxCoord rightLabelX = m_borderX - dc.GetCharWidth() - leftLabelX;
    c = m_bViewInverted ? '0' : '9';
    for (line = 0; line < NUM_VERTICAL_CELL+1; ++line)
    {
        y1 = m_borderY + line * m_cellS;
        y2 = y1;

        dc.DrawText(c, leftLabelX, y1);
        dc.DrawLine(x1, y1, x2, y2);
        dc.DrawText(c, x2 + rightLabelX, y2);
        if (m_bViewInverted) ++c;
        else                 --c;
    }

    // Draw crossing lines at the red-palace.
    dc.DrawLine(m_borderX+3*m_cellS, m_borderY+9*m_cellS, 
                m_borderX+5*m_cellS, m_borderY+7*m_cellS);
    dc.DrawLine(m_borderX+3*m_cellS, m_borderY+7*m_cellS, 
                m_borderX+5*m_cellS, m_borderY+9*m_cellS);

    // Draw crossing lines at the black-palace.
    dc.DrawLine(m_borderX+3*m_cellS, m_borderY,
                m_borderX+5*m_cellS, m_borderY+2*m_cellS);
    dc.DrawLine(m_borderX+3*m_cellS, m_borderY+2*m_cellS, 
                m_borderX+5*m_cellS, m_borderY);

    // Draw the "miror" lines for Cannon and Pawn.
    const int nSize  = m_cellS / 7;  // The "miror" 's size.
    const int nSpace = 3;            // The "miror" 's space (how close/far).
    
    int locations[][2] = { { 1, 2 }, { 7, 2 },
                           { 0, 3 }, { 2, 3 }, { 4, 3 }, { 6, 3 }, { 8, 3 },
                           { 0, 6 }, { 2, 6 }, { 4, 6 }, { 6, 6 }, { 8, 6 },
                           { 1, 7 }, { 7, 7 },
                         };
    wxPoint mirrors[3];
    for ( int i = 0; i < 14; ++i )
    {
        wxPoint oriPoint( m_borderX+locations[i][0]*m_cellS, 
                          m_borderY+locations[i][1]*m_cellS );
        bool bNoLeft = ( locations[i][0] == 0 )
                    && ( locations[i][1] == 3 || locations[i][1] == 6 );
        bool bNoRight = ( locations[i][0] == 8 )
                     && ( locations[i][1] == 3 || locations[i][1] == 6 );

        if ( !bNoLeft )
        {
            mirrors[0] = wxPoint( oriPoint.x - nSpace, oriPoint.y - nSpace - nSize );
            mirrors[1] = wxPoint( oriPoint.x - nSpace, oriPoint.y - nSpace );
            mirrors[2] = wxPoint( oriPoint.x - nSpace - nSize, oriPoint.y - nSpace );
            dc.DrawLines(3, mirrors);

            mirrors[0] = wxPoint( oriPoint.x - nSpace - nSize, oriPoint.y + nSpace );
            mirrors[1] = wxPoint( oriPoint.x - nSpace, oriPoint.y + nSpace );
            mirrors[2] = wxPoint( oriPoint.x - nSpace, oriPoint.y + nSpace + nSize );
            dc.DrawLines(3, mirrors);
        }

        if ( !bNoRight )
        {
            mirrors[0] = wxPoint( oriPoint.x + nSpace, oriPoint.y - nSpace - nSize );
            mirrors[1] = wxPoint( oriPoint.x + nSpace, oriPoint.y - nSpace );
            mirrors[2] = wxPoint( oriPoint.x + nSpace + nSize, oriPoint.y - nSpace );
            dc.DrawLines(3, mirrors);

            mirrors[0] = wxPoint( oriPoint.x + nSpace, oriPoint.y + nSpace + nSize );
            mirrors[1] = wxPoint( oriPoint.x + nSpace, oriPoint.y + nSpace );
            mirrors[2] = wxPoint( oriPoint.x + nSpace + nSize, oriPoint.y + nSpace );
            dc.DrawLines(3, mirrors);
        }
    }

    // Delete lines at the 'river' by drawing lines with
    // the background's color.
    y1 = m_borderY + 4*m_cellS;
    y2 = y1 + m_cellS;
    dc.SetPen( wxPen( m_backgroundColor ) );
    for (line = 1; line < NUM_VERTICAL_CELL-1; ++line)
    {
        x1 = m_borderX + line * m_cellS;
        x2 = x1;

        dc.DrawLine(x1, y1, x2, y2);
    }

    // Drawing "Game Over" if specified.
    if ( m_isGameOver )
    {
        this->DrawGameOverText(dc);
    }
}

/*****************************************************************************
 *
 *          hoxImageBackground
 *
 *****************************************************************************/

hoxImageBackground::hoxImageBackground( const wxString& sImage )
{
    wxImage           boardImage;
    hoxBoardImageInfo boardInfo;
    if ( ! hoxUtil::LoadBoardImage( sImage, boardImage, boardInfo ) )
    {
        wxLogError("%s: Failed to load board-image [%s].", __FUNCTION__, sImage.c_str());
        return;
    }
    m_borderX = boardInfo.borderX;
    m_borderY = boardInfo.borderY;
    m_cellS   = boardInfo.cellS;

    m_bitmap = wxBitmap(boardImage);
}

void
hoxImageBackground::OnReverseView()
{
    hoxCoreBackground::OnReverseView();
    wxImage image = m_bitmap.ConvertToImage();
    m_bitmap = wxBitmap(image.Rotate90().Rotate90());
}

void
hoxImageBackground::OnPaint( wxDC& dc )
{
    dc.SetBrush( *hoxBOARD_WORKSPACE_BRUSH );
    dc.DrawRectangle( m_totalSize );

    _DrawBoardImage( dc );
}

void
hoxImageBackground::OnResize( const wxSize totalSize )
{
    m_totalSize = totalSize;
}

wxSize
hoxImageBackground::GetBestSize( const wxSize proposedSize )
{
    // TODO: just ignore the proposed Size for now!
    return wxSize( m_bitmap.GetWidth(), m_bitmap.GetHeight() );
}

void 
hoxImageBackground::_DrawBoardImage( wxDC& dc )
{
    wxMemoryDC memDC( m_bitmap );
    dc.Blit( 0, 0, m_bitmap.GetWidth(), m_bitmap.GetHeight(),
             &memDC, 0, 0, wxCOPY, true );

    if ( m_isGameOver )
    {
        this->DrawGameOverText(dc);
    }
}

/************************* END OF FILE ***************************************/
