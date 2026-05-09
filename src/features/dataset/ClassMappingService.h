#ifndef CLASSMAPPINGSERVICE_H
#define CLASSMAPPINGSERVICE_H

#include <QObject>
#include <QString>

class ClassMappingService : public QObject
{
    Q_OBJECT
public:
    explicit ClassMappingService(QObject *parent = nullptr);
};

#endif
