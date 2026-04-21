/**
 * Authors: Jan Szwagierczak
 * Description: Header of the non-modal Find and Replace dialog.
 */

#pragma once

#include <QCheckBox>
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>

class FindReplaceDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Constructs the non-modal Find and Replace dialog.
     * @param parent The parent widget.
     */
    explicit FindReplaceDialog( QWidget* parent = nullptr );
    ~FindReplaceDialog() override = default;

    /**
     * @brief Shows the dialog with the "Find" tab active.
     */
    void showFind();

    /**
     * @brief Shows the dialog with the "Replace" tab active.
     */
    void showReplace();

private:
    void setupUi();

    QTabWidget* tab_widget_{};

    // Find Tab
    QLineEdit* find_input_1_{};
    QCheckBox* match_case_1_{};
    QCheckBox* match_word_1_{};

    // Replace Tab
    QLineEdit* find_input_2_{};
    QLineEdit* replace_input_{};
    QCheckBox* match_case_2_{};
    QCheckBox* match_word_2_{};
};
