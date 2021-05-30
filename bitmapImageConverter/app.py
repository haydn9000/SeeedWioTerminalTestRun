"""
    Main GUI file.
"""

from PySide6.QtWidgets import QApplication, QMainWindow, QFileDialog
from WioTerminalBitmapConverterGUI import Ui_MainWindow
from bmp_converter import BMPProcessingThread
from os.path import expanduser
from PySide6 import QtCore
import sys
import os


class TheApp(QMainWindow, Ui_MainWindow):
    # Desktop path
    userDesktop = expanduser("~") + "\Desktop"

    def __init__(self, window):
        self.setupUi(window)

        # Threads/controllers
        self.dataProcessingWorkerThread = None

        # Connect slots/callbacks
        # self.PushButton.clicked.connect(self.startThreads)


    # ============================================ Functions =================================================
    def startConversion(self):
        rgbType = self.InputLineEdit.currentText()
        saveDirName = self.OutputLineEdit.currentText()

        # Start data processing thread
        self.dataProcessingWorkerThread = BMPProcessingThread(rgbType, saveDirName)

        # Update textBrowser (signal/slot for progress window)
        self.generalAnalysesWorkerThread.message.connect(self.updateTextBrowser)
        self.generalAnalysesWorkerThread.finished.connect(self.stopRunningThread)


    def stopRunningThreads(self):
        if self.dataProcessingWorkerThread != None:
            self.dataProcessingWorkerThread.stop()
            self.dataProcessingWorkerThread = None


    def updateTextBrowser(self, message):
        print(message)


    def getInputDir():
        pass

    def getOuputDir():
        pass


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