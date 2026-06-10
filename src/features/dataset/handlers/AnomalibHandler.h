#ifndef ANOMALIBHANDLER_H
#define ANOMALIBHANDLER_H

#include "DatasetFormatHandler.h"

class AnomalibHandler : public DatasetFormatHandler
{
public:
    bool canHandle(const QString &folderPath) const override;
    QVariantMap scanFolder(ImportScanner *scanner, const QString &folderPath) override;
    QVariantMap scan(ImportScanner *scanner, const QString &imageDir, const QString &labelDir) override;
    QString getFormatName() const override { return QStringLiteral("anomaly_unsupervised"); }
    int getPriority() const override { return 5; } // 最高优先级
};

#endif // ANOMALIBHANDLER_H
