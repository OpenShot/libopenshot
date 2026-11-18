// SPDX-FileCopyrightText: 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "openshot_catch.h"
#include "Qt/AudioDeviceInitialization.h"

namespace {
// Real JUCE manager and selection logic, deterministic devices without hardware.
struct DeviceState {
    bool fail = false;
    bool silent = false;
    double actual_rate = 0;
    std::vector<std::string> opened;
};
class TestDevice : public juce::AudioIODevice {
    DeviceState& state;
    bool opened = false, playing = false;
    double rate = 0;
    juce::BigInteger outputs;
public:
    TestDevice(juce::String name, DeviceState& state)
        : AudioIODevice(name, "Test audio"), state(state) {}
    juce::StringArray getOutputChannelNames() override { return {"Left", "Right"}; }
    juce::StringArray getInputChannelNames() override { return {}; }
    juce::Array<double> getAvailableSampleRates() override { return {48000, 44100, 22050}; }
    juce::Array<int> getAvailableBufferSizes() override { return {512}; }
    int getDefaultBufferSize() override { return 512; }
    juce::String open(const juce::BigInteger&, const juce::BigInteger& output,
                      double sample_rate, int) override {
        state.opened.push_back(getName().toStdString());
        if (state.fail || getName() == "Broken") return "test open failure";
        opened = true;
        rate = state.actual_rate ? state.actual_rate : sample_rate;
        outputs = state.silent ? juce::BigInteger() : output;
        return {};
    }
    void close() override { opened = false; }
    bool isOpen() override { return opened; }
    void start(juce::AudioIODeviceCallback*) override { playing = true; }
    void stop() override { playing = false; }
    bool isPlaying() override { return playing; }
    juce::String getLastError() override { return {}; }
    int getCurrentBufferSizeSamples() override { return 512; }
    double getCurrentSampleRate() override { return rate; }
    int getCurrentBitDepth() override { return 32; }
    juce::BigInteger getActiveOutputChannels() const override { return outputs; }
    juce::BigInteger getActiveInputChannels() const override { return {}; }
    int getOutputLatencyInSamples() override { return 0; }
    int getInputLatencyInSamples() override { return 0; }
};
class TestDeviceType : public juce::AudioIODeviceType {
    DeviceState& state;
public:
    TestDeviceType(DeviceState& state) : AudioIODeviceType("Test audio"), state(state) {}
    void scanForDevices() override {}
    juce::StringArray getDeviceNames(bool input = false) const override {
        return input ? juce::StringArray() : juce::StringArray{"Default", "Speakers", "Broken"};
    }
    int getDefaultDeviceIndex(bool) const override { return 0; }
    int getIndexOfDevice(juce::AudioIODevice* device, bool) const override {
        return device ? getDeviceNames().indexOf(device->getName()) : -1;
    }
    bool hasSeparateInputsAndOutputs() const override { return true; }
    juce::AudioIODevice* createDevice(const juce::String& output, const juce::String&) override {
        return getDeviceNames().contains(output) ? new TestDevice(output, state) : nullptr;
    }
};
class TestManager : public juce::AudioDeviceManager {
    DeviceState& state;
public:
    TestManager(DeviceState& state) : state(state) {}
    void createAudioDeviceTypes(juce::OwnedArray<juce::AudioIODeviceType>& types) override {
        types.add(new TestDeviceType(state));
    }
};
}

TEST_CASE("Requested output is selected by JUCE", "[audio-initialization]") {
    DeviceState state;
    TestManager manager(state);
    juce::String requested_type = "Test audio";
    SECTION("explicit device type") {}
    SECTION("infer device type from name") { requested_type = ""; }
    auto result = openshot::detail::InitializeAudioDevice(manager,
        {requested_type, "Speakers"}, 48000, 2, 512);
    REQUIRE(result.error.empty());
    REQUIRE(manager.getCurrentAudioDevice() != nullptr);
    CHECK(manager.getCurrentAudioDevice()->getName() == "Speakers");
    CHECK(result.device.name == "Speakers");
    CHECK(result.device.type == "Test audio");
    CHECK(result.sample_rate == 48000);
    CHECK(manager.getCurrentAudioDevice()->getActiveOutputChannels().countNumberOfSetBits() == 2);
}

TEST_CASE("Fallback reports the opened device and its actual rate", "[audio-initialization]") {
    DeviceState state;
    TestManager manager(state);
    SECTION("missing requested device") {
        auto result = openshot::detail::InitializeAudioDevice(manager,
            {"Test audio", "Missing"}, 48000, 2, 512);
        REQUIRE(result.error.empty());
        REQUIRE(manager.getCurrentAudioDevice() != nullptr);
        CHECK(result.device.name == manager.getCurrentAudioDevice()->getName());
        CHECK(result.device.name != "Missing");
        CHECK(result.sample_rate == manager.getCurrentAudioDevice()->getCurrentSampleRate());
    }
    SECTION("requested device fails to open") {
        auto result = openshot::detail::InitializeAudioDevice(manager,
            {"Test audio", "Broken"}, 48000, 2, 512);
        REQUIRE(result.error.empty());
        CHECK(result.device.name == "Default");
        CHECK(result.sample_rate == 48000);
        CHECK(std::find(state.opened.begin(), state.opened.end(), "Broken") != state.opened.end());
    }
    SECTION("no preference uses system default") {
        auto result = openshot::detail::InitializeAudioDevice(manager, {}, 48000, 2, 512);
        REQUIRE(result.error.empty());
        CHECK(result.device.name == "Default");
        CHECK(result.sample_rate == 48000);
    }
    SECTION("supported rate differs from requested rate") {
        auto result = openshot::detail::InitializeAudioDevice(manager,
            {"Test audio", "Speakers"}, 12300, 2, 512);
        REQUIRE(result.error.empty());
        CHECK(result.device.name == "Speakers");
        CHECK(result.sample_rate == 48000);
    }
    SECTION("device negotiates a rate outside the retry list") {
        state.actual_rate = 96000;
        auto result = openshot::detail::InitializeAudioDevice(manager,
            {"Test audio", "Speakers"}, 48000, 2, 512);
        REQUIRE(result.error.empty());
        CHECK(result.sample_rate == 96000);
        CHECK(result.device.name == "Speakers");
    }
}

TEST_CASE("Failures and repeated initialization cannot expose attempted metadata", "[audio-initialization]") {
    DeviceState state;
    TestManager manager(state);
    auto init = [&] { return openshot::detail::InitializeAudioDevice(manager,
        {"Test audio", "Speakers"}, 48000, 2, 512); };
    REQUIRE(init().device.name == "Speakers");
    manager.closeAudioDevice();
    SECTION("open failure") { state.fail = true; }
    SECTION("open device has no active output channels") { state.silent = true; }
    auto failed = init();
    CHECK_FALSE(failed.error.empty());
    CHECK(failed.device.name.isEmpty());
    CHECK(failed.device.type.isEmpty());
    CHECK(failed.sample_rate == 0);
    manager.closeAudioDevice();
    state.fail = state.silent = false;
    auto recovered = init();
    REQUIRE(recovered.error.empty());
    CHECK(recovered.device.name == "Speakers");
    CHECK(recovered.sample_rate == 48000);
}

TEST_CASE("Native device initialization reports verifiable success or failure", "[audio-native]") {
    juce::AudioDeviceManager manager;
    auto result = openshot::detail::InitializeAudioDevice(manager, {}, 48000, 2, 512);
    if (result.error.empty()) {
        auto* device = manager.getCurrentAudioDevice();
        REQUIRE(device != nullptr);
        CHECK(device->isOpen());
        CHECK_FALSE(device->getActiveOutputChannels().isZero());
        CHECK(result.device.name == device->getName());
        CHECK(result.device.type == device->getTypeName());
        CHECK(result.sample_rate == device->getCurrentSampleRate());
        WARN("Native output opened: " << result.device.name << " at " << result.sample_rate);
    } else {
        CHECK(result.device.name.isEmpty());
        CHECK(result.device.type.isEmpty());
        CHECK(result.sample_rate == 0);
        WARN("Native output unavailable; validated failure metadata: " << result.error);
    }
}
