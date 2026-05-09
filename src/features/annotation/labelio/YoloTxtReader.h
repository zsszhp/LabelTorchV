#ifndef YOLOTXTREADER_H
#define YOLOTXTREADER_H

#include <QObject>
#include <QString>

class YoloTxtReader : public QObject
{
    Q_OBJECT
public:
    explicit YoloTxtReader(QObject *parent = nullptr);
};

#endif
