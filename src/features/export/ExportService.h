#ifndef EXPORTSERVICE_H
#define EXPORTSERVICE_H

#include <QObject>

class ExportService : public QObject
{
    Q_OBJECT
public:
    explicit ExportService(QObject *parent = nullptr);
};

#endif
