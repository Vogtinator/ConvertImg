#include <cstdint>
#include <iostream>

#include <QFileInfo>
#include <QCoreApplication>
#include <QImage>
#include <QStringList>
#include <QCommandLineParser>
#include <QFile>

constexpr uint16_t toRGB16(QRgb rgb)
{
    return (qRed(rgb) & 0b11111000) << 8 | (qGreen(rgb) & 0b11111100) << 3 | (qBlue(rgb) & 0b11111000) >> 3;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QCoreApplication::setApplicationName("ConvertImg");
    QCoreApplication::setApplicationVersion("0.9");

    //Parse the commandline
    QCommandLineParser parser;

    parser.setApplicationDescription("Convert pictures and images to .h files suitable for various libraries");
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addPositionalArgument("image", "The image to be converted");
    parser.addPositionalArgument("output", "Write to this file instead of stdout (optional)");

    QCommandLineOption opt_format("format", "The format to convert to. Available options: nsdl,ngl,n2dlib", "format");
    QCommandLineOption opt_var_name("var", "The name of the global array", "name");
    QCommandLineOption opt_notstatic("not-static", "Don't make the variables static");
    parser.addOption(opt_format);
    parser.addOption(opt_var_name);
    parser.addOption(opt_notstatic);

    parser.process(app);

    //Verify the arguments
    if(!parser.isSet(opt_format))
    {
        std::cerr << "You have to set the output format!" << std::endl;
        return 1;
    }
    QString format = parser.value(opt_format);
    if(format != "nsdl" && format != "ngl" && format != "n2dlib")
    {
        std::cerr << "Only nsdl, ngl or n2dlib as format allowed!" << std::endl;
        return 1;
    }

    if(parser.positionalArguments().size() == 0)
    {
        std::cerr << "No input file provided!" << std::endl;
        return 1;
    }

    //Finally do stuff: Load the image and convert it to RGB8565
    QFileInfo file_info(parser.positionalArguments()[0]);
    QString var_name = parser.isSet(opt_var_name) ? parser.value(opt_var_name) : file_info.baseName();
    bool notstatic = parser.isSet(opt_notstatic);

    QImage i = QImage(parser.positionalArguments()[0]);
    if(i.isNull())
    {
        std::cerr << "Loading of image '" << parser.positionalArguments()[0].toStdString() << "' failed!" << std::endl;
        return 1;
    }

    //To have the best transparency we need to find a color that is not present in the image
    bool has_transparency = i.hasAlphaChannel();
    i = i.convertToFormat(QImage::Format_ARGB32);

    uint16_t unused_color = 0x0000;
    if(has_transparency || format == "n2dlib")
    {
        bool color_present[0x10000]{false};
        for(unsigned int y = 0; y < static_cast<unsigned int>(i.height()); ++y)
        {
            QRgb *scanline = reinterpret_cast<QRgb*>(i.scanLine(y));
            for(unsigned int x = 0; x < static_cast<unsigned int>(i.width()); ++x, ++scanline)
            {
                if(qAlpha(*scanline) >= 0x80)
                    color_present[toRGB16(*scanline)] = true;
            }
        }

        auto i = std::find(color_present, color_present + 0x10000, false);
        if(i == color_present + 0x10000)
        {
            std::cerr << "Every possible color is present in this image! Transparency not available!" << std::endl;
            return 1;
        }
        else
            unused_color = i - color_present;

        //nGL treats only 0x0000 as transparent, because it's much faster (3d only, 2d unaffected)
        if(format == "ngl" && unused_color != 0x0000)
            std::cerr << "Warning: Transparency won't work if used as 3d texture!" << std::endl;
    }

    //Last but not least output in C array format
    QStringList lines;
    lines << ("//Generated from " + file_info.fileName() + " (output format: " + format + ")");

    QString str_static = "static ";
    if(notstatic)
        str_static = "";

    if(format == "ngl")
        lines << QString("%0uint16_t %1_data[] = {").arg(str_static).arg(var_name);
    else if(format == "nsdl")
        lines << QString("%0uint16_t %0[] = {0x2a01,\n%1,\n%2,\n0x0000,").arg(str_static).arg(var_name).arg(i.width()).arg(i.height());
    else //n2dlib
        lines << QString("%0uint16_t %0[] = {%1,\n%2,\n0x%3,").arg(var_name).arg(str_static).arg(i.width()).arg(i.height()).arg(unused_color, 0, 16);

    for(unsigned int y = 0; y < static_cast<unsigned int>(i.height()); ++y)
    {
        QRgb *scanline = reinterpret_cast<QRgb*>(i.scanLine(y));
        QString line;
        for(unsigned int x = 0; x < static_cast<unsigned int>(i.width()); ++x, ++scanline)
        {
            uint16_t color = (qAlpha(*scanline) >= 0x80) ? toRGB16(*scanline) : unused_color;
            line += QString("0x%0, ").arg(color, 0, 16);
        }
        lines << line;
    }

    lines << QString("};");

    if(format == "ngl")
    {
        lines << str_static + QString("TEXTURE ") + var_name + "{";
        lines << QString(".width = %0,").arg(i.width());
        lines << QString(".height = %0,").arg(i.height());
        lines << QString(".bitmap = %0_data };").arg(var_name);
    }

    if(parser.positionalArguments().size() >= 2)
    {
        QFile output(parser.positionalArguments()[1]);
        output.open(QFile::WriteOnly);
        output.write(lines.join("\n").toUtf8());
    }
    else
        std::cout << lines.join("\n").toStdString();
    
    app.quit();

    return 0;
}
