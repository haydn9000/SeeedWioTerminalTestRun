from PySide6.QtCore import QThread, Signal
from PIL import Image
import os


class ConvertBMP:

    def rgb332(self, r, g, b):
        r = r >> 5
        g = g >> 5
        b = b >> 6
        c = r << 5 | g << 2 | b
        return [c]


    def rgb565(self, r, g, b):
        r = r >> 3
        g = g >> 2
        b = b >> 3
        c = r << 11 | g << 5 | b
        return [c >> 8, c & 0xff]


    def convert(self, rgbType, fileDir, saveDirName):
        saveDirName = os.path.join(fileDir, saveDirName)

        if not os.path.exists(saveDirName):
            os.mkdir(saveDirName)

        for _, _, filesnames in os.walk(fileDir):
            for file in filesnames:
                if (os.path.splitext(file)[-1] != ".bmp"):
                    continue

                im = Image.open(os.path.join(fileDir, file))
                width, height = im.size
                v = [rgbType(r, g, b) for (r, g, b) in im.getdata()]
                b = bytearray()
                b.append(width & 0xff)
                b.append(width >> 8)
                b.append(height & 0xff)
                b.append(height >> 8)

                for pair in v:
                    for i in pair:
                        b.append(i)

                f = open(os.path.join(saveDirName, file), "wb")
                f.write(b)
                f.close()
            break


    def choose(self):
        option = int(input("Enter (1) for 8-bit colour convert, Enter (2) for 16-bit colour convert\n"))

        if option == 1:
            saveDirName = "rgb332"
            return self.rgb332, saveDirName
        elif option == 2:
            saveDirName = "rgb565"
            return self.rgb565, saveDirName
        else:
            print("Invalid input!")


class BMPProcessingThread(QThread):
    message = Signal(str)

    def __init__(self, rgbType, saveDirName, parent=None):
        QThread.__init__(self, parent)

        self.rgbType = rgbType
        self.saveDirName = saveDirName


    """
    - The starting point for the thread. After calling start(), 
      QThreads begin executing in run(). By default, 
      run() starts the event loop by calling exec() 
      and runs a Qt event loop inside the thread.
    - https://doc.qt.io/qt-6/qthread.html#details
    """
    def run(self):
        try:
            self.convertBMP()

        except Exception as e:
            self.message.emit(f"<b style=color:LightCoral;>{e}<br></b>")


    def stop(self):
        self.quit()
        self.wait()
        self.deleteLater()  # May not be needed, added to delete object when finished..


    # Processing function
    def convertBMP(self):
        convert = ConvertBMP()
        rgbType = convert.rgb332 if (self.rgbType == 1) else convert.rgb565
        convert.convert(rgbType, "bmp", self.saveDirName)
        self.message.emit("Process complete")


if __name__ == "__main__":
    convert = ConvertBMP()
    rgbType, saveDirName = convert.choose()
    convert.convert(rgbType, "bmp", saveDirName)
