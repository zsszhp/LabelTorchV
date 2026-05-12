#include "InteractionManager.h"
#include "utils/Log.h"

InteractionManager::InteractionManager(QObject *parent) : QObject(parent)
{
    ltTrace(LT_LOG_ANNOTATION()) << "parent=" << parent;
    ltInfo(LT_LOG_ANNOTATION()) << "InteractionManager initialized (stub)";
}
