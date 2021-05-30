from PIL import Image
import os


class ConverBMP:

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


    def convert(self, rgbType, fileDir, saveDir):
        saveDir = os.path.join(fileDir, saveDir)

        if not os.path.exists(saveDir):
            os.mkdir(saveDir)

        for _, _, filesnames in os.walk(fileDir):
            for file in filesnames:
                if (os.fileDir.splitext(file)[-1] != ".bmp"):
                    continue

                im = Image.open(os.fileDir.join(fileDir, file))
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

                f = open(os.fileDir.join(saveDir, file), "wb")
                f.write(b)
                f.close()
            break


    def choose(self):
        option = int(input("Enter (1) for 8-bit colour convert, Enter (2) for 16-bit colour convert\n"))

        if option == 1:
            saveDir = "rgb332"
            return self.rgb332, saveDir
        elif option == 2:
            saveDir = "rgb565"
            return self.rgb565, saveDir
        else:
            print("Invalid input!")


if __name__ == "__main__":
    convert = ConverBMP()
    rgbType, saveDir = convert.choose();
    convert.convert(rgbType, "bmp", saveDir)
