package org.vita3k.emulator.data

/**
 * Mirrors the JNI-backed emulator settings object.
 *
 * All properties are exposed as @JvmField vars so the native layer can access
 * them directly after constructing this class with the no-arg constructor.
 */
class EmulatorConfig {

    // Core
    @JvmField var modulesMode: Int = 0
    @JvmField var lleModules: Array<String> = emptyArray()
    @JvmField var cpuOpt: Boolean = true

    // GPU
    @JvmField var backendRenderer: String = "Vulkan"
    @JvmField var highAccuracy: Boolean = false
    @JvmField var resolutionMultiplier: Float = 1.0f
    @JvmField var disableSurfaceSync: Boolean = true
    @JvmField var screenFilter: String = "Bilinear"
    @JvmField var memoryMapping: String = "double-buffer"
    @JvmField var vSync: Boolean = true
    @JvmField var anisotropicFiltering: Int = 1
    @JvmField var asyncPipelineCompilation: Boolean = true
    @JvmField var exportTextures: Boolean = false
    @JvmField var importTextures: Boolean = false
    @JvmField var exportAsPng: Boolean = true
    @JvmField var fpsHack: Boolean = false
    @JvmField var turboMode: Boolean = false
    @JvmField var shaderCache: Boolean = true
    @JvmField var spirvShader: Boolean = false
    @JvmField var customDriverName: String = ""

    // Audio
    @JvmField var audioBackend: String = "SDL"
    @JvmField var audioVolume: Int = 100
    @JvmField var ngsEnable: Boolean = true
    @JvmField var disableMotion: Boolean = false
    @JvmField var controllerAnalogMultiplier: Float = 1.0f
    @JvmField var controllerBinds: IntArray = defaultControllerBinds()
    @JvmField var controllerAxisBinds: IntArray = defaultControllerAxisBinds()

    // Camera
    @JvmField var frontCameraType: Int = 2
    @JvmField var frontCameraId: String = ""
    @JvmField var frontCameraImage: String = ""
    @JvmField var frontCameraColor: Long = 0L
    @JvmField var backCameraType: Int = 2
    @JvmField var backCameraId: String = ""
    @JvmField var backCameraImage: String = ""
    @JvmField var backCameraColor: Long = 0L

    // System
    @JvmField var pstvMode: Boolean = false
    @JvmField var showMode: Boolean = false
    @JvmField var demoMode: Boolean = false
    @JvmField var sysButton: Int = 1
    @JvmField var sysLang: Int = 1
    @JvmField var sysDateFormat: Int = 2
    @JvmField var sysTimeFormat: Int = 0
    @JvmField var imeLangs: Long = 4L
    @JvmField var userLang: String = ""

    // Network
    @JvmField var psnSignedIn: Boolean = false
    @JvmField var httpEnable: Boolean = true
    @JvmField var httpTimeoutAttempts: Int = 50
    @JvmField var httpTimeoutSleepMs: Int = 100
    @JvmField var httpReadEndAttempts: Int = 10
    @JvmField var httpReadEndSleepMs: Int = 250
    @JvmField var adhocAddr: Int = 0

    // Debug
    @JvmField var logImports: Boolean = false
    @JvmField var logExports: Boolean = false
    @JvmField var logActiveShaders: Boolean = false
    @JvmField var logUniforms: Boolean = false
    @JvmField var colorSurfaceDebug: Boolean = false
    @JvmField var dumpElfs: Boolean = false
    @JvmField var validationLayer: Boolean = true
    @JvmField var textureCache: Boolean = true
    @JvmField var stretchDisplayArea: Boolean = false
    @JvmField var fullscreenHdResPixelPerfect: Boolean = false
    @JvmField var fileLoadingDelay: Int = 0

    // Emulator
    @JvmField var showLiveAreaScreen: Boolean = false
    @JvmField var showCompileShaders: Boolean = true
    @JvmField var checkForUpdates: Boolean = true
    @JvmField var checkForUpdatesMode: Int = 1
    @JvmField var archiveLog: Boolean = false
    @JvmField var logCompatWarn: Boolean = false
    @JvmField var logLevel: Int = 0
    @JvmField var performanceOverlay: Boolean = false
    @JvmField var performanceOverlayDetail: Int = 0
    @JvmField var performanceOverlayPosition: Int = 0
    @JvmField var screenshotFormat: Int = 1

    fun copy(): EmulatorConfig = EmulatorConfig().also { config ->
        config.modulesMode = modulesMode
        config.lleModules = lleModules.copyOf()
        config.cpuOpt = cpuOpt
        config.backendRenderer = backendRenderer
        config.highAccuracy = highAccuracy
        config.resolutionMultiplier = resolutionMultiplier
        config.disableSurfaceSync = disableSurfaceSync
        config.screenFilter = screenFilter
        config.memoryMapping = memoryMapping
        config.vSync = vSync
        config.anisotropicFiltering = anisotropicFiltering
        config.asyncPipelineCompilation = asyncPipelineCompilation
        config.exportTextures = exportTextures
        config.importTextures = importTextures
        config.exportAsPng = exportAsPng
        config.fpsHack = fpsHack
        config.turboMode = turboMode
        config.shaderCache = shaderCache
        config.spirvShader = spirvShader
        config.customDriverName = customDriverName
        config.audioBackend = audioBackend
        config.audioVolume = audioVolume
        config.ngsEnable = ngsEnable
        config.disableMotion = disableMotion
        config.controllerAnalogMultiplier = controllerAnalogMultiplier
        config.controllerBinds = controllerBinds.copyOf()
        config.controllerAxisBinds = controllerAxisBinds.copyOf()
        config.frontCameraType = frontCameraType
        config.frontCameraId = frontCameraId
        config.frontCameraImage = frontCameraImage
        config.frontCameraColor = frontCameraColor
        config.backCameraType = backCameraType
        config.backCameraId = backCameraId
        config.backCameraImage = backCameraImage
        config.backCameraColor = backCameraColor
        config.pstvMode = pstvMode
        config.showMode = showMode
        config.demoMode = demoMode
        config.sysButton = sysButton
        config.sysLang = sysLang
        config.sysDateFormat = sysDateFormat
        config.sysTimeFormat = sysTimeFormat
        config.imeLangs = imeLangs
        config.userLang = userLang
        config.psnSignedIn = psnSignedIn
        config.httpEnable = httpEnable
        config.httpTimeoutAttempts = httpTimeoutAttempts
        config.httpTimeoutSleepMs = httpTimeoutSleepMs
        config.httpReadEndAttempts = httpReadEndAttempts
        config.httpReadEndSleepMs = httpReadEndSleepMs
        config.adhocAddr = adhocAddr
        config.logImports = logImports
        config.logExports = logExports
        config.logActiveShaders = logActiveShaders
        config.logUniforms = logUniforms
        config.colorSurfaceDebug = colorSurfaceDebug
        config.dumpElfs = dumpElfs
        config.validationLayer = validationLayer
        config.textureCache = textureCache
        config.stretchDisplayArea = stretchDisplayArea
        config.fullscreenHdResPixelPerfect = fullscreenHdResPixelPerfect
        config.fileLoadingDelay = fileLoadingDelay
        config.showLiveAreaScreen = showLiveAreaScreen
        config.showCompileShaders = showCompileShaders
        config.checkForUpdates = checkForUpdates
        config.checkForUpdatesMode = checkForUpdatesMode
        config.archiveLog = archiveLog
        config.logCompatWarn = logCompatWarn
        config.logLevel = logLevel
        config.performanceOverlay = performanceOverlay
        config.performanceOverlayDetail = performanceOverlayDetail
        config.performanceOverlayPosition = performanceOverlayPosition
        config.screenshotFormat = screenshotFormat
    }

    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (other !is EmulatorConfig) return false

        return modulesMode == other.modulesMode &&
            lleModules.contentEquals(other.lleModules) &&
            cpuOpt == other.cpuOpt &&
            backendRenderer == other.backendRenderer &&
            highAccuracy == other.highAccuracy &&
            resolutionMultiplier == other.resolutionMultiplier &&
            disableSurfaceSync == other.disableSurfaceSync &&
            screenFilter == other.screenFilter &&
            memoryMapping == other.memoryMapping &&
            vSync == other.vSync &&
            anisotropicFiltering == other.anisotropicFiltering &&
            asyncPipelineCompilation == other.asyncPipelineCompilation &&
            exportTextures == other.exportTextures &&
            importTextures == other.importTextures &&
            exportAsPng == other.exportAsPng &&
            fpsHack == other.fpsHack &&
            turboMode == other.turboMode &&
            shaderCache == other.shaderCache &&
            spirvShader == other.spirvShader &&
            customDriverName == other.customDriverName &&
            audioBackend == other.audioBackend &&
            audioVolume == other.audioVolume &&
            ngsEnable == other.ngsEnable &&
            disableMotion == other.disableMotion &&
            controllerAnalogMultiplier == other.controllerAnalogMultiplier &&
            controllerBinds.contentEquals(other.controllerBinds) &&
            controllerAxisBinds.contentEquals(other.controllerAxisBinds) &&
            frontCameraType == other.frontCameraType &&
            frontCameraId == other.frontCameraId &&
            frontCameraImage == other.frontCameraImage &&
            frontCameraColor == other.frontCameraColor &&
            backCameraType == other.backCameraType &&
            backCameraId == other.backCameraId &&
            backCameraImage == other.backCameraImage &&
            backCameraColor == other.backCameraColor &&
            pstvMode == other.pstvMode &&
            showMode == other.showMode &&
            demoMode == other.demoMode &&
            sysButton == other.sysButton &&
            sysLang == other.sysLang &&
            sysDateFormat == other.sysDateFormat &&
            sysTimeFormat == other.sysTimeFormat &&
            imeLangs == other.imeLangs &&
            userLang == other.userLang &&
            psnSignedIn == other.psnSignedIn &&
            httpEnable == other.httpEnable &&
            httpTimeoutAttempts == other.httpTimeoutAttempts &&
            httpTimeoutSleepMs == other.httpTimeoutSleepMs &&
            httpReadEndAttempts == other.httpReadEndAttempts &&
            httpReadEndSleepMs == other.httpReadEndSleepMs &&
            adhocAddr == other.adhocAddr &&
            logImports == other.logImports &&
            logExports == other.logExports &&
            logActiveShaders == other.logActiveShaders &&
            logUniforms == other.logUniforms &&
            colorSurfaceDebug == other.colorSurfaceDebug &&
            dumpElfs == other.dumpElfs &&
            validationLayer == other.validationLayer &&
            textureCache == other.textureCache &&
            stretchDisplayArea == other.stretchDisplayArea &&
            fullscreenHdResPixelPerfect == other.fullscreenHdResPixelPerfect &&
            fileLoadingDelay == other.fileLoadingDelay &&
            showLiveAreaScreen == other.showLiveAreaScreen &&
            showCompileShaders == other.showCompileShaders &&
            checkForUpdates == other.checkForUpdates &&
            checkForUpdatesMode == other.checkForUpdatesMode &&
            archiveLog == other.archiveLog &&
            logCompatWarn == other.logCompatWarn &&
            logLevel == other.logLevel &&
            performanceOverlay == other.performanceOverlay &&
            performanceOverlayDetail == other.performanceOverlayDetail &&
            performanceOverlayPosition == other.performanceOverlayPosition &&
            screenshotFormat == other.screenshotFormat
    }

    override fun hashCode(): Int {
        var result = modulesMode
        result = 31 * result + lleModules.contentHashCode()
        result = 31 * result + cpuOpt.hashCode()
        result = 31 * result + backendRenderer.hashCode()
        result = 31 * result + highAccuracy.hashCode()
        result = 31 * result + resolutionMultiplier.hashCode()
        result = 31 * result + disableSurfaceSync.hashCode()
        result = 31 * result + screenFilter.hashCode()
        result = 31 * result + memoryMapping.hashCode()
        result = 31 * result + vSync.hashCode()
        result = 31 * result + anisotropicFiltering
        result = 31 * result + asyncPipelineCompilation.hashCode()
        result = 31 * result + exportTextures.hashCode()
        result = 31 * result + importTextures.hashCode()
        result = 31 * result + exportAsPng.hashCode()
        result = 31 * result + fpsHack.hashCode()
        result = 31 * result + turboMode.hashCode()
        result = 31 * result + shaderCache.hashCode()
        result = 31 * result + spirvShader.hashCode()
        result = 31 * result + customDriverName.hashCode()
        result = 31 * result + audioBackend.hashCode()
        result = 31 * result + audioVolume
        result = 31 * result + ngsEnable.hashCode()
        result = 31 * result + disableMotion.hashCode()
        result = 31 * result + controllerAnalogMultiplier.hashCode()
        result = 31 * result + controllerBinds.contentHashCode()
        result = 31 * result + controllerAxisBinds.contentHashCode()
        result = 31 * result + frontCameraType
        result = 31 * result + frontCameraId.hashCode()
        result = 31 * result + frontCameraImage.hashCode()
        result = 31 * result + frontCameraColor.hashCode()
        result = 31 * result + backCameraType
        result = 31 * result + backCameraId.hashCode()
        result = 31 * result + backCameraImage.hashCode()
        result = 31 * result + backCameraColor.hashCode()
        result = 31 * result + pstvMode.hashCode()
        result = 31 * result + showMode.hashCode()
        result = 31 * result + demoMode.hashCode()
        result = 31 * result + sysButton
        result = 31 * result + sysLang
        result = 31 * result + sysDateFormat
        result = 31 * result + sysTimeFormat
        result = 31 * result + imeLangs.hashCode()
        result = 31 * result + userLang.hashCode()
        result = 31 * result + psnSignedIn.hashCode()
        result = 31 * result + httpEnable.hashCode()
        result = 31 * result + httpTimeoutAttempts
        result = 31 * result + httpTimeoutSleepMs
        result = 31 * result + httpReadEndAttempts
        result = 31 * result + httpReadEndSleepMs
        result = 31 * result + adhocAddr
        result = 31 * result + logImports.hashCode()
        result = 31 * result + logExports.hashCode()
        result = 31 * result + logActiveShaders.hashCode()
        result = 31 * result + logUniforms.hashCode()
        result = 31 * result + colorSurfaceDebug.hashCode()
        result = 31 * result + dumpElfs.hashCode()
        result = 31 * result + validationLayer.hashCode()
        result = 31 * result + textureCache.hashCode()
        result = 31 * result + stretchDisplayArea.hashCode()
        result = 31 * result + fullscreenHdResPixelPerfect.hashCode()
        result = 31 * result + fileLoadingDelay
        result = 31 * result + showLiveAreaScreen.hashCode()
        result = 31 * result + showCompileShaders.hashCode()
        result = 31 * result + checkForUpdates.hashCode()
        result = 31 * result + checkForUpdatesMode
        result = 31 * result + archiveLog.hashCode()
        result = 31 * result + logCompatWarn.hashCode()
        result = 31 * result + logLevel
        result = 31 * result + performanceOverlay.hashCode()
        result = 31 * result + performanceOverlayDetail
        result = 31 * result + performanceOverlayPosition
        result = 31 * result + screenshotFormat
        return result
    }
}

private fun defaultControllerBinds(): IntArray = intArrayOf(
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14
)

private fun defaultControllerAxisBinds(): IntArray = intArrayOf(
    0, 1, 2, 3, 4, 5
)
