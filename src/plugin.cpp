// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Nosimo-SN. Part of Pulse.

// Pulse: timers for SkyrimNet triggers. Four fixed timers fire every N seconds, four random timers fire
// after a fresh random delay between a min and a max each time. Each firing sends an SKSE mod event
// (Pulse_Fixed1..4, Pulse_Random1..4), which SkyrimNet triggers can match as a mod_event. Pulse itself
// posts nothing to SkyrimNet; it only reads its own settings from the dashboard.
// Time counts only while playing: not in the main menu, loading screens or menus that pause the game.
// A worker thread paces the ticks and reloads settings; timers advance and events are sent in tasks on
// the game thread.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "SkyrimNet/PublicAPI.h"

namespace
{
	constexpr auto kConfigName = "Pulse";
	constexpr auto kTickInterval = 100ms;
	constexpr int  kReloadEveryTicks = 50;  // settings reload every ~5 s
	constexpr auto kMaxStep = 0.5;          // seconds; a longer gap (hitch, alt-tab) counts as this much
	constexpr int  kMinSeconds = 1;
	constexpr int  kMaxSeconds = 1200;
	constexpr std::size_t kTimerCount = 4;

	using Clock = std::chrono::steady_clock;

	// --- settings --------------------------------------------------------------------------------------

	struct FixedSettings
	{
		bool enabled = false;
		int  interval = 60;
	};

	struct RandomSettings
	{
		bool enabled = false;
		int  min = 30;
		int  max = 120;
	};

	struct Settings
	{
		std::array<FixedSettings, kTimerCount>  fixed{};
		std::array<RandomSettings, kTimerCount> random{};
		bool logFires = false;
	};

	std::mutex g_settingsLock;
	Settings   g_settings;

	std::string ConfigValue(const std::string& a_path, const std::string& a_default)
	{
		if (!PublicGetPluginConfigValue) {
			return a_default;
		}
		return PublicGetPluginConfigValue(kConfigName, a_path.c_str(), a_default.c_str());
	}

	bool ConfigBool(const std::string& a_path, bool a_default)
	{
		auto value = ConfigValue(a_path, a_default ? "true" : "false");
		std::ranges::transform(value, value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return value == "true" || value == "1" || value == "yes" || value == "on";
	}

	// Anything unparsable keeps the default; everything is clamped to 1..1200 seconds.
	int ConfigSeconds(const std::string& a_path, int a_default)
	{
		const auto value = ConfigValue(a_path, std::to_string(a_default));
		double     parsed = a_default;
		try {
			parsed = std::stod(value);
		} catch (...) {
			parsed = a_default;
		}
		return std::clamp(static_cast<int>(parsed), kMinSeconds, kMaxSeconds);
	}

	void ReloadSettings()
	{
		Settings s;
		if (PublicGetPluginConfigValue) {
			for (std::size_t i = 0; i < kTimerCount; ++i) {
				const auto fixed = "fixed" + std::to_string(i + 1);
				s.fixed[i].enabled = ConfigBool(fixed + ".enabled", s.fixed[i].enabled);
				s.fixed[i].interval = ConfigSeconds(fixed + ".interval", s.fixed[i].interval);

				const auto random = "random" + std::to_string(i + 1);
				s.random[i].enabled = ConfigBool(random + ".enabled", s.random[i].enabled);
				s.random[i].min = ConfigSeconds(random + ".min", s.random[i].min);
				s.random[i].max = ConfigSeconds(random + ".max", s.random[i].max);
				if (s.random[i].min > s.random[i].max) {
					std::swap(s.random[i].min, s.random[i].max);
				}
			}
			s.logFires = ConfigBool("debug.log_fires", s.logFires);
		}
		std::scoped_lock lock(g_settingsLock);
		g_settings = s;
	}

	Settings SnapshotSettings()
	{
		std::scoped_lock lock(g_settingsLock);
		return g_settings;
	}

	// --- timers (game thread only) ---------------------------------------------------------------------

	struct FixedTimer
	{
		double elapsed = 0.0;
	};

	struct RandomTimer
	{
		bool   wasEnabled = false;
		double elapsed = 0.0;
		double delay = 0.0;  // the current draw
		int    drawnMin = 0;
		int    drawnMax = 0;
	};

	std::array<FixedTimer, kTimerCount>  g_fixed{};
	std::array<RandomTimer, kTimerCount> g_random{};
	std::optional<Clock::time_point>     g_lastTick;
	std::mt19937                         g_rng{ std::random_device{}() };

	std::atomic_bool g_inGame{ false };
	std::atomic_bool g_tickQueued{ false };
	std::atomic_bool g_resetPending{ false };

	void Draw(RandomTimer& a_timer, const RandomSettings& a_settings)
	{
		std::uniform_real_distribution<double> dist(a_settings.min, a_settings.max);
		a_timer.delay = a_settings.min == a_settings.max ? a_settings.min : dist(g_rng);
		a_timer.drawnMin = a_settings.min;
		a_timer.drawnMax = a_settings.max;
	}

	void ResetTimers()
	{
		g_fixed = {};
		g_random = {};
		g_lastTick.reset();
	}

	void Send(const std::string& a_name, std::string_view a_kind, double a_seconds, bool a_log)
	{
		const auto source = SKSE::GetModCallbackEventSource();
		const auto player = RE::PlayerCharacter::GetSingleton();
		if (!source || !player) {
			return;
		}
		// Rounded to 0.1 s so a trigger comparing num_arg sees a tidy number.
		const auto seconds = static_cast<float>(std::round(a_seconds * 10.0) / 10.0);
		SKSE::ModCallbackEvent event{ a_name.c_str(), RE::BSFixedString(a_kind), seconds, player };
		source->SendEvent(&event);
		if (a_log) {
			logs::info("{} fired ({} s)", a_name, seconds);
		}
	}

	bool Counting()
	{
		if (!g_inGame) {
			return false;
		}
		const auto ui = RE::UI::GetSingleton();
		return ui && !ui->GameIsPaused() && !ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME);
	}

	void Tick()
	{
		g_tickQueued = false;
		if (g_resetPending.exchange(false)) {
			ResetTimers();
		}

		const auto now = Clock::now();
		const auto last = g_lastTick;
		g_lastTick = now;
		if (!last || !Counting()) {
			return;
		}
		const double step = (std::min)(std::chrono::duration<double>(now - *last).count(), kMaxStep);
		const auto   s = SnapshotSettings();

		for (std::size_t i = 0; i < kTimerCount; ++i) {
			// A timer switched off forgets its progress, so switching it back on starts a fresh count.
			auto& fixed = g_fixed[i];
			const auto& fs = s.fixed[i];
			if (!fs.enabled) {
				fixed = {};
			} else {
				fixed.elapsed += step;
				// Interval changes apply at once: a shorter interval than the time already counted fires now.
				if (fixed.elapsed >= fs.interval) {
					fixed.elapsed = 0.0;
					Send("Pulse_Fixed" + std::to_string(i + 1), "fixed", fs.interval, s.logFires);
				}
			}

			auto& random = g_random[i];
			const auto& rs = s.random[i];
			if (!rs.enabled) {
				random = {};
			} else {
				if (!random.wasEnabled || random.drawnMin != rs.min || random.drawnMax != rs.max) {
					// New range: draw again, keeping the time already counted.
					random.wasEnabled = true;
					Draw(random, rs);
				}
				random.elapsed += step;
				if (random.elapsed >= random.delay) {
					const double fired = random.delay;
					random.elapsed = 0.0;
					Draw(random, rs);
					Send("Pulse_Random" + std::to_string(i + 1), "random", fired, s.logFires);
				}
			}
		}
	}

	// Pace from our own thread: one queued tick at a time, never a self-requeueing task.
	void WorkerLoop()
	{
		int ticks = 0;
		for (;;) {
			std::this_thread::sleep_for(kTickInterval);
			if (++ticks >= kReloadEveryTicks) {
				ticks = 0;
				ReloadSettings();
			}
			if (!g_tickQueued.exchange(true)) {
				SKSE::GetTaskInterface()->AddTask([] { Tick(); });
			}
		}
	}

	// --- lifecycle -------------------------------------------------------------------------------------

	void InitializeLog()
	{
		auto path = logs::log_directory();
		if (!path) {
			SKSE::stl::report_and_fail("Failed to find standard logging directory"sv);
		}
		*path /= "Pulse.log"sv;
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
		auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
		log->set_level(spdlog::level::info);
		log->flush_on(spdlog::level::info);
		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("[%H:%M:%S:%e] [%l] %v"s);
	}

	void LogSettings()
	{
		const auto s = SnapshotSettings();
		for (std::size_t i = 0; i < kTimerCount; ++i) {
			logs::info("Fixed{}: {} every {} s; Random{}: {} {}-{} s", i + 1, s.fixed[i].enabled ? "on" : "off",
				s.fixed[i].interval, i + 1, s.random[i].enabled ? "on" : "off", s.random[i].min, s.random[i].max);
		}
	}

	void OnMessage(SKSE::MessagingInterface::Message* a_msg)
	{
		switch (a_msg->type) {
		case SKSE::MessagingInterface::kDataLoaded:
			if (FindFunctions() && PublicGetVersion) {
				logs::info("SkyrimNet API v{}; settings {}", PublicGetVersion(), PublicGetPluginConfigValue ? "yes" : "NO (all timers off)");
			} else {
				logs::warn("SkyrimNet API not found: all timers stay off");
			}
			ReloadSettings();
			LogSettings();
			std::thread(WorkerLoop).detach();
			break;
		case SKSE::MessagingInterface::kPreLoadGame:
			g_inGame = false;
			g_resetPending = true;
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
			// The payload is the success bool cast into the pointer value.
			g_inGame = a_msg->data != nullptr;
			break;
		case SKSE::MessagingInterface::kNewGame:
			g_resetPending = true;
			g_inGame = true;
			break;
		default:
			break;
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	InitializeLog();
	SKSE::Init(a_skse, false);
	logs::info("Pulse {} loaded", SKSE::PluginDeclaration::GetSingleton()->GetVersion().string("."));

	if (!SKSE::GetMessagingInterface()->RegisterListener(OnMessage)) {
		logs::critical("Could not register the SKSE message listener");
		return false;
	}
	return true;
}
