# Matter is deferred (ESP32-WROOM heap/stability). Source kept for a later bring-up.
# To re-enable:
#   1. Restore SRCS/REQUIRES below (see git history)
#   2. Rename idf_component.yml.deferred → idf_component.yml
#   3. Add lumos_matter to main/ and lumos_api_rest CMakeLists REQUIRES
#   4. Re-wire main.cpp + REST/UI Matter sections
message(STATUS "lumos_matter: deferred — not linked into firmware")
idf_component_register(
    INCLUDE_DIRS "include"
)
