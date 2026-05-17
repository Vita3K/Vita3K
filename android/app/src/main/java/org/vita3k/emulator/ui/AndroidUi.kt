package org.vita3k.emulator.ui

import android.content.Context
import android.hardware.input.InputManager
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import org.vita3k.emulator.ConnectedGamepad
import org.vita3k.emulator.InputDeviceUtils

const val CUSTOM_DRIVER_DOWNLOAD_URL = "https://github.com/K11MCH1/AdrenoToolsDrivers/releases/"

@Composable
fun rememberConnectedGamepads(context: Context): List<ConnectedGamepad> {
    var connectedGamepads by remember { mutableStateOf(InputDeviceUtils.getPhysicalGamepads()) }

    DisposableEffect(context) {
        val inputManager = context.getSystemService(Context.INPUT_SERVICE) as? InputManager
        val listener = object : InputManager.InputDeviceListener {
            private fun refresh() {
                connectedGamepads = InputDeviceUtils.getPhysicalGamepads()
            }

            override fun onInputDeviceAdded(deviceId: Int) = refresh()

            override fun onInputDeviceRemoved(deviceId: Int) = refresh()

            override fun onInputDeviceChanged(deviceId: Int) = refresh()
        }

        connectedGamepads = InputDeviceUtils.getPhysicalGamepads()
        inputManager?.registerInputDeviceListener(listener, null)
        onDispose {
            inputManager?.unregisterInputDeviceListener(listener)
        }
    }

    return connectedGamepads
}
