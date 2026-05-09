#ifndef TRAININGSERVICE_H
#define TRAININGSERVICE_H

#include <QObject>

class TrainingService : public QObject
{
    Q_OBJECT
public:
    explicit TrainingService(QObject *parent = nullptr);
};

#endif
