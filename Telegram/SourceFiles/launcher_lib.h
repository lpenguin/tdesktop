#ifndef LAUNCHER_LIB_H
#define LAUNCHER_LIB_H

class UnityLauncher
{

private:
    UnityLauncher(const char* desktopId);
    void* entry;

public:
    static UnityLauncher* create(const char* desktopId);
    void setCountEnabled(bool enabled);
    void setCount(int count);
};

#endif // LAUNCHER_LIB_H
