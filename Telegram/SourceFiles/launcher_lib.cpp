#define QT_NO_SIGNALS_SLOTS_KEYWORDS
#define QT_KEYWORDS
#undef signals
#include "launcher_lib.h"
#include <unity/unity/unity.h>

UnityLauncher::UnityLauncher(const char* desktopId)
{
    entry = unity_launcher_entry_get_for_desktop_id(desktopId);
}

UnityLauncher *UnityLauncher::create(const char* desktopId)
{
    return new UnityLauncher(desktopId);
}

void UnityLauncher::setCountEnabled(bool enabled)
{
    unity_launcher_entry_set_count_visible((UnityLauncherEntry*)entry, enabled ? 1: 0);
    g_main_context_iteration(NULL, true);

}

void UnityLauncher::setCount(int count)
{
    unity_launcher_entry_set_count((UnityLauncherEntry*)entry, count);
    g_main_context_iteration(NULL, true);

}
