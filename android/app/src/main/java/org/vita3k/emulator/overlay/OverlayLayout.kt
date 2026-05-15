package org.vita3k.emulator.overlay

data class OverlayLayout(
    val positions: Map<Int, OverlayPosition> = emptyMap()
) {
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
