#ifndef DATASETSERVICE_H
#define DATASETSERVICE_H

#include <QObject>
#include <QString>

class DatasetService : public QObject
{
    Q_OBJECT
public:
    explicit DatasetService(QObject *parent = nullptr);
};

#endif
