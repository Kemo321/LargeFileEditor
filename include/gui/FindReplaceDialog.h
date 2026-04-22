#pragma once

#include <QCheckBox>
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QString>
#include <QTabWidget>

class FindReplaceDialog : public QDialog {
    Q_OBJECT

public:
    explicit FindReplaceDialog( QWidget* parent = nullptr );
    ~FindReplaceDialog() override = default;

    void showFind();
    void showReplace();

signals:
    void findNextRequested( const QString& text, bool matchCase, bool matchWord );
    void replaceNextRequested( const QString& findText, const QString& replaceText, bool matchCase,
                               bool matchWord );
    void replaceAllRequested( const QString& findText, const QString& replaceText, bool matchCase,
                              bool matchWord );

private:
    void setupUi();

    QTabWidget* tab_widget_{};

    QLineEdit* find_input_1_{};
    QCheckBox* match_case_1_{};
    QCheckBox* match_word_1_{};

    QLineEdit* find_input_2_{};
    QLineEdit* replace_input_{};
    QCheckBox* match_case_2_{};
    QCheckBox* match_word_2_{};
};
