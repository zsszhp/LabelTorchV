#ifndef MODELREGISTRY_H
#define MODELREGISTRY_H

#include <QObject>

class ModelRegistry : public QObject
{
    Q_OBJECT
public:
    explicit ModelRegistry(QObject *parent = nullptr);
};

#endif
