/**
 * @file EditorController.h
 * @brief Translates raw keyboard/mouse input into high-level editing commands.
 */

#pragma once

#include <QKeyEvent>
#include <QObject>
#include <cstdint>

class PieceTable;
class LineManager;
class CursorManager;

/**
 * @class EditorController
 * @brief Owns keyboard/mouse handling: cursor navigation and document mutation.
 *
 * Operates on the shared @ref CursorManager and the active PieceTable/LineManager, keeping the
 * byte-offset arithmetic for insertion and deletion out of the view widget. Reports edits via
 * @ref documentEdited so the view can invalidate its line cache synchronously.
 */
class EditorController : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Constructs the controller bound to a cursor.
     * @param cursor Shared cursor state (not owned).
     * @param parent Qt parent.
     */
    explicit EditorController( CursorManager* cursor, QObject* parent = nullptr );

    /**
     * @brief Updates the backend context when the open document changes.
     */
    auto setContext( PieceTable* pieceTable, LineManager* lineManager ) -> void;

    /**
     * @brief Handles a key press: navigation, insertion or deletion.
     * @param event The Qt key event.
     * @return True if the document was modified.
     */
    auto handleKeyPress( QKeyEvent* event ) -> bool;

    /**
     * @brief Handles a click already mapped to a virtual line and approximate column.
     * @param targetLine Virtual line clicked.
     * @param approxCol Column from pixel mapping (clamped to the line length).
     */
    auto handleMouseClick( int targetLine, int approxCol ) -> void;

    /**
     * @brief Converts (line, col) to a logical byte position.
     */
    [[nodiscard]] auto logicalPosition( int line, int col ) const -> uint64_t;

signals:
    /**
     * @brief Emitted when the document is edited; @p changedOffset is the first affected byte.
     *
     * Connected synchronously so the view's line cache is invalidated before subsequent
     * line-length queries in the same handler.
     */
    void documentEdited( uint64_t changedOffset );

private:
    CursorManager* cursor_;
    PieceTable* pieceTable_{ nullptr };
    LineManager* lineManager_{ nullptr };
};
