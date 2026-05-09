#ifndef ASSISTEDLABELSERVICE_H
#define ASSISTEDLABELSERVICE_H

#include <QObject>

class AssistedLabelService : public QObject
{
    Q_OBJECT
public:
    explicit AssistedLabelService(QObject *parent = nullptr);
};

#endif
