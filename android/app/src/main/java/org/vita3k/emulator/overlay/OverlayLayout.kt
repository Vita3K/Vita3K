package org.vita3k.emulator.overlay

data class OverlayCustomization(
    val visible: Boolean = true,
    val scalePercent: Int = 100,
    val opacityPercent: Int = 100,
    val sensitivityPercent: Int = 100
) {
    fun normalized() = copy(
        scalePercent = scalePercent.coerceIn(35, 250),
        opacityPercent = opacityPercent.coerceIn(10, 100),
        sensitivityPercent = sensitivityPercent.coerceIn(10, 200)
    )
}
data class OverlayLayout(
    val positions: Map<Int, OverlayPosition> = emptyMap(),
    val customizations: Map<Int, OverlayCustomization> = emptyMap()
) {
    fun customizationFor(buttonType: Int): OverlayCustomization = customizations[buttonType] ?: OverlayCustomization()
    fun withCustomization(buttonType: Int, customization: OverlayCustomization): OverlayLayout =
        copy(customizations = customizations + (buttonType to customization.normalized()))
    fun positionFor(buttonType: Int): OverlayPosition? = positions[buttonType]

    fun withPosition(buttonType: Int, normalizedX: Float, normalizedY: Float): OverlayLayout =
        copy(
            positions = positions + (
                buttonType to OverlayPosition(
                    normalizedX = normalizedX.coerceNormalizedCoordinate(),
                    normalizedY = normalizedY.coerceNormalizedCoordinate()
                )
            )
        )

    fun normalized(): OverlayLayout =
        copy(
            customizations = customizations.mapValues { (_, customization) -> customization.normalized() },
            positions = positions.mapValues { (_, position) ->
                OverlayPosition(
                    normalizedX = position.normalizedX.coerceNormalizedCoordinate(),
                    normalizedY = position.normalizedY.coerceNormalizedCoordinate()
                )
            }
        )
}

data class OverlayState(
    val config: OverlayConfig,
    val layout: OverlayLayout
)

internal fun Float.coerceNormalizedCoordinate(): Float {
    if (!isFinite()) {
        return 0f
    }
    return coerceIn(0f, 1f)
}
