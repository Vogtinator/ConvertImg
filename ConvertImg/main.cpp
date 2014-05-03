#include <cstdint>
#include <iostream>

#include <QFileInfo>
#include <QCoreApplication>
#include <QImage>
#include <QStringList>

#define RGB(r,g,b) (((r&0b11111) << 11) | ((g&0b111111) << 5) | (b&0b11111))

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    if(argc != 2)
    {
        std::cout << argv[0] << " Picture" << std::endl;
        return 1;
    }

    QImage i = QImage(argv[1]);
    i.convertToFormat(QImage::Format_RGB32);

    QStringList lines;
    QFileInfo info(argv[1]);
    lines << ("//Generated from " + info.fileName());

    lines << QString("uint16_t %0_data[] = {").arg(info.baseName());

    for(int y = 0; y < i.height(); y++)
    {
        QString line;
        for(int x = 0; x < i.width(); x++)
        {
            QRgb rgb = i.pixel(x, y);
            float r = float(qRed(rgb)) / 255.0f;
            float g = float(qGreen(rgb)) / 255.0f;
            float b = float(qBlue(rgb)) / 255.0f;
            uint16_t p = RGB(int(r*0b11111), int(g*0b111111), int(b*0b11111));
            line += QString("0x%0, ").arg(p, 0, 16);
        }
        lines << line;
    }

    lines << QString("};");

    lines << QString("TEXTURE ") + info.baseName() + "{";
    lines << QString(".width = %0,").arg(i.width());
    lines << QString(".height = %0,").arg(i.height());
    lines << QString(".bitmap = %0_data };").arg(info.baseName());

    std::cout << lines.join("\n").toStdString();
    
    a.quit();

    return 0;
}
