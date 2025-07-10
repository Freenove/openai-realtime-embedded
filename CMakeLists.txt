cmake_minimum_required(VERSION 3.19)

if(IDF_TARGET STREQUAL linux)
  file(READ privateConfig.json PV_CFG)
  string(JSON PV_WIFI_SSID GET "${PV_CFG}" wifi_ssid)
  string(JSON PV_WIFI_PASSWORD GET "${PV_CFG}" wifi_password)
  string(JSON PV_OPENAI_API_KEY GET "${PV_CFG}" openai_api_key)
  set(ENV{WIFI_SSID} ${PV_WIFI_SSID})
  set(ENV{WIFI_PASSWORD} ${PV_WIFI_PASSWORD})
  set(ENV{OPENAI_API_KEY} ${PV_OPENAI_API_KEY})
  add_compile_definitions(WIFI_SSID="$ENV{WIFI_SSID}")
  add_compile_definitions(WIFI_PASSWORD="$ENV{WIFI_PASSWORD}")
  add_compile_definitions(LOG_DATACHANNEL_MESSAGES="1")
  if(NOT DEFINED ENV{OPENAI_API_KEY})
    message(FATAL_ERROR "Env variable OPENAI_API_KEY must be set")
  endif()
  add_compile_definitions(OPENAI_API_KEY="$ENV{OPENAI_API_KEY}")
	add_compile_definitions(LINUX_BUILD=1)
  list(APPEND EXTRA_COMPONENT_DIRS
    $ENV{IDF_PATH}/examples/protocols/linux_stubs/esp_stubs
    "components/esp-protocols/common_components/linux_compat/esp_timer"
    "components/esp-protocols/common_components/linux_compat/freertos"
    )
endif()

add_compile_definitions(OPENAI_REALTIMEAPI="https://api.openai.com/v1/realtime?model=gpt-4o-mini-realtime-preview-2024-12-17")

set(COMPONENTS src)
set(EXTRA_COMPONENT_DIRS "src" "components/srtp" "components/peer" "components/esp-libopus")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(src)
