/**
 * @file FindReplaceDialog.h
 * @author Tomasz Okon
 * @brief GUI Dialog for managing search and replace operations.
 */

#pragma once

#include <QCheckBox>
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QString>
#include <QTabWidget>

/**
 * @class FindReplaceDialog
 * @brief Simple tabbed dialog providing Find and Replace user inputs.
 */
class FindReplaceDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Constructs the dialog widget.
     * @param parent Pointer to parent QWidget.
     */
    explicit FindReplaceDialog( QWidget* parent = nullptr );
    ~FindReplaceDialog() override = default;

    /**
     * @brief Displays the dialog on the Find tab.
     */
    auto showFind() -> void;

    /**
     * @brief Displays the dialog on the Replace tab.
     */
    auto showReplace() -> void;

signals:
    auto findNextRequested( const QString& text, bool matchCase, bool matchWord ) -> void;
    auto replaceNextRequested( const QString& findText, const QString& replaceText, bool matchCase,
                               bool matchWord ) -> void;
    auto replaceAllRequested( const QString& findText, const QString& replaceText, bool matchCase,
                              bool matchWord ) -> void;

private:
    auto setupUi() -> void;

    QTabWidget* tab_widget_{};

    QLineEdit* find_input_1_{};
    QCheckBox* match_case_1_{};
    QCheckBox* match_word_1_{};

    QLineEdit* find_input_2_{};
    QLineEdit* replace_input_{};
    QCheckBox* match_case_2_{};
    QCheckBox* match_word_2_{};
};
