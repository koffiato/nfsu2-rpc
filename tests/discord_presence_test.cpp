#include "discord_presence.h"
#include "config.h"

#include <cstdlib>
#include <iostream>
#include <string>

#define CHECK(condition) do { if (!(condition)) { \
  std::cerr << "Line " << __LINE__ << ": " #condition "\n"; std::exit(1); \
} } while (false)

namespace {
  struct FakeCore;
  struct FakeActivityManager : IDiscordActivityManager {
    FakeCore* owner = nullptr;
  };

  struct FakeCore : IDiscordCore {
    FakeActivityManager activityManager{};
    void* callbackData = nullptr;
    void (*callback)(void*, EDiscordResult) = nullptr;
  };

  bool running = false;
  bool insideCallbacks = false;
  bool completeUpdates = true;
  EDiscordResult callbackResult = DiscordResult_Ok;
  EDiscordResult pumpResult = DiscordResult_Ok;
  int creates = 0;
  int destroys = 0;
  int updates = 0;
  DiscordActivity lastActivity{};

  void complete(FakeCore* core, EDiscordResult result) {
    auto callback = core->callback;
    auto data = core->callbackData;
    core->callback = nullptr;
    if (callback) {
      callback(data, result);
    }
  }

  DiscordConnection::TimePoint at(int milliseconds) {
    return DiscordConnection::TimePoint{} + std::chrono::seconds(10)
      + std::chrono::milliseconds(milliseconds);
  }

  void reset() {
    running = false;
    completeUpdates = true;
    callbackResult = DiscordResult_Ok;
    pumpResult = DiscordResult_Ok;
    creates = destroys = updates = 0;
  }

  discord::Activity activity(const char* state) {
    discord::Activity value{};
    value.SetState(state);
    value.SetDetails("Circuit race");
    value.GetAssets().SetLargeImage(IMG_KEY);
    value.GetTimestamps().SetStart(123456);
    return value;
  }

  void testLateStartAndRestart() {
    reset();
    DiscordConnection presence;
    CHECK(!presence.poll(at(0)));
    presence.updateActivity(activity("Offline"), at(0));
    CHECK(updates == 0);
    CHECK(!presence.poll(at(1999)));
    CHECK(creates == 1);
    CHECK(!presence.poll(at(2000)));
    CHECK(creates == 2);
    running = true;
    CHECK(!presence.poll(at(3999)));
    CHECK(presence.poll(at(4000)));
    presence.updateActivity(activity("Racing"), at(4000));
    CHECK(updates == 1);
    CHECK(std::string(lastActivity.state) == "Racing");

    running = false;
    CHECK(!presence.poll(at(4001)));
    CHECK(destroys == 1);
    presence.updateActivity(activity("Disconnected"), at(4001));
    CHECK(updates == 1);
    CHECK(!presence.poll(at(6001)));
    running = true;
    CHECK(presence.poll(at(8001)));
    presence.updateActivity(activity("Exploring"), at(8001));
    CHECK(updates == 2);
    CHECK(std::string(lastActivity.state) == "Exploring");
    CHECK(std::string(lastActivity.assets.large_image) == IMG_KEY);
    CHECK(lastActivity.timestamps.start == 123456);

    for (int cycle = 0; cycle < 3; ++cycle) {
      const int stoppedAt = 8002 + cycle * 2001;
      running = false;
      CHECK(!presence.poll(at(stoppedAt)));
      running = true;
      CHECK(!presence.poll(at(stoppedAt + 1999)));
      CHECK(presence.poll(at(stoppedAt + 2000)));
      presence.updateActivity(activity("Reconnected"), at(stoppedAt + 2000));
      CHECK(updates == 3 + cycle);
      CHECK(lastActivity.timestamps.start == 123456);
    }
  }

  void testUpdatePacing() {
    reset();
    running = true;
    DiscordConnection presence;
    CHECK(presence.poll(at(0)));
    completeUpdates = false;
    presence.updateActivity(activity("Racing"), at(0));
    presence.updateActivity(activity("Racing"), at(6000));
    CHECK(updates == 1); // No second request while the first is pending.
    completeUpdates = true;
    CHECK(presence.poll(at(6001)));
    presence.updateActivity(activity("Racing"), at(6001));
    CHECK(updates == 2);
    CHECK(presence.poll(at(6002)));
    presence.updateActivity(activity("Racing"), at(11000));
    CHECK(updates == 2);
    presence.updateActivity(activity("Racing"), at(11001));
    CHECK(updates == 3);
  }

  void testActivityErrors() {
    for (auto result : {DiscordResult_NotRunning, DiscordResult_ServiceUnavailable,
                        DiscordResult_InternalError, DiscordResult_NotAuthenticated,
                        DiscordResult_NotInstalled, DiscordResult_TransactionAborted}) {
      reset();
      running = true;
      DiscordConnection presence;
      CHECK(presence.poll(at(0)));
      callbackResult = result;
      presence.updateActivity(activity("Racing"), at(0));
      CHECK(!presence.poll(at(1)));
      CHECK(destroys == 1);
      CHECK(!presence.poll(at(2000)));
      callbackResult = DiscordResult_Ok;
      CHECK(presence.poll(at(2001)));
      presence.updateActivity(activity("Recovered"), at(2001));
      CHECK(updates == 2);
    }
    for (auto result : {DiscordResult_RateLimited, DiscordResult_InvalidPayload}) {
      reset();
      running = true;
      DiscordConnection presence;
      CHECK(presence.poll(at(0)));
      callbackResult = result;
      presence.updateActivity(activity("Racing"), at(0));
      CHECK(presence.poll(at(1)));
      CHECK(destroys == 0);
      CHECK(creates == 1);
    }
  }

  void testPumpFailure() {
    reset();
    running = true;
    DiscordConnection presence;
    CHECK(presence.poll(at(0)));
    pumpResult = DiscordResult_ServiceUnavailable;
    CHECK(!presence.poll(at(1)));
    CHECK(destroys == 1);
    CHECK(!presence.poll(at(2000)));
    pumpResult = DiscordResult_Ok;
    CHECK(presence.poll(at(2001)));
  }
}

// Replace only the SDK's C entry point. The tests exercise the real vendored
// C++ wrappers and production connection code without launching Discord.
extern "C" EDiscordResult DiscordCreate(DiscordVersion version,
  DiscordCreateParams* params, IDiscordCore** result) {
  CHECK(version == DISCORD_VERSION);
  CHECK(params->client_id == APP_ID);
  CHECK(params->flags == DiscordCreateFlags_NoRequireDiscord);
  ++creates;
  *result = nullptr;
  if (!running) {
    return DiscordResult_NotRunning;
  }

  auto core = new FakeCore{};
  core->activityManager.owner = core;
  core->destroy = [](IDiscordCore* value) {
    CHECK(!insideCallbacks);
    auto fake = static_cast<FakeCore*>(value);
    complete(fake, DiscordResult_TransactionAborted);
    ++destroys;
    delete fake;
  };
  core->run_callbacks = [](IDiscordCore* value) {
    insideCallbacks = true;
    auto result = running ? pumpResult : DiscordResult_NotRunning;
    if (result == DiscordResult_Ok && completeUpdates) {
      complete(static_cast<FakeCore*>(value), callbackResult);
    }
    insideCallbacks = false;
    return result;
  };
  core->get_activity_manager = [](IDiscordCore* value) {
    return static_cast<IDiscordActivityManager*>(&static_cast<FakeCore*>(value)->activityManager);
  };
  core->activityManager.update_activity = [](IDiscordActivityManager* manager,
    DiscordActivity* activity, void* data, void (*callback)(void*, EDiscordResult)) {
    auto fake = static_cast<FakeActivityManager*>(manager)->owner;
    CHECK(!fake->callback);
    lastActivity = *activity;
    ++updates;
    fake->callbackData = data;
    fake->callback = callback;
  };
  *result = core;
  return DiscordResult_Ok;
}

int main() {
  testLateStartAndRestart();
  testUpdatePacing();
  testActivityErrors();
  testPumpFailure();
  std::cout << "Discord presence lifecycle tests passed.\n";
}
