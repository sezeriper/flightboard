#pragma once

#include <SDL3/SDL_gpu.h>

namespace flb
{
namespace gpu
{
struct RenderContext
{
  SDL_GPUGraphicsPipeline* pipeline;
  SDL_GPUSampler* sampler;
  SDL_GPUCommandBuffer* commandBuffer;
  SDL_GPUTexture* swapchainTexture;
  SDL_GPUTexture* depthTexture;
  SDL_GPURenderPass* renderPass;
};

static SDL_GPURenderPass* beginRenderPass(const RenderContext& context)
{
  SDL_GPUColorTargetInfo colorTargetInfo{
    .texture = context.swapchainTexture,
    .clear_color{0.0f, 0.0f, 0.0f, 1.0f},
    .load_op = SDL_GPU_LOADOP_CLEAR,
    .store_op = SDL_GPU_STOREOP_STORE,
  };

  SDL_GPUDepthStencilTargetInfo depthTargetInfo{
    .texture = context.depthTexture,
    .clear_depth = 0.0f, // reversed-z
    .load_op = SDL_GPU_LOADOP_CLEAR,
    .store_op = SDL_GPU_STOREOP_STORE,
  };

  SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(context.commandBuffer, &colorTargetInfo, 1, &depthTargetInfo);

  return renderPass;
}

static void endRenderPass(const RenderContext& context)
{
  SDL_EndGPURenderPass(context.renderPass);
  SDL_SubmitGPUCommandBuffer(context.commandBuffer);
}

static void bindVertexBuffer(const RenderContext& context, SDL_GPUBuffer* buffer)
{
  SDL_GPUBufferBinding vertexBufferBinding{
    .buffer = buffer,
    .offset = 0,
  };
  SDL_BindGPUVertexBuffers(context.renderPass, 0, &vertexBufferBinding, 1);
};

static void bindIndexBuffer(const RenderContext& context, SDL_GPUBuffer* buffer)
{
  SDL_GPUBufferBinding indexBufferBinding{
    .buffer = buffer,
    .offset = 0,
  };
  SDL_BindGPUIndexBuffer(context.renderPass, &indexBufferBinding, SDL_GPU_INDEXELEMENTSIZE_16BIT);
};

static void bindSampler(const RenderContext& context, SDL_GPUTexture* texture)
{
  SDL_GPUTextureSamplerBinding samplerBinding{
    .texture = texture,
    .sampler = context.sampler,
  };
  SDL_BindGPUFragmentSamplers(context.renderPass, 0, &samplerBinding, 1);
}

static void bindPipeline(const RenderContext& context)
{
  SDL_BindGPUGraphicsPipeline(context.renderPass, context.pipeline);
}

} // namespace gpu
} // namespace flb
