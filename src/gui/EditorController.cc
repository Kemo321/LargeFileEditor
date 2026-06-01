#include "gui/EditorController.h"

#include <algorithm>
#include <string>

#include "backend/PieceTable.h"
#include "gui/CursorManager.h"
#include "gui/LineManager.h"
#include "util/Utf8Utils.h"

EditorController::EditorController( CursorManager* cursor, QObject* parent )
    : QObject( parent ), cursor_( cursor )
{
}

auto EditorController::setContext( PieceTable* pieceTable, LineManager* lineManager ) -> void
{
    pieceTable_ = pieceTable;
    lineManager_ = lineManager;
}

auto EditorController::logicalPosition( int line, int col ) const -> uint64_t
{
    if( pieceTable_ == nullptr || lineManager_ == nullptr ) {
        return 0;
    }
    return lineManager_->getLineOffset( line ) + col;
}

auto EditorController::handleKeyPress( QKeyEvent* event ) -> bool
{
    if( pieceTable_ == nullptr || lineManager_ == nullptr ) {
        return false;
    }

    QString text = event->text();
    int key = event->key();
    int line = cursor_->line();
    int col = cursor_->col();
    bool tableModified = false;

    switch( key ) {
        case Qt::Key_Left:
            moveCursorLeft( line, col );
            break;
        case Qt::Key_Right:
            moveCursorRight( line, col );
            break;
        case Qt::Key_Up:
            moveCursorUp( line, col );
            break;
        case Qt::Key_Down:
            moveCursorDown( line, col );
            break;
        case Qt::Key_Backspace:
            tableModified = handleBackspace( line, col );
            break;
        case Qt::Key_Delete:
            tableModified = handleDelete( line, col );
            break;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            tableModified = handleNewline( line, col );
            break;
        default:
            if( !text.isEmpty() && text.at( 0 ).isPrint() ) {
                tableModified = insertPrintableText( text, line, col );
            }
            break;
    }

    cursor_->setPosition( line, col );
    return tableModified;
}

auto EditorController::moveCursorLeft( int& line, int& col ) -> void
{
    if( col > 0 ) {
        --col;
        uint64_t lineStart = lineManager_->getLineOffset( line );
        while( col > 0 ) {
            std::string b = pieceTable_->getSubstr( lineStart + col, 1 );
            if( b.empty() ) {
                break;
            }
            if( !Utf8Utils::isContinuationByte( static_cast<unsigned char>( b[0] ) ) ) {
                break;
            }
            --col;
        }
    } else if( line > 0 ) {
        --line;
        col = static_cast<int>( lineManager_->getVirtualLineLength( line ) );
    }
}

auto EditorController::moveCursorRight( int& line, int& col ) -> void
{
    uint64_t lineLen = lineManager_->getVirtualLineLength( line );
    if( col < static_cast<int>( lineLen ) ) {
        uint64_t lineStart = lineManager_->getLineOffset( line );
        std::string b = pieceTable_->getSubstr( lineStart + col, 1 );
        ++col;
        if( !b.empty() ) {
            col += Utf8Utils::sequenceLength( static_cast<unsigned char>( b[0] ) ) - 1;
        }
        col = std::min( col, static_cast<int>( lineLen ) );
    } else if( line < lineManager_->getLineCount() - 1 ) {
        ++line;
        col = 0;
    }
}

auto EditorController::moveCursorUp( int& line, int& col ) -> void
{
    if( line > 0 ) {
        --line;
        col = std::min( col, static_cast<int>( lineManager_->getVirtualLineLength( line ) ) );
    }
}

auto EditorController::moveCursorDown( int& line, int& col ) -> void
{
    if( line < lineManager_->getLineCount() - 1 ) {
        ++line;
        col = std::min( col, static_cast<int>( lineManager_->getVirtualLineLength( line ) ) );
    }
}

auto EditorController::handleBackspace( int& line, int& col ) -> bool
{
    uint64_t pos = logicalPosition( line, col );
    if( pos == 0 ) {
        return false;
    }

    int bytes_to_remove = 1;
    int check_col = col - 1;
    uint64_t lineStart = lineManager_->getLineOffset( line );
    while( check_col > 0 ) {
        std::string b = pieceTable_->getSubstr( lineStart + check_col, 1 );
        if( b.empty() ) {
            break;
        }
        if( !Utf8Utils::isContinuationByte( static_cast<unsigned char>( b[0] ) ) ) {
            break;
        }
        --check_col;
        ++bytes_to_remove;
    }
    pieceTable_->remove( pos - bytes_to_remove, bytes_to_remove );
    emit documentEdited( pos - bytes_to_remove );
    if( col > 0 ) {
        col -= bytes_to_remove;
    } else {
        --line;
        col = static_cast<int>( lineManager_->getVirtualLineLength( line ) );
    }
    return true;
}

auto EditorController::handleDelete( int line, int col ) -> bool
{
    uint64_t pos = logicalPosition( line, col );
    if( pos >= pieceTable_->size() ) {
        return false;
    }

    int bytes_to_remove = 1;
    std::string b = pieceTable_->getSubstr( pos, 1 );
    if( !b.empty() ) {
        bytes_to_remove = Utf8Utils::sequenceLength( static_cast<unsigned char>( b[0] ) );
    }
    pieceTable_->remove( pos, bytes_to_remove );
    emit documentEdited( pos );
    return true;
}

auto EditorController::handleNewline( int& line, int& col ) -> bool
{
    uint64_t pos = logicalPosition( line, col );
    pieceTable_->insert( pos, "\n" );
    emit documentEdited( pos );
    ++line;
    col = 0;
    return true;
}

auto EditorController::insertPrintableText( const QString& text, int line, int& col ) -> bool
{
    uint64_t pos = logicalPosition( line, col );
    pieceTable_->insert( pos, text.toStdString() );
    emit documentEdited( pos );
    col += static_cast<int>( text.length() );
    return true;
}

auto EditorController::handleMouseClick( int targetLine, int approxCol ) -> void
{
    if( pieceTable_ == nullptr || lineManager_ == nullptr ) {
        return;
    }

    uint64_t lineLen = lineManager_->getVirtualLineLength( targetLine );
    uint64_t lineStart = lineManager_->getLineOffset( targetLine );

    // Snap approxCol back to a UTF-8 character boundary.
    if( approxCol > 0 && approxCol < static_cast<int>( lineLen ) ) {
        auto byteAt = [this]( uint64_t pos ) {
            return static_cast<unsigned char>( pieceTable_->getSubstr( pos, 1 )[0] );
        };
        uint64_t snapped =
            Utf8Utils::snapToCharacterBoundary( byteAt, lineStart, lineStart + approxCol );
        approxCol = static_cast<int>( snapped - lineStart );
    }

    cursor_->setPosition( targetLine, approxCol );
}
