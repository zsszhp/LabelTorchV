#ifndef COCOJSONHANDLER_H
#define COCOJSONHANDLER_H

#include "DatasetFormatHandler.h"

class CocoJsonHandler : public DatasetFormatHandler
{
public:
    bool canHandle(const QString &folderPath) const override;
    QVariantMap scanFolder(ImportScanner *scanner, const QString &folderPath) override;
    QVariantMap scan(ImportScanner *scanner, const QString &imageDir, const QString &labelDir) override;
    QString getFormatName() const override { return QStringLiteral("coco_json"); }
    int getPriority() const override { return 20; }

private:
    QVariantMap detectNestedLayout(ImportScanner *scanner, const QString &folderPath);
    QVariantMap detectFlatLayout(ImportScanner *scanner, const QString &folderPath);
};

#endif // COCOJSONHANDLER_H
