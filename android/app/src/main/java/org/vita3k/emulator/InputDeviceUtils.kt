package org.vita3k.emulator

import android.view.InputDevice

enum class ControllerFamily {
    Standard,
    Xbox,
    Nintendo
}

data class ConnectedGamepad(
    val deviceId: Int,
    val name: String,
    val family: ControllerFamily
)

object InputDeviceUtils {
    @JvmStatic
    fun hasPhysicalGamepadConnected(): Boolean {
        return getPhysicalGamepads().isNotEmpty()
    }

    @JvmStatic
    fun getPhysicalGamepads(): List<ConnectedGamepad> {
        val controllers = mutableListOf<ConnectedGamepad>()
        val deviceIds = InputDevice.getDeviceIds()
        for (deviceId in deviceIds) {
            val device = InputDevice.getDevice(deviceId) ?: continue
            if (device.isVirtual) {
                continue
            }

            val name = device.name.orEmpty()
            if (name.startsWith("uinput-") || name.startsWith("gf_")) {
                continue
            }

            val sources = device.sources
            val isGamepad = (sources and InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD ||
                (sources and InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK
            if (isGamepad) {
                controllers += ConnectedGamepad(
                    deviceId = deviceId,
                    name = name,
                    family = inferControllerFamily(name)
                )
            }
        }

        return controllers.sortedBy { it.deviceId }
    }

    private fun inferControllerFamily(name: String): ControllerFamily {
        val normalized = name.lowercase()
        return when {
            normalized.contains("xbox") || normalized.contains("x-input") -> ControllerFamily.Xbox
            normalized.contains("switch") || normalized.contains("joy-con") || normalized.contains("nintendo") -> ControllerFamily.Nintendo
            else -> ControllerFamily.Standard
        }
    }
}
