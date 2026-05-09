#ifndef YOLOTXTWRITER_H
#define YOLOTXTWRITER_H

#include <QObject>
#include <QString>

class YoloTxtWriter : public QObject
{
    Q_OBJECT
public:
    explicit YoloTxtWriter(QObject *parent = nullptr);
};

#endif
