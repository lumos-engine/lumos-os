#include "lumos/plugins/register_builtins.hpp"

namespace lumos {

IPlugin* create_off_plugin();
IPlugin* create_static_plugin();
IPlugin* create_bias_plugin();
IPlugin* create_rainbow_plugin();
IPlugin* create_hyperhdr_plugin();

void register_builtin_plugins(PluginManager& manager) {
    manager.register_plugin(create_off_plugin);
    manager.register_plugin(create_static_plugin);
    manager.register_plugin(create_bias_plugin);
    manager.register_plugin(create_rainbow_plugin);
    manager.register_plugin(create_hyperhdr_plugin);
}

} // namespace lumos
