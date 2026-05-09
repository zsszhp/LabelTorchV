#ifndef IMPORTSCANNER_H
#define IMPORTSCANNER_H

#include <QObject>
#include <QString>

class ImportScanner : public QObject
{
    Q_OBJECT
public:
    explicit ImportScanner(QObject *parent = nullptr);
};

#endif
