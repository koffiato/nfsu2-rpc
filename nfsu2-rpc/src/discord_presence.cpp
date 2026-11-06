#include "discord_presence.h"
#include "config.h"

namespace {
  const auto reconnectDelay = std::chrono::seconds(2);
  // The Game SDK permits five activity updates per 20 seconds.
  const auto updateDelay = std::chrono::seconds(5);

  bool isConnectionError(discord::Result result) {
    switch (result) {
    case discord::Result::ServiceUnavailable:
    case discord::Result::InternalError:
    case discord::Result::NotAuthenticated:
    case discord::Result::NotInstalled:
    case discord::Result::NotRunning:
    case discord::Result::TransactionAborted:
      return true;
    default:
      return false;
    }
  }
}

bool DiscordConnection::poll(TimePoint now) {
  if (!core_) {
    if (now < nextConnect_) {
      return false;
    }

    // Never launch Discord. A failed connection is optional and retried later.
    discord::Core* core = nullptr;
    const auto result = discord::Core::Create(APP_ID, DiscordCreateFlags_NoRequireDiscord, &core);
    core_.reset(core);
    if (result != discord::Result::Ok || !core_) {
      disconnect(now);
      return false;
    }
  }

  const auto result = core_->RunCallbacks();
  if (result != discord::Result::Ok || connectionFailed_) {
    // Callbacks may report failure while RunCallbacks is still using the core.
    // Wait until it returns before destroying the SDK instance.
    disconnect(now);
    return false;
  }

  return true;
}

void DiscordConnection::updateActivity(const discord::Activity& activity, TimePoint now) {
  if (!core_ || connectionFailed_ || updatePending_ || now < nextUpdate_) {
    return;
  }

  updatePending_ = true;
  nextUpdate_ = now + updateDelay;
  core_->ActivityManager().UpdateActivity(activity, [this](discord::Result result) {
    updatePending_ = false;
    connectionFailed_ = connectionFailed_ || isConnectionError(result);
  });
}

void DiscordConnection::disconnect(TimePoint now) {
  core_.reset();
  updatePending_ = false;
  connectionFailed_ = false;
  nextConnect_ = now + reconnectDelay;
  nextUpdate_ = TimePoint{};
}
