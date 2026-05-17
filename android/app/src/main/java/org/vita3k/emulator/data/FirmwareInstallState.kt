package org.vita3k.emulator.data

enum class FirmwareComponent {
    Preinstalled,
    Main,
    FontPackage
}

data class FirmwareComponentPresence(
    val preinstalled: Boolean = false,
    val main: Boolean = false,
    val fontPackage: Boolean = false
) {
    val hasAnyInstalled: Boolean
        get() = preinstalled || main || fontPackage

    val missingComponents: Set<FirmwareComponent>
        get() = buildSet {
            if (!preinstalled) add(FirmwareComponent.Preinstalled)
            if (!main) add(FirmwareComponent.Main)
            if (!fontPackage) add(FirmwareComponent.FontPackage)
        }

    val missingComponentsInOrder: List<FirmwareComponent>
        get() = buildList {
            if (!preinstalled) add(FirmwareComponent.Preinstalled)
            if (!main) add(FirmwareComponent.Main)
            if (!fontPackage) add(FirmwareComponent.FontPackage)
        }

    val missingRequiredComponentsInOrder: List<FirmwareComponent>
        get() = buildList {
            if (!main) add(FirmwareComponent.Main)
            if (!fontPackage) add(FirmwareComponent.FontPackage)
        }
}

sealed interface FirmwareInstallState {
    val components: FirmwareComponentPresence
    val hasRequiredComponents: Boolean

    val hasAnyInstalled: Boolean
        get() = components.hasAnyInstalled

    val hasMainFirmware: Boolean
        get() = components.main

    val hasFontPackage: Boolean
        get() = components.fontPackage

    val isComplete: Boolean
        get() = this is Complete

    data object Missing : FirmwareInstallState {
        override val components = FirmwareComponentPresence()
        override val hasRequiredComponents = false
    }

    data class Partial(
        override val components: FirmwareComponentPresence,
        override val hasRequiredComponents: Boolean
    ) : FirmwareInstallState

    data class Complete(
        override val components: FirmwareComponentPresence
    ) : FirmwareInstallState {
        override val hasRequiredComponents = true
    }

    companion object {
        private const val PREINSTALLED_MASK = 1 shl 0
        private const val MAIN_MASK = 1 shl 1
        private const val FONT_MASK = 1 shl 2

        fun fromMask(mask: Int): FirmwareInstallState {
            val components = FirmwareComponentPresence(
                preinstalled = mask and PREINSTALLED_MASK != 0,
                main = mask and MAIN_MASK != 0,
                fontPackage = mask and FONT_MASK != 0
            )

            val hasRequiredComponents = components.main && components.fontPackage

            return when {
                !components.hasAnyInstalled -> Missing
                components.missingComponents.isEmpty() -> Complete(components)
                else -> Partial(components, hasRequiredComponents)
            }
        }
    }
}
