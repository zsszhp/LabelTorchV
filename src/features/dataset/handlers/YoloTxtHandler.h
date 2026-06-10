#ifndef YOLOTXTHANDLER_H
#define YOLOTXTHANDLER_H

#include "DatasetFormatHandler.h"

class YoloTxtHandler : public DatasetFormatHandler
{
public:
    bool canHandle(const QString &folderPath) const override;
    QVariantMap scanFolder(ImportScanner *scanner, const QString &folderPath) override;
    QVariantMap scan(ImportScanner *scanner, const QString &imageDir, const QString &labelDir) override;
    QString getFormatName() const override { return QStringLiteral("yolo_txt"); }
    int getPriority() const override { return 50; } // 低优先级，作为兜底

private:
    QVariantMap detectNestedLayout(ImportScanner *scanner, const QString &folderPath);
    QVariantMap detectFlatLayout(ImportScanner *scanner, const QString &folderPath);
};

#endif // YOLOTXTHANDLER_H
