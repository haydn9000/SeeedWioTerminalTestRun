# -*- coding: utf-8 -*-

################################################################################
## Form generated from reading UI file 'WioTerminalBitmapConverterGUI.ui'
##
## Created by: Qt User Interface Compiler version 6.1.0
##
## WARNING! All changes made in this file will be lost when recompiling UI file!
################################################################################

from PySide6.QtCore import *
from PySide6.QtGui import *
from PySide6.QtWidgets import *


class Ui_MainWindow(object):
    def setupUi(self, MainWindow):
        if not MainWindow.objectName():
            MainWindow.setObjectName(u"MainWindow")
        MainWindow.resize(485, 269)
        MainWindow.setMinimumSize(QSize(485, 224))
        MainWindow.setStyleSheet(u"background-color: #354f58;\n"
"color: rgb(144, 216, 0);")
        self.centralwidget = QWidget(MainWindow)
        self.centralwidget.setObjectName(u"centralwidget")
        self.verticalLayout = QVBoxLayout(self.centralwidget)
        self.verticalLayout.setObjectName(u"verticalLayout")
        self.frame = QFrame(self.centralwidget)
        self.frame.setObjectName(u"frame")
        self.frame.setStyleSheet(u"label_3\n"
"{\n"
"	color: rgb(255, 255, 255);\n"
"}")
        self.frame.setFrameShape(QFrame.NoFrame)
        self.frame.setFrameShadow(QFrame.Raised)
        self.frame.setLineWidth(0)
        self.horizontalLayout = QHBoxLayout(self.frame)
        self.horizontalLayout.setObjectName(u"horizontalLayout")
        self.horizontalLayout.setContentsMargins(1, 1, 1, 1)
        self.gridLayout = QGridLayout()
        self.gridLayout.setObjectName(u"gridLayout")
        self.gridLayout.setVerticalSpacing(20)
        self.InputLineEdit = QLineEdit(self.frame)
        self.InputLineEdit.setObjectName(u"InputLineEdit")
        self.InputLineEdit.setStyleSheet(u"background-color: rgb(214, 214, 214);")

        self.gridLayout.addWidget(self.InputLineEdit, 2, 2, 1, 1)

        self.InputBrowsePushButton = QPushButton(self.frame)
        self.InputBrowsePushButton.setObjectName(u"InputBrowsePushButton")
        self.InputBrowsePushButton.setStyleSheet(u"")

        self.gridLayout.addWidget(self.InputBrowsePushButton, 2, 3, 1, 1)

        self.OutputLineEdit = QLineEdit(self.frame)
        self.OutputLineEdit.setObjectName(u"OutputLineEdit")
        self.OutputLineEdit.setStyleSheet(u"background-color: rgb(214, 214, 214);")

        self.gridLayout.addWidget(self.OutputLineEdit, 3, 2, 1, 1)

        self.InputBrowsePushButton_2 = QPushButton(self.frame)
        self.InputBrowsePushButton_2.setObjectName(u"InputBrowsePushButton_2")
        self.InputBrowsePushButton_2.setStyleSheet(u"")

        self.gridLayout.addWidget(self.InputBrowsePushButton_2, 3, 3, 1, 1)

        self.label = QLabel(self.frame)
        self.label.setObjectName(u"label")
        self.label.setStyleSheet(u"")
        self.label.setAlignment(Qt.AlignRight|Qt.AlignTrailing|Qt.AlignVCenter)

        self.gridLayout.addWidget(self.label, 2, 0, 1, 1)

        self.label_2 = QLabel(self.frame)
        self.label_2.setObjectName(u"label_2")
        self.label_2.setStyleSheet(u"")

        self.gridLayout.addWidget(self.label_2, 3, 0, 1, 1)

        self.label_3 = QLabel(self.frame)
        self.label_3.setObjectName(u"label_3")
        self.label_3.setMaximumSize(QSize(16777215, 50))
        font = QFont()
        font.setFamilies([u"Bahnschrift SemiLight"])
        font.setPointSize(12)
        self.label_3.setFont(font)
        self.label_3.setStyleSheet(u"")
        self.label_3.setAlignment(Qt.AlignCenter)

        self.gridLayout.addWidget(self.label_3, 0, 0, 1, 4)

        self.StatusLabel = QLabel(self.frame)
        self.StatusLabel.setObjectName(u"StatusLabel")
        self.StatusLabel.setMinimumSize(QSize(0, 40))
        self.StatusLabel.setStyleSheet(u"")
        self.StatusLabel.setAlignment(Qt.AlignLeading|Qt.AlignLeft|Qt.AlignTop)

        self.gridLayout.addWidget(self.StatusLabel, 8, 0, 1, 3)

        self.horizontalLayout_2 = QHBoxLayout()
        self.horizontalLayout_2.setObjectName(u"horizontalLayout_2")
        self.label_5 = QLabel(self.frame)
        self.label_5.setObjectName(u"label_5")
        self.label_5.setAlignment(Qt.AlignRight|Qt.AlignTrailing|Qt.AlignVCenter)
        self.label_5.setMargin(0)
        self.label_5.setIndent(10)

        self.horizontalLayout_2.addWidget(self.label_5)

        self.radioButton = QRadioButton(self.frame)
        self.radioButton.setObjectName(u"radioButton")
        self.radioButton.setMaximumSize(QSize(100, 16777215))
        self.radioButton.setLayoutDirection(Qt.LeftToRight)
        self.radioButton.setAutoFillBackground(False)
        self.radioButton.setChecked(False)

        self.horizontalLayout_2.addWidget(self.radioButton)

        self.radioButton_2 = QRadioButton(self.frame)
        self.radioButton_2.setObjectName(u"radioButton_2")
        self.radioButton_2.setChecked(True)

        self.horizontalLayout_2.addWidget(self.radioButton_2)


        self.gridLayout.addLayout(self.horizontalLayout_2, 1, 2, 1, 1)

        self.ProcessPushButton = QPushButton(self.frame)
        self.ProcessPushButton.setObjectName(u"ProcessPushButton")

        self.gridLayout.addWidget(self.ProcessPushButton, 4, 2, 1, 1)


        self.horizontalLayout.addLayout(self.gridLayout)


        self.verticalLayout.addWidget(self.frame)

        MainWindow.setCentralWidget(self.centralwidget)
        QWidget.setTabOrder(self.radioButton, self.radioButton_2)
        QWidget.setTabOrder(self.radioButton_2, self.InputLineEdit)
        QWidget.setTabOrder(self.InputLineEdit, self.InputBrowsePushButton)
        QWidget.setTabOrder(self.InputBrowsePushButton, self.OutputLineEdit)
        QWidget.setTabOrder(self.OutputLineEdit, self.InputBrowsePushButton_2)
        QWidget.setTabOrder(self.InputBrowsePushButton_2, self.ProcessPushButton)

        self.retranslateUi(MainWindow)

        QMetaObject.connectSlotsByName(MainWindow)
    # setupUi

    def retranslateUi(self, MainWindow):
        MainWindow.setWindowTitle(QCoreApplication.translate("MainWindow", u"ConvertBMP", None))
        self.InputBrowsePushButton.setText(QCoreApplication.translate("MainWindow", u"Browse", None))
        self.InputBrowsePushButton_2.setText(QCoreApplication.translate("MainWindow", u"Browse", None))
        self.label.setText(QCoreApplication.translate("MainWindow", u"Input Dir:", None))
        self.label_2.setText(QCoreApplication.translate("MainWindow", u"Ouput Dir:", None))
        self.label_3.setText(QCoreApplication.translate("MainWindow", u"Wio Teminal \"Special\" Bitmap Converter", None))
        self.StatusLabel.setText(QCoreApplication.translate("MainWindow", u"Status:", None))
        self.label_5.setText(QCoreApplication.translate("MainWindow", u"Convert to:", None))
        self.radioButton.setText(QCoreApplication.translate("MainWindow", u"8 Bit Color", None))
        self.radioButton_2.setText(QCoreApplication.translate("MainWindow", u"16 Bit Color", None))
        self.ProcessPushButton.setText(QCoreApplication.translate("MainWindow", u"Process", None))
    # retranslateUi

