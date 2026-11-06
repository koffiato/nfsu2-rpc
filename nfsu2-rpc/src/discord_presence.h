#pragma once

#include <discord/discord.h>
#include <chrono>
#include <memory>

// Operate this connection only from the game loop thread; the SDK is not threadsafe.
class DiscordConnection {
public:
  using Clock = std::chrono::steady_clock;
  using TimePoint = Clock::time_point;

  bool poll(TimePoint now = Clock::now());
  void updateActivity(const discord::Activity& activity, TimePoint now = Clock::now());

private:
  void disconnect(TimePoint now);

  std::unique_ptr<discord::Core> core_;
  TimePoint nextConnect_{};
  TimePoint nextUpdate_{};
  bool updatePending_ = false;
  bool connectionFailed_ = false;
};
