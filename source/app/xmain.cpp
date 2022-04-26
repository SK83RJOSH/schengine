#include "version.h"

#include <CrossWindow/CrossWindow.h>

#if D3D11_SUPPORTED
#include "EngineFactoryD3D11.h"
#endif

#if D3D12_SUPPORTED
#include "EngineFactoryD3D12.h"
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
#include "EngineFactoryOpenGL.h"
#endif

#if VULKAN_SUPPORTED
#include "EngineFactoryVk.h"
#endif

#if METAL_SUPPORTED
#include "EngineFactoryMtl.h"
#endif

#include <NativeWindow.h>
#include <EngineFactory.h>
#include <RefCntAutoPtr.hpp>
#include <RenderDevice.h>
#include <DeviceContext.h>
#include <SwapChain.h>

using namespace Diligent;

static const char* VSSource = R"(
struct PSInput 
{ 
    float4 Pos   : SV_POSITION; 
    float3 Color : COLOR; 
};
void main(in uint VertId : SV_VertexID, out PSInput PSIn) 
{
    float4 Pos[3];
    Pos[0] = float4(-0.5, -0.5, 0.0, 1.0);
    Pos[1] = float4( 0.0, +0.5, 0.0, 1.0);
    Pos[2] = float4(+0.5, -0.5, 0.0, 1.0);
    float3 Col[3];
    Col[0] = float3(1.0, 0.0, 0.0); // red
    Col[1] = float3(0.0, 1.0, 0.0); // green
    Col[2] = float3(0.0, 0.0, 1.0); // blue
    PSIn.Pos   = Pos[VertId];
    PSIn.Color = Col[VertId];
}
)";

static const char* PSSource = R"(
struct PSInput 
{ 
    float4 Pos   : SV_POSITION; 
    float3 Color : COLOR; 
};
struct PSOutput
{ 
    float4 Color : SV_TARGET; 
};
void main(in PSInput PSIn, out PSOutput PSOut)
{
    PSOut.Color = float4(PSIn.Color.rgb, 1.0);
}
)";

void xmain([[maybe_unused]] int argc, [[maybe_unused]] const char** argv)
{
    // Initialize
    xwin::Window window;
    xwin::EventQueue event_queue;

    {
        // Create Window Description
        xwin::WindowDesc window_desc;
        window_desc.name = "The schengine application.";
        window_desc.title = "Schengine App";
        window_desc.visible = true;
        window_desc.resizable = false;
        window_desc.width = 1280;
        window_desc.height = 720;

        if (!window.create(window_desc, event_queue))
        {
            return;
        }
    }

    // Create device, context, and swapchain
    RefCntAutoPtr<IEngineFactory> factory;
    RefCntAutoPtr<IRenderDevice> device;
    RefCntAutoPtr<IDeviceContext> context;
    RefCntAutoPtr<ISwapChain> swap_chain;

    {
#if VULKAN_SUPPORTED
#if EXPLICITLY_LOAD_ENGINE_VK_DLL
        auto GetEngineFactoryVk = LoadGraphicsEngineVk();
#endif

        const auto factory_vk = GetEngineFactoryVk();

        ImmediateContextCreateInfo context_create_info;
        context_create_info.Name = "Graphics";
        context_create_info.QueueId = 0;
        context_create_info.Priority = QUEUE_PRIORITY_HIGH;

        EngineVkCreateInfo vk_create_info;
        vk_create_info.NumImmediateContexts = 1;
        vk_create_info.pImmediateContextInfo = &context_create_info;
        vk_create_info.Features.Tessellation = DEVICE_FEATURE_STATE_ENABLED;
        vk_create_info.Features.ComputeShaders = DEVICE_FEATURE_STATE_OPTIONAL;
        vk_create_info.Features.GeometryShaders = DEVICE_FEATURE_STATE_OPTIONAL;
        vk_create_info.Features.MeshShaders = DEVICE_FEATURE_STATE_OPTIONAL;
        vk_create_info.Features.RayTracing = DEVICE_FEATURE_STATE_OPTIONAL;
        vk_create_info.Features.MultithreadedResourceCreation = DEVICE_FEATURE_STATE_ENABLED;

        factory_vk->CreateDeviceAndContextsVk(vk_create_info, &device, &context);

        if (!device)
        {
            LOG_ERROR_AND_THROW("Unable to initialize vulkan");
        }

        const auto size = window.getWindowSize();

        SwapChainDesc swap_chain_desc;
        swap_chain_desc.Width = size.x;
        swap_chain_desc.Height = size.y;
        swap_chain_desc.DepthBufferFormat = TEX_FORMAT_D24_UNORM_S8_UINT;

#if PLATFORM_WIN32 && XWIN_WIN32
        NativeWindow native_window(window.getHwnd());
#else
#error "Platform does not support windowing"
#endif

        factory_vk->CreateSwapChainVk(device, context, swap_chain_desc, native_window, &swap_chain);

        if (!swap_chain)
        {
            LOG_ERROR_AND_THROW("Unable to create swap chain");
        }

        factory = factory_vk;
#endif        
    }

    // Create pipeline
    RefCntAutoPtr<IShader> vertex_shader;
    RefCntAutoPtr<IShader> pixel_shader;
    RefCntAutoPtr<IPipelineState> pipeline;

    {
        ShaderCreateInfo shader_info;
        shader_info.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        shader_info.UseCombinedTextureSamplers = true;

        // Create a vertex shader
        {
            shader_info.Desc.ShaderType = SHADER_TYPE_VERTEX;
            shader_info.EntryPoint = "main";
            shader_info.Desc.Name = "vertex shader";
            shader_info.Source = VSSource;
            device->CreateShader(shader_info, &vertex_shader);
        }

        // Create a pixel shader
        {
            shader_info.Desc.ShaderType = SHADER_TYPE_PIXEL;
            shader_info.EntryPoint = "main";
            shader_info.Desc.Name = "pixel shader";
            shader_info.Source = PSSource;
            device->CreateShader(shader_info, &pixel_shader);
        }

        // Finally, create the pipeline state
        GraphicsPipelineStateCreateInfo pipline_info;
        pipline_info.pVS = vertex_shader;
        pipline_info.pPS = pixel_shader;

        pipline_info.PSODesc.Name = "default";
        pipline_info.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

        pipline_info.GraphicsPipeline.NumRenderTargets = 1;
        pipline_info.GraphicsPipeline.RTVFormats[0] = swap_chain->GetDesc().ColorBufferFormat;
        pipline_info.GraphicsPipeline.DSVFormat = swap_chain->GetDesc().DepthBufferFormat;
        pipline_info.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        pipline_info.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
        pipline_info.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

        device->CreateGraphicsPipelineState(pipline_info, &pipeline);
    }

    // Engine loop
    bool isRunning = true;

    while (isRunning)
    {
        bool shouldRender = true;

        // Update the event queue
        event_queue.update();

        // Iterate through that queue:
        while (!event_queue.empty())
        {
            const xwin::Event& event = event_queue.front();

            switch (event.type)
            {
            case xwin::EventType::Keyboard:
            {
                auto key = event.data.keyboard.key;
                if (key == xwin::Key::Escape)
                {
                    window.close();
                    shouldRender = false;
                    isRunning = false;
                }
                break;
            }
            case xwin::EventType::DPI:
            {
                auto size = window.getWindowSize();
                swap_chain->Resize(size.x, size.y);
                shouldRender = false;
                break;
            }
            case xwin::EventType::Resize:
            {
                swap_chain->Resize(event.data.resize.width, event.data.resize.height);
                shouldRender = false;
                break;
            }
            case xwin::EventType::Close:
                window.close();
                shouldRender = false;
                isRunning = false;
                break;
            }

            event_queue.pop();
        }

        if (shouldRender)
        {
            constexpr float CLEAR_COLOR[4] = { 0.f, 0.f, 0.f, 1.f };
            constexpr CLEAR_DEPTH_STENCIL_FLAGS CLEAR_FLAGS = CLEAR_DEPTH_FLAG | CLEAR_STENCIL_FLAG;

            auto* rtv = swap_chain->GetCurrentBackBufferRTV();
            auto* dsv = swap_chain->GetDepthBufferDSV();

            context->SetRenderTargets(1, &rtv, dsv, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            context->ClearRenderTarget(rtv, CLEAR_COLOR, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            context->ClearDepthStencil(dsv, CLEAR_FLAGS, 0.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            context->SetPipelineState(pipeline);
            
            DrawAttribs draw_attrs;
            draw_attrs.NumVertices = 3;
            context->Draw(draw_attrs);

            swap_chain->Present();
        }
    }
}