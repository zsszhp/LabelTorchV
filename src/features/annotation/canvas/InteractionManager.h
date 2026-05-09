#ifndef INTERACTIONMANAGER_H
#define INTERACTIONMANAGER_H

#include <QObject>

class InteractionManager : public QObject
{
    Q_OBJECT
public:
    explicit InteractionManager(QObject *parent = nullptr);
};

#endif
