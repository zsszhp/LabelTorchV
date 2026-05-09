#ifndef CANVASCONTROLLER_H
#define CANVASCONTROLLER_H

#include <QObject>

class CanvasController : public QObject
{
    Q_OBJECT
public:
    explicit CanvasController(QObject *parent = nullptr);
};

#endif
