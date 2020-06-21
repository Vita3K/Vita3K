#include <CLI11.hpp>
#include <fstream>
#include <shader/spirv_recompiler.h>

std::ifstream::pos_type filesize(const char *filename) {
    std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
    return in.tellg();
}

int main(int argc, char **argv) {
    CLI::App app{ "Shader utility cli" };

    std::string gxp_file;
    std::string features_file;
    app.add_option("-g,--gxp,gxp", gxp_file, "GXP dump");
    app.add_option("-f,--features,features", features_file, "features.yml");
    CLI11_PARSE(app, argc, argv);

    FeatureState features;
    YAML::Node yy = YAML::LoadFile(features_file);
    features.deserialize(yy);

    auto lastindex = gxp_file.find_last_of(".");
    auto name = gxp_file.substr(0, lastindex);

    std::fstream fh;
    auto size = filesize(gxp_file.c_str());
    fh.open(gxp_file, std::fstream::in | std::fstream::binary);
    std::cout << "program parsing (" << size << ")" << std::endl;

    auto program = static_cast<SceGxmProgram *>(calloc(size, 1));
    fh.read((char *)program, size);

    std::cout << "program parsed" << std::endl;

    std::string spirv_dump;
    std::string disasm_dump;

    std::string glsl = shader::convert_gxp_to_glsl(*program, "hello", features, true, &spirv_dump, &disasm_dump);
    std::cout << "recompiled" << std::endl;

    std::ofstream sfh(name + ".spv");
    sfh << spirv_dump;

    std::ofstream dfh(name + ".dsm");
    dfh << disasm_dump;

    std::ofstream gfh(name + ".glsl");
    gfh << glsl;

    return 0;
}