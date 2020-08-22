#include <shader/gxp_parser.h>

using namespace shader::usse;

std::tuple<DataType, std::string> shader::get_parameter_type_store_and_name(const SceGxmParameterType &type) {
    switch (type) {
    case SCE_GXM_PARAMETER_TYPE_F16: {
        return std::make_tuple(DataType::F16, "half");
    }

    case SCE_GXM_PARAMETER_TYPE_U16: {
        return std::make_tuple(DataType::UINT16, "ushort");
    }

    case SCE_GXM_PARAMETER_TYPE_S16: {
        return std::make_tuple(DataType::INT16, "ishort");
    }

    case SCE_GXM_PARAMETER_TYPE_U8: {
        return std::make_tuple(DataType::UINT8, "uchar");
    }

    case SCE_GXM_PARAMETER_TYPE_S8: {
        return std::make_tuple(DataType::INT8, "ichar");
    }

    case SCE_GXM_PARAMETER_TYPE_U32: {
        return std::make_tuple(DataType::UINT32, "uint");
    }
    case SCE_GXM_PARAMETER_TYPE_S32: {
        return std::make_tuple(DataType::INT32, "int");
    }

    default:
        break;
    }
}
