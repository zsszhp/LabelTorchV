#ifndef ANNOTATIONSERVICE_H
#define ANNOTATIONSERVICE_H

#include <QObject>

class AnnotationService : public QObject
{
    Q_OBJECT
public:
    explicit AnnotationService(QObject *parent = nullptr);
};

#endif
