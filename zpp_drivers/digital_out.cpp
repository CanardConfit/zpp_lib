// Copyright 2025 Haute école d'ingénierie et d'architecture de Fribourg
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/****************************************************************************
 * @file digital_out.cpp
 * @author Serge Ayer <serge.ayer@hefr.ch>
 *
 * @brief CPP class implementation for wrapping zephyr os _gpio used as digital output pin
 *
 * @date 2025-08-31
 * @version 1.0.0
 ***************************************************************************/

#include "zpp_include/digital_out.hpp"

// Zephyr

// our syscalls
#if CONFIG_USERSPACE
#include <zephyr/syscalls/gpio_syscalls.h>

#include "syscalls/gpio_syscalls_driver.h"
#endif

// zpp_lib
#include "zpp_include/zpp_assert.hpp"
#include "zpp_include/zpp_log.hpp"

ZPP_LOG_MODULE_REGISTER(zpp_drivers, CONFIG_ZPP_DRIVERS_LOG_LEVEL);

namespace zpp_lib {

DigitalOut::DigitalOut(PinName pin_name) : DigitalOut(pin_name, false) {}

// _gpio is initialized with an error in default switch case
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
DigitalOut::DigitalOut(PinName pin_name, bool value) {
  switch (pin_name) {
#if HAS_LED0
  case PinName::LED0:
    _gpio = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
    break;
#endif  // HAS_LED0

#if HAS_LED1
  case PinName::LED1:
    _gpio = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
    break;
#endif  // HAS_LED1

  default:
    ZPP_ASSERT(false, "Invalid pinName %d", static_cast<int>(pin_name));
    ZPP_LOG_ERR("Invalid pinName %d", static_cast<int>(pin_name));
    return;
  }

  if (!gpio_is_ready_dt(&_gpio)) {
    ZPP_LOG_ERR("GPIO %s not existing on platform", _gpio.port->name);
    ZPP_ASSERT(false, "GPIO %s not existing on platform", _gpio.port->name);
    return;
  }

  // make sure that we can both read and write to the pin
  int ret = gpio_pin_configure_dt(&_gpio, GPIO_INPUT | GPIO_OUTPUT_ACTIVE);
  if (ret < 0) {
    ZPP_LOG_ERR("Cannot configure GPIO %s as output (%d)", _gpio.port->name, ret);
    ZPP_ASSERT(false, "Cannot configure GPIO %s as output (%d)", _gpio.port->name, ret);
    return;
  }
  ZPP_LOG_DBG("Pin %s initialized", _gpio.port->name);

#if CONFIG_USERSPACE
  _gpio_device = device_get_binding(GPIO_SYSCALL_DRIVER_NAME);
  if (_gpio_device == nullptr) {
    ZPP_LOG_ERR("bad gpio device");
    k_oops();
  }
  k_thread_access_grant(k_current_get(), _gpio_device);
#endif  // CONFIG_USERSPACE

  ZephyrResult res = write(value);
  if (!res) {
    ZPP_LOG_ERR("Cannot write value %d to output (%s): %d", value, _gpio.port->name, static_cast<int>(res.error()));
    ZPP_ASSERT(false, "Cannot write value %d to output (%s): %d", value, _gpio.port->name, static_cast<int>(res.error()));
    return;
  }
}

ZephyrResult DigitalOut::write(bool value) {
  ZephyrResult res;
#if CONFIG_USERSPACE
  auto ret = gpio_syscall_set(_gpio_device, &_gpio, static_cast<int>(value));
#else   // CONFIG_USERSPACE
  auto ret = gpio_pin_set_dt(&_gpio, static_cast<int>(value));
#endif  // CONFIG_USERSPACE
  if (ret != 0) {
    res.assign_error(zephyr_to_zpp_error_code(ret));
    ZPP_LOG_ERR("Cannot set value %d to pin %s", value, _gpio.port->name);
  }
  return res;
}

bool DigitalOut::read() {
#if CONFIG_USERSPACE
  return static_cast<bool>(gpio_syscall_get(_gpio_device, &_gpio));
#else   // CONFIG_USERSPACE
  return static_cast<bool>(gpio_pin_get_dt(&_gpio));
#endif  // CONFIG_USERSPACE
}

#if CONFIG_USERSPACE
void DigitalOut::grant_access(k_tid_t tid) {
  const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
  k_object_access_grant(led.port, tid);
  k_object_access_grant(_gpio.port, tid);
}
#endif  // CONFIG_USERSPACE

}  // namespace zpp_lib
