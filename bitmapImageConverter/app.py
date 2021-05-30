"""
    Main GUI file.
"""

from PySide6.QtWidgets import QApplication, QMainWindow, QFileDialog
from WioTerminalBitmapConverterGUI import Ui_MainWindow
from os.path import expanduser
from PySide6 import QtCore
import bmp_converter
import sys
import os


class TheApp(QMainWindow, Ui_MainWindow):
    # Desktop path
    userDesktop = expanduser("~") + "\Desktop"

    def __init__(self, window):
        self.setupUi(window)

        # Threads/controllers
        self.dataProcessingWorkerThread = None

        self.OutputLineEdit.setText(self.userDesktop)

        # Connect slots/callbacks
        self.InputBrowsePushButton.clicked.connect(self.getInputDir)
        self.OutputBrowsePushButton.clicked.connect(self.getOuputDir)
        self.ProcessPushButton.clicked.connect(self.startConversion)


    # ============================================ Functions =================================================
    def startConversion(self):
        rgbType = self.radioButton.isChecked()
        fileDir = self.InputLineEdit.text()
        saveDirName = self.OutputLineEdit.text()

        # Start data processing thread
        self.dataProcessingWorkerThread = bmp_converter.BMPProcessingThread(rgbType, fileDir, saveDirName)
        self.dataProcessingWorkerThread.start()

        # Update textBrowser (signal/slot for progress window)
        self.dataProcessingWorkerThread.message.connect(self.updateTextBrowser)
        self.dataProcessingWorkerThread.finished.connect(self.stopRunningThread)


    def stopRunningThread(self):
        if self.dataProcessingWorkerThread != None:
            self.dataProcessingWorkerThread.stop()
            self.dataProcessingWorkerThread = None


    def updateTextBrowser(self, message):
        self.StatusLabel.setText(f"Status:\n{message} ✔")


    def getInputDir(self):
        dir = QFileDialog.getExistingDirectory(None, "Select folder", self.userDesktop)

        if dir:
            path = dir.replace("/", "\\")
            self.InputLineEdit.setText(path)


    def getOuputDir(self):
        dir = QFileDialog.getExistingDirectory(None, "Select save folder location", self.userDesktop)

        if dir:
            path = dir.replace("/", "\\")
            self.OutputLineEdit.setText(path)
        

if __name__ == "__main__":
    os.environ["QT_AUTO_SCREEN_SCALE_FACTOR"] = "1"      # For High DPI Displays
    app = QApplication(sys.argv)                         # Create the Qt Application
    app.setAttribute(QtCore.Qt.AA_EnableHighDpiScaling)  # For High DPI Displays
    MainWindow = QMainWindow()

    # Create an instance
    ui = TheApp(MainWindow)

    # Show the window and start the app
    MainWindow.show()
    app.exec()