#include "lumos/plugin/plugin.hpp"

namespace lumos {
namespace {

class OffPlugin final : public IPlugin {
public:
    Result<void> initialize(PluginContext&) override { return Result<void>::ok(); }
    Result<void> start() override { return Result<void>::ok(); }
    Result<void> stop() override { return Result<void>::ok(); }
    void update(float) override {}
    void render(Framebuffer& fb) override { fb.clear(); }
    const PluginDescriptor& descriptor() const override { return desc_; }

private:
    PluginDescriptor desc_{
        .id = "off",
        .name = "Off",
        .icon = "power",
        .is_default = false,
        .parameters = {},
    };
};

} // namespace

IPlugin* create_off_plugin() {
    return new OffPlugin();
}

} // namespace lumos
