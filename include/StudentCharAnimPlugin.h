// Student Character Animation Plugin
// University Character Animation Project
// Based on Arkheon Simulation Technologies SDK

#pragma once

#include <plugin/IPlugin.h>
#include <string>
#include <vector>

namespace arkheon::astsim {
class ModelFactoryRegistry;
}

namespace student::charanim {

class StudentCharAnimPlugin final
    : public arkheon::astlib::IPlugin {
public:
    StudentCharAnimPlugin() = default;
    ~StudentCharAnimPlugin() override = default;

    [[nodiscard]] int getInterfaceVersion() const override;
    [[nodiscard]] arkheon::astlib::PluginMetadata getMetadata() const override;

    void initialize(arkheon::astlib::PluginContext& context) override;
    void tick(double dt) override;
    void shutdown() override;

private:
    bool initialized_  = false;
    bool shutdown_     = false;
    bool animationRegistered_ = false;

    // Target: the NathanHuman animation model registered in N8RO
    std::string modelType_ = "animationModelNathanHuman";
    // All animation codes we registered on
    std::vector<std::string> registeredCodes_;

    arkheon::astsim::ModelFactoryRegistry* modelFactoryRegistry_ = nullptr;
};

} // namespace student::charanim

extern "C" {
ARKHEON_ASTLIB_API arkheon::astlib::IPlugin* create_plugin();
ARKHEON_ASTLIB_API void destroy_plugin(arkheon::astlib::IPlugin* plugin);
ARKHEON_ASTLIB_API const char* get_plugin_signature();
}

