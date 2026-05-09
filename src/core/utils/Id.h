#ifndef ID_H
#define ID_H

#include <QString>
#include <QUuid>

/**
 * @brief 强类型ID生成
 */
namespace Id {

inline QString generate() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

} // namespace Id

#endif // ID_H
