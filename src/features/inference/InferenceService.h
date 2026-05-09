#ifndef INFERENCESERVICE_H
#define INFERENCESERVICE_H

#include <QObject>

class InferenceService : public QObject
{
    Q_OBJECT
public:
    explicit InferenceService(QObject *parent = nullptr);
};

#endif
