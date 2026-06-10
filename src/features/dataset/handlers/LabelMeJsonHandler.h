#ifndef LABELMEJSONHANDLER_H
#define LABELMEJSONHANDLER_H

#include "DatasetFormatHandler.h"

class LabelMeJsonHandler : public DatasetFormatHandler
{
public:
    bool canHandle(const QString &folderPath) const override;
    QVariantMap scanFolder(ImportScanner *scanner, const QString &folderPath) override;
    QVariantMap scan(ImportScanner *scanner, const QString &imageDir, const QString &labelDir) override;
    QString getFormatName() const override { return QStringLiteral("labelme_json"); }
    int getPriority() const override { return 15; } // 比 COCO 优先

private:
    QVariantMap detectNestedLayout(ImportScanner *scanner, const QString &folderPath);
    QVariantMap detectFlatLayout(ImportScanner *scanner, const QString &folderPath);
};

#endif // LABELMEJSONHANDLER_H
