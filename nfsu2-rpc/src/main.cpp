#include "../resource.h"
#include "config.h"
#include "discord_presence.h"
#include "utils.h"
#include "lang.h"

#include <discord/discord.h>
#include <json.hpp>

#include <ctime>
#include <string>
#include <windows.h>

namespace {
  DiscordConnection presence;
  discord::Activity activity{};
}

void mainLoopHook() {
  // Keep polling even when Discord was absent at startup or has since closed.
  if (!presence.poll()) {
    return;
  }

#if FRAME_SKIP > 0
  static size_t skippedFrames = 0;

  if (skippedFrames != FRAME_SKIP) {
    skippedFrames++;
    return;
  }

  skippedFrames = 0;
#endif

  activity.SetState(LangManager::getInstance().getStateString().c_str());
  activity.SetDetails(LangManager::getInstance().getDetailsString().c_str());

  presence.updateActivity(activity);
}

void init(HMODULE hModule) {
  try {
    LangManager::getInstance().update(buildLangJsonObj(hModule, MAKEINTRESOURCE(IDR_LANGF)));

    activity.GetAssets().SetLargeImage(IMG_KEY);
    activity.GetAssets().SetLargeText(IMG_TXT);
    activity.GetTimestamps().SetStart(time(0));

    makeCall(0x005814DB, reinterpret_cast<void*>(&mainLoopHook));
  } catch (...) {}
}

// Prevents game from going brrr
DWORD WINAPI threadInit(LPVOID lpParam) {
  init((HMODULE)lpParam);
  return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved) {
  if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(hModule);
    HANDLE thread = CreateThread(0, 0, threadInit, hModule, 0, 0);
    if (thread) {
      CloseHandle(thread);
    }
  }

  return 1;
}
