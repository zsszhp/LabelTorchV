#ifndef SNAPSHOTSERVICE_H
#define SNAPSHOTSERVICE_H

#include <QObject>

class SnapshotService : public QObject
{
    Q_OBJECT
public:
    explicit SnapshotService(QObject *parent = nullptr);
};

#endif
