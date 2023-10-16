# DDS++

DDS image readers are typically not multiplatform and pull in many extra headers (e.g. the fantastic [gli](https://github.com/g-truc/gli) or [Microsoft DirectXTex](https://github.com/Microsoft/DirectXTex)) This single no-dependency header can decode and encode the DDS headers and compute the mipmap and slice offset of a given DDS texture. It is meant as an aid to provide this information directly to graphics APIs to upload data to the GPU.

## Usage

The code below is not meant to be an exhaustive example on how to create textures from a DDS file, merely a guideline of how you would go about using this in your own engine/API.

```cpp
#include "ddspp.h"

// Load image as a stream of bytes
const char* ddsData = ...

// Decode header and get pointer to initial data
ddspp::Descriptor desc;
ddspp::Result decodeResult = ddspp::decode_header(ddsData, desc);

// Feed to the graphics API
if(decodeResult == ddspp::Success)
{
    const char* initialData = ddsData + desc.headerSize;

    // D3D example
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = desc.format; // Maps directly to D3D
        if(desc.type == ddspp::Texture2D)
        {
            if(desc.arraySize > 1)
            {
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
                srvDesc.Texture2DArray.MipLevels = desc.numMips;
                srvDesc.Texture2DArray.ArraySize = desc.arraySize;
            }
            else
            {
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                srvDesc.Texture2D.MipLevels = desc.mipMapCount;
            }
        }
        // ... Fill in the rest
    }

    // Vulkan Example
    {
        VkImageCreateInfo imageCreateInfo = {};
        imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.pNext = nullptr;
        imageCreateInfo.format = GetVulkanFormat(desc.format); // Translate DXGI format to Vulkan
        imageCreateInfo.extent = { desc.width, desc.height, desc.depth };
        imageCreateInfo.mipLevels = desc.numMips;
        imageCreateInfo.arrayLayers = desc.arraySize;
        if(desc.type == ddspp::Texture2D)
        {
            imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        }
        // ... Fill in the rest
    }
}

```
