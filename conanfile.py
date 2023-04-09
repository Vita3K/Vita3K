# Conan configuration file
# Conan Python scripts are used for further customization than what conanfile.txt allows
# More info:
# https://docs.conan.io/en/latest/mastering/conanfile_py.html
# https://docs.conan.io/en/latest/reference/conanfile.html
# https://docs.conan.io/en/latest/reference/conanfile_txt.html

from conan import ConanFile


class Vita3K(ConanFile):
    """ Main Python class describing all the project settings related to Conan. """

    # Imported compilation settings
    # These will be appended to the Python object by Conan when Conan is run and
    # their values will be set from those of the Conan profile file
    settings = "os", "compiler", "build_type", "arch"

    # Conan generators to use with the project
    # https://docs.conan.io/en/latest/reference/generators.html
    generators = "CMakeDeps"

    # List of packages required by the project
    def requirements(self):
        self.requires("boost/1.81.0")
        # self.requires("ffmpeg/4.4")

    # Options used to compile the dependencies organized in a Python dictionary
    default_options = {
        # ------ Boost ------
        "boost/*:shared": False,
        "boost/*:zlib": False,
        "boost/*:zstd": False,
        "boost/*:bzip2": False,
        "boost/*:lzma": False,
        "boost/*:system_use_utf8": False,
        "boost/*:fPIC": True,
        # Set compiled library names and layout to the b2 default for the system
        # Needed to make libtomcrypt (from psvpfsparser) link successfully in Windows
        # "boost/*:layout": "b2-default",

        # Pick only the needed Boost components
        # variant and icl are needed as well but they are always built anyway
        "boost/*:without_chrono": True,
        "boost/*:without_container": True,
        "boost/*:without_context": True,
        "boost/*:without_contract": True,
        "boost/*:without_coroutine": True,
        "boost/*:without_date_time": True,
        "boost/*:without_exception": True,
        "boost/*:without_fiber": True,
        "boost/*:without_filesystem": False,
        "boost/*:without_graph": True,
        "boost/*:without_graph_parallel": True,
        "boost/*:without_iostreams": True,
        "boost/*:without_json": True,
        "boost/*:without_locale": True,
        "boost/*:without_log": True,
        "boost/*:without_math": True,
        "boost/*:without_mpi": True,
        "boost/*:without_nowide": True,
        "boost/*:without_program_options": False,
        "boost/*:without_python": True,
        "boost/*:without_random": True,
        "boost/*:without_regex": True,
        "boost/*:without_serialization": True,
        "boost/*:without_stacktrace": True,
        "boost/*:without_system": False,
        "boost/*:without_test": True,
        "boost/*:without_thread": True,
        "boost/*:without_timer": True,
        "boost/*:without_type_erasure": True,
        "boost/*:without_wave": True,

        # ------ FFmpeg ------
        # LICENSING-RELATED
        # These options are needed with our config in order to license FFmpeg under LGPL v2.1+
        # More info:
        # https://github.com/FFmpeg/FFmpeg/blob/master/LICENSE.md
        # https://ffmpeg.org/legal.html
        "ffmpeg/*:shared": True,
        "ffmpeg/*:avfilter": False,
        "ffmpeg/*:with_libx264": False,
        "ffmpeg/*:with_libx265": False,

        # Component options
        "ffmpeg/*:avdevice": False,
        "ffmpeg/*:disable_all_decoders": True,
        "ffmpeg/*:disable_all_encoders": True,
        "ffmpeg/*:disable_all_filters": True,
        "ffmpeg/*:disable_all_hardware_accelerators": True,
        "ffmpeg/*:disable_all_input_devices": True,
        "ffmpeg/*:disable_all_muxers": True,
        "ffmpeg/*:disable_all_output_devices": True,
        "ffmpeg/*:disable_all_parsers": True,
        "ffmpeg/*:disable_all_protocols": True,
        "ffmpeg/*:disable_everything": True,

        # FFmpeg enable/disable lists accept a unique string as value
        # This string is expected to be a comma-separated list with the names
        # of the components to enable or disable
        # Example "item1, item2, item3"
        "ffmpeg/*:enable_decoders": "aac, aac_latm, atrac3, atrac3p, atrac9, mp3, pcm_s16le, pcm_s8, raw, mov, h264, mpeg4, mpeg2video, mjpeg, mjpegb",
        "ffmpeg/*:enable_encoders": "pcm_s16le, ffv1, mpeg4",
        "ffmpeg/*:enable_demuxers": "h264, m4v, mp3, mpegvideo, mpegps, mjpeg, mov, avi, aac, pmp, oma, pcm_s16le, pcm_s8, wav",
        "ffmpeg/*:enable_hardware_accelerators": "h264_dxva2",
        "ffmpeg/*:enable_input_devices": "dshow",
        "ffmpeg/*:enable_muxers": "avi",
        "ffmpeg/*:enable_parsers": "h264, mpeg4video, mpegaudio, mpegvideo, mjpeg, aac, aac_latm",
        "ffmpeg/*:enable_protocols": "file",
        "ffmpeg/*:postproc": False,
        "ffmpeg/*:with_asm": False,
        "ffmpeg/*:with_bzip2": False,
        "ffmpeg/*:with_freetype": False,
        "ffmpeg/*:with_libfdk_aac": False,
        "ffmpeg/*:with_libiconv": False,
        "ffmpeg/*:with_lzma": False,
        "ffmpeg/*:with_programs": False,
        "ffmpeg/*:with_ssl": False,

        "ffmpeg/*:with_libalsa": False,
        "ffmpeg/*:with_pulse": False,
        "ffmpeg/*:with_vulkan": False,
        "ffmpeg/*:with_xcb": False
    }

    def layout(self):
        # cmake_layout(self) could be used, but it's better to put all <pkg>Config.cmake files in the same folder

        # Folder in which the generated configuration files should be put relative to the output folder
        self.folders.generators = "CMake"
