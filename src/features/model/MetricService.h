#ifndef METRICSERVICE_H
#define METRICSERVICE_H

#include <QObject>

class MetricService : public QObject
{
    Q_OBJECT
public:
    explicit MetricService(QObject *parent = nullptr);
};

#endif
