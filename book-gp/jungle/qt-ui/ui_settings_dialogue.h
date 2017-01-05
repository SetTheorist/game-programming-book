/********************************************************************************
** Form generated from reading UI file 'settings_dialogue.ui'
**
** Created: Tue Dec 21 16:01:04 2010
**      by: Qt User Interface Compiler version 4.6.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_SETTINGS_DIALOGUE_H
#define UI_SETTINGS_DIALOGUE_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QCheckBox>
#include <QtGui/QDialog>
#include <QtGui/QDialogButtonBox>
#include <QtGui/QFrame>
#include <QtGui/QHBoxLayout>
#include <QtGui/QHeaderView>
#include <QtGui/QLabel>
#include <QtGui/QSpinBox>
#include <QtGui/QVBoxLayout>
#include <QtGui/QWidget>

QT_BEGIN_NAMESPACE

class Ui_settingsDialog
{
public:
    QCheckBox *humanCheck;
    QFrame *compFrame;
    QWidget *widget;
    QHBoxLayout *hboxLayout;
    QFrame *frame;
    QCheckBox *nmpCheck;
    QWidget *layoutWidget;
    QVBoxLayout *vboxLayout;
    QHBoxLayout *hboxLayout1;
    QLabel *nmpr1Label;
    QSpinBox *nmpr1Spin;
    QHBoxLayout *hboxLayout2;
    QLabel *nmpr2Label;
    QSpinBox *nmpr2Spin;
    QHBoxLayout *hboxLayout3;
    QLabel *nmpcutoffLabel;
    QSpinBox *nmpcutoffSpin;
    QVBoxLayout *vboxLayout1;
    QHBoxLayout *hboxLayout4;
    QLabel *label_4;
    QSpinBox *spinBox_4;
    QHBoxLayout *hboxLayout5;
    QCheckBox *checkBox_3;
    QCheckBox *checkBox_4;
    QFrame *frame_2;
    QWidget *layoutWidget1;
    QVBoxLayout *vboxLayout2;
    QCheckBox *quiescenceCheck;
    QHBoxLayout *hboxLayout6;
    QLabel *qdepthLabel;
    QSpinBox *qdepthSpin;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *settingsDialog)
    {
        if (settingsDialog->objectName().isEmpty())
            settingsDialog->setObjectName(QString::fromUtf8("settingsDialog"));
        settingsDialog->resize(400, 300);
        humanCheck = new QCheckBox(settingsDialog);
        humanCheck->setObjectName(QString::fromUtf8("humanCheck"));
        humanCheck->setGeometry(QRect(10, 10, 201, 24));
        compFrame = new QFrame(settingsDialog);
        compFrame->setObjectName(QString::fromUtf8("compFrame"));
        compFrame->setGeometry(QRect(9, 40, 339, 181));
        compFrame->setFrameShape(QFrame::StyledPanel);
        compFrame->setFrameShadow(QFrame::Raised);
        widget = new QWidget(compFrame);
        widget->setObjectName(QString::fromUtf8("widget"));
        widget->setGeometry(QRect(10, 10, 319, 161));
        hboxLayout = new QHBoxLayout(widget);
        hboxLayout->setObjectName(QString::fromUtf8("hboxLayout"));
        hboxLayout->setContentsMargins(0, 0, 0, 0);
        frame = new QFrame(widget);
        frame->setObjectName(QString::fromUtf8("frame"));
        frame->setFrameShape(QFrame::StyledPanel);
        frame->setFrameShadow(QFrame::Raised);
        nmpCheck = new QCheckBox(frame);
        nmpCheck->setObjectName(QString::fromUtf8("nmpCheck"));
        nmpCheck->setGeometry(QRect(20, 10, 51, 24));
        nmpCheck->setChecked(true);
        layoutWidget = new QWidget(frame);
        layoutWidget->setObjectName(QString::fromUtf8("layoutWidget"));
        layoutWidget->setGeometry(QRect(10, 40, 93, 104));
        vboxLayout = new QVBoxLayout(layoutWidget);
        vboxLayout->setObjectName(QString::fromUtf8("vboxLayout"));
        vboxLayout->setContentsMargins(0, 0, 0, 0);
        hboxLayout1 = new QHBoxLayout();
        hboxLayout1->setObjectName(QString::fromUtf8("hboxLayout1"));
        nmpr1Label = new QLabel(layoutWidget);
        nmpr1Label->setObjectName(QString::fromUtf8("nmpr1Label"));

        hboxLayout1->addWidget(nmpr1Label);

        nmpr1Spin = new QSpinBox(layoutWidget);
        nmpr1Spin->setObjectName(QString::fromUtf8("nmpr1Spin"));

        hboxLayout1->addWidget(nmpr1Spin);


        vboxLayout->addLayout(hboxLayout1);

        hboxLayout2 = new QHBoxLayout();
        hboxLayout2->setObjectName(QString::fromUtf8("hboxLayout2"));
        nmpr2Label = new QLabel(layoutWidget);
        nmpr2Label->setObjectName(QString::fromUtf8("nmpr2Label"));

        hboxLayout2->addWidget(nmpr2Label);

        nmpr2Spin = new QSpinBox(layoutWidget);
        nmpr2Spin->setObjectName(QString::fromUtf8("nmpr2Spin"));

        hboxLayout2->addWidget(nmpr2Spin);


        vboxLayout->addLayout(hboxLayout2);

        hboxLayout3 = new QHBoxLayout();
        hboxLayout3->setObjectName(QString::fromUtf8("hboxLayout3"));
        nmpcutoffLabel = new QLabel(layoutWidget);
        nmpcutoffLabel->setObjectName(QString::fromUtf8("nmpcutoffLabel"));

        hboxLayout3->addWidget(nmpcutoffLabel);

        nmpcutoffSpin = new QSpinBox(layoutWidget);
        nmpcutoffSpin->setObjectName(QString::fromUtf8("nmpcutoffSpin"));

        hboxLayout3->addWidget(nmpcutoffSpin);


        vboxLayout->addLayout(hboxLayout3);


        hboxLayout->addWidget(frame);

        vboxLayout1 = new QVBoxLayout();
        vboxLayout1->setObjectName(QString::fromUtf8("vboxLayout1"));
        hboxLayout4 = new QHBoxLayout();
        hboxLayout4->setObjectName(QString::fromUtf8("hboxLayout4"));
        label_4 = new QLabel(widget);
        label_4->setObjectName(QString::fromUtf8("label_4"));

        hboxLayout4->addWidget(label_4);

        spinBox_4 = new QSpinBox(widget);
        spinBox_4->setObjectName(QString::fromUtf8("spinBox_4"));

        hboxLayout4->addWidget(spinBox_4);


        vboxLayout1->addLayout(hboxLayout4);

        hboxLayout5 = new QHBoxLayout();
        hboxLayout5->setObjectName(QString::fromUtf8("hboxLayout5"));
        checkBox_3 = new QCheckBox(widget);
        checkBox_3->setObjectName(QString::fromUtf8("checkBox_3"));

        hboxLayout5->addWidget(checkBox_3);

        checkBox_4 = new QCheckBox(widget);
        checkBox_4->setObjectName(QString::fromUtf8("checkBox_4"));

        hboxLayout5->addWidget(checkBox_4);


        vboxLayout1->addLayout(hboxLayout5);

        frame_2 = new QFrame(widget);
        frame_2->setObjectName(QString::fromUtf8("frame_2"));
        frame_2->setFrameShape(QFrame::StyledPanel);
        frame_2->setFrameShadow(QFrame::Raised);
        layoutWidget1 = new QWidget(frame_2);
        layoutWidget1->setObjectName(QString::fromUtf8("layoutWidget1"));
        layoutWidget1->setGeometry(QRect(10, 10, 94, 62));
        vboxLayout2 = new QVBoxLayout(layoutWidget1);
        vboxLayout2->setObjectName(QString::fromUtf8("vboxLayout2"));
        vboxLayout2->setContentsMargins(0, 0, 0, 0);
        quiescenceCheck = new QCheckBox(layoutWidget1);
        quiescenceCheck->setObjectName(QString::fromUtf8("quiescenceCheck"));
        quiescenceCheck->setChecked(true);

        vboxLayout2->addWidget(quiescenceCheck);

        hboxLayout6 = new QHBoxLayout();
        hboxLayout6->setObjectName(QString::fromUtf8("hboxLayout6"));
        qdepthLabel = new QLabel(layoutWidget1);
        qdepthLabel->setObjectName(QString::fromUtf8("qdepthLabel"));

        hboxLayout6->addWidget(qdepthLabel);

        qdepthSpin = new QSpinBox(layoutWidget1);
        qdepthSpin->setObjectName(QString::fromUtf8("qdepthSpin"));

        hboxLayout6->addWidget(qdepthSpin);


        vboxLayout2->addLayout(hboxLayout6);


        vboxLayout1->addWidget(frame_2);


        hboxLayout->addLayout(vboxLayout1);

        buttonBox = new QDialogButtonBox(settingsDialog);
        buttonBox->setObjectName(QString::fromUtf8("buttonBox"));
        buttonBox->setGeometry(QRect(10, 230, 341, 32));
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::NoButton|QDialogButtonBox::Ok);

        retranslateUi(settingsDialog);
        QObject::connect(buttonBox, SIGNAL(accepted()), settingsDialog, SLOT(accept()));
        QObject::connect(buttonBox, SIGNAL(rejected()), settingsDialog, SLOT(reject()));
        QObject::connect(humanCheck, SIGNAL(toggled(bool)), compFrame, SLOT(setDisabled(bool)));
        QObject::connect(quiescenceCheck, SIGNAL(toggled(bool)), qdepthLabel, SLOT(setEnabled(bool)));
        QObject::connect(quiescenceCheck, SIGNAL(toggled(bool)), qdepthSpin, SLOT(setEnabled(bool)));
        QObject::connect(nmpCheck, SIGNAL(toggled(bool)), nmpr1Label, SLOT(setEnabled(bool)));
        QObject::connect(nmpCheck, SIGNAL(toggled(bool)), nmpr1Spin, SLOT(setEnabled(bool)));
        QObject::connect(nmpCheck, SIGNAL(toggled(bool)), nmpr2Label, SLOT(setEnabled(bool)));
        QObject::connect(nmpCheck, SIGNAL(toggled(bool)), nmpr2Spin, SLOT(setEnabled(bool)));
        QObject::connect(nmpCheck, SIGNAL(toggled(bool)), nmpcutoffLabel, SLOT(setEnabled(bool)));
        QObject::connect(nmpCheck, SIGNAL(toggled(bool)), nmpcutoffSpin, SLOT(setEnabled(bool)));

        QMetaObject::connectSlotsByName(settingsDialog);
    } // setupUi

    void retranslateUi(QDialog *settingsDialog)
    {
        settingsDialog->setWindowTitle(QApplication::translate("settingsDialog", "Dialog", 0, QApplication::UnicodeUTF8));
        humanCheck->setText(QApplication::translate("settingsDialog", "Human (or equivalent) player", 0, QApplication::UnicodeUTF8));
        nmpCheck->setText(QApplication::translate("settingsDialog", "NMP", 0, QApplication::UnicodeUTF8));
        nmpr1Label->setText(QApplication::translate("settingsDialog", "R1", 0, QApplication::UnicodeUTF8));
        nmpr2Label->setText(QApplication::translate("settingsDialog", "R2", 0, QApplication::UnicodeUTF8));
        nmpcutoffLabel->setText(QApplication::translate("settingsDialog", "Cutoff", 0, QApplication::UnicodeUTF8));
        label_4->setText(QApplication::translate("settingsDialog", "Depth", 0, QApplication::UnicodeUTF8));
        checkBox_3->setText(QApplication::translate("settingsDialog", "TT", 0, QApplication::UnicodeUTF8));
        checkBox_4->setText(QApplication::translate("settingsDialog", "ID", 0, QApplication::UnicodeUTF8));
        quiescenceCheck->setText(QApplication::translate("settingsDialog", "Quiescence", 0, QApplication::UnicodeUTF8));
        qdepthLabel->setText(QApplication::translate("settingsDialog", "Depth", 0, QApplication::UnicodeUTF8));
    } // retranslateUi

};

namespace Ui {
    class settingsDialog: public Ui_settingsDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SETTINGS_DIALOGUE_H
