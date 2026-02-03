# The Complete Guide to SDL3 GPU API

The SDL3 GPU API is a modern, explicit graphics API designed to abstract over backends like Vulkan, Metal, and DirectX 12. It requires manual management of resources, synchronization, and command recording, offering high performance and control.

---

## 1. Initialization and Setup

### 1.1 Creating the Device
The entry point is `SDL_CreateGPUDevice`. You must specify the shader formats your application supports (SPIR-V, DXIL, MSL).
*   **Best Practice:** Check `SDL_GetGPUShaderFormats` to determine which backend formats are supported by the driver.
*   **Code Example:**
    ```c
    // Supports Vulkan, DX12, and Metal backends
    SDL_GPUDevice* device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL,
        true, // Debug mode
        NULL  // Properties
    );
    ```
    **

### 1.2 Window Association
To render to a screen, you must claim the window for the GPU device. This sets up the swapchain.
```c
SDL_ClaimWindowForGPUDevice(context->Device, context->Window);
```
**

You can also set swapchain parameters, such as VSync (Present Mode) or color composition (SDR/HDR).
```c
SDL_SetGPUSwapchainParameters(context->Device, context->Window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC);
```
**

---

## 2. Resource Management

### 2.1 Shaders
Shaders are backend-specific. You must load the correct bytecode (SPV, DXIL, MSL) based on the device capabilities.
*   **Entry Points:** Usually "main" for SPIR-V/DXIL and "main0" for MSL.
*   **Stages:** You must define if a shader is Vertex, Fragment, or Compute.
*   **Creation:** Use `SDL_CreateGPUShader` with a struct defining the code pointer, size, entry point, and resource counts (samplers, buffers).

### 2.2 Buffers
Buffers hold vertex data, indices, or compute data.
*   **Creation:** Use `SDL_CreateGPUBuffer`. You must specify the `usage` flags (e.g., `SDL_GPU_BUFFERUSAGE_VERTEX`, `SDL_GPU_BUFFERUSAGE_INDEX`) and the size in bytes.
*   **Best Practice:** Buffers created with `SDL_CreateGPUBuffer` are generally GPU-only. You cannot write to them directly from the CPU. You must use a Transfer Buffer.

### 2.3 Transfer Buffers (Data Upload)
To move data from CPU to GPU, use a `SDL_GPUTransferBuffer`.
1.  **Create:** `SDL_CreateGPUTransferBuffer` with `SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD`.
2.  **Map:** Use `SDL_MapGPUTransferBuffer` to get a pointer, write your data, then `SDL_UnmapGPUTransferBuffer`.
3.  **Upload:** Use a command buffer and a Copy Pass to transfer data from the Transfer Buffer to the GPU Buffer or Texture.

### 2.4 Textures
Textures can be 2D, 3D, Cubemaps, or Arrays.
*   **Creation:** `SDL_CreateGPUTexture`. Define format (e.g., `R8G8B8A8_UNORM`), dimensions, and usage flags (Sampler, Color Target, Compute Storage, etc.).
*   **Texture Types:**
    *   `SDL_GPU_TEXTURETYPE_2D`
    *   `SDL_GPU_TEXTURETYPE_2D_ARRAY`
    *   `SDL_GPU_TEXTURETYPE_CUBE`
    *   `SDL_GPU_TEXTURETYPE_3D`
*   **Mipmaps:** You can generate mipmaps using `SDL_GenerateMipmapsForGPUTexture` after uploading the base level.

### 2.5 Samplers
Samplers define how textures are read (filtering and wrapping).
*   **Creation:** `SDL_CreateGPUSampler`.
*   **Parameters:** Min/Mag filters (Nearest/Linear), Address Modes (Repeat/Clamp), and Anisotropy.

---

## 3. Pipelines (PSO)

State is baked into Pipeline objects to reduce driver overhead.

### 3.1 Graphics Pipelines
Created via `SDL_CreateGPUGraphicsPipeline`. Key components include:
*   **Shaders:** Vertex and Fragment shaders.
*   **Target Info:** The format of the color render targets and blend states.
*   **Vertex Input:** Layout of vertex buffers and attributes (position, UVs, normals).
*   **Primitive Type:** Triangle List, Line List, etc..
*   **Rasterizer State:** Fill mode (Solid/Line), Cull mode (Front/Back/None), Front Face (CW/CCW).
*   **Depth/Stencil:** Depth test enable, write mask, compare ops.
*   **Multisample:** Sample count for MSAA.

### 3.2 Compute Pipelines
Created via `SDL_CreateGPUComputePipeline`.
*   Requires a Compute Shader and specific read-only/read-write resource counts (storage textures/buffers).

---

## 4. The Render Loop and Command Recording

Commands are recorded into a buffer and submitted in batches.

### 4.1 acquiring the Command Buffer
Start the frame by acquiring a command buffer:
```c
SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(context->Device);
```
**

### 4.2 The Swapchain Texture
You must wait for the swapchain to provide a texture to draw into.
```c
SDL_GPUTexture* swapchainTexture;
SDL_WaitAndAcquireGPUSwapchainTexture(cmdbuf, context->Window, &swapchainTexture, NULL, NULL);
```
*   **Pitfall:** This function can return false or a NULL texture if the window is minimized or the swapchain is resetting. Always check for NULL before rendering.

### 4.3 Passes
All actions happen inside "Passes".

#### Copy Pass (Data Transfer)
Used for uploading/downloading data.
```c
SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdbuf);
SDL_UploadToGPUBuffer(copyPass, ...);
SDL_UploadToGPUTexture(copyPass, ...);
SDL_EndGPUCopyPass(copyPass);
```
**

#### Render Pass (Graphics)
Used for drawing. You must define `SDL_GPUColorTargetInfo`.
*   **Load Op:** What happens to the texture at the start (`LOAD` keep content, `CLEAR` clear content, `DONT_CARE`).
*   **Store Op:** What happens at the end (`STORE` save result, `RESOLVE` for MSAA).
*   **Usage:**
    ```c
    SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdbuf, &colorTargetInfo, 1, &depthStencilInfo);
    SDL_BindGPUGraphicsPipeline(renderPass, pipeline);
    SDL_BindGPUVertexBuffers(renderPass, ...);
    SDL_DrawGPUPrimitives(renderPass, vertexCount, instanceCount, firstVertex, firstInstance);
    SDL_EndGPURenderPass(renderPass);
    ```
    **

#### Compute Pass
Used for general-purpose GPU math.
*   Binds storage textures or buffers.
*   Uses `SDL_DispatchGPUCompute(pass, x, y, z)` to launch thread groups.
*   **Sync:** `SDL_EndGPUComputePass` handles memory barriers automatically between passes.

### 4.4 Uniforms
You can push small amounts of uniform data (matrices, floats) directly to the command buffer without creating a dedicated buffer.
```c
SDL_PushGPUVertexUniformData(cmdbuf, slot, &data, size);
SDL_PushGPUFragmentUniformData(cmdbuf, slot, &data, size);
```
**

### 4.5 Submission
Finally, submit the command buffer to the GPU queue.
```c
SDL_SubmitGPUCommandBuffer(cmdbuf);
```
**

---

## 5. Advanced Features

### 5.1 Indirect Draw
Instead of the CPU telling the GPU how many vertices to draw, the GPU reads draw commands from a buffer (`SDL_GPU_BUFFERUSAGE_INDIRECT`). This is useful for GPU-driven rendering or particle systems.
*   Use `SDL_DrawGPUPrimitivesIndirect` or `SDL_DrawGPUIndexedPrimitivesIndirect`.

### 5.2 Instancing
Draw the same geometry multiple times efficiently.
*   Set the `instance_step_rate` in the vertex buffer description.
*   Pass the number of instances to `SDL_DrawGPUPrimitives`.

### 5.3 MSAA (Multisample Anti-Aliasing)
1.  Create a texture with `sample_count > 1`.
2.  Create a pipeline with the same `multisample_state.sample_count`.
3.  In the Render Pass, set the MSAA texture as the target and a standard 1x texture as the `resolve_texture`. Set `store_op` to `SDL_GPU_STOREOP_RESOLVE`.

### 5.4 Readback (Downloads)
To read data back to the CPU (e.g., screenshots or logic):
1.  Create a Transfer Buffer with usage `DOWNLOAD`.
2.  Use `SDL_DownloadFromGPUTexture` in a Copy Pass.
3.  Submit the buffer and wait for a **Fence** to ensure the GPU is finished.

---

## 6. Synchronization (Fences)

When you need the CPU to wait for the GPU (e.g., deleting resources safely, reading back downloads), use Fences.
```c
SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmdbuf);
SDL_WaitForGPUFences(context->Device, true, &fence, 1);
SDL_ReleaseGPUFence(context->Device, fence);
```
**

---

## 7. Pitfalls and Best Practices

### Potential Pitfalls
*   **Coordinate Systems:**
    *   3D projection matrices usually require explicit handling. SDL examples often use a "LookAt" and "Perspective" matrix calculation logic provided in the helper files. Ensure your winding order (`FrontFace`) matches your data (CW vs CCW) or you will cull the wrong faces.
*   **Resource Lifecycles:**
    *   You must manually release shaders, pipelines, textures, samplers, and buffers using `SDL_ReleaseGPU...` functions. Failing to do so causes leaks.
    *   Do not release a resource while it is currently being used by an in-flight command buffer.
*   **Format Support:**
    *   Not all formats are supported on all devices. Use `SDL_GPUTextureSupportsFormat` to check before creating specific depth or compressed textures (e.g., D24 vs D32).

### Best Practices
*   **Auto-Release:** `SDL_ReleaseGPUTransferBuffer` can be called immediately after submission if you don't need to reuse the struct on the CPU side immediately; the driver often tracks usage. However, strictly speaking, ensure the GPU is done or trust SDL's internal reference counting if applicable (the examples release resources at Quit time mostly, but transfer buffers are released after submission in init functions).
*   **UBO Alignment:** Uniform structures often require specific padding (e.g., `float` arrays often need 16-byte alignment depending on the backend shader compiler rules). See the manual padding in `ComputeSpriteBatch.c` structs.
*   **Texture Transfer Layout:** When uploading textures, use `SDL_GPUTextureTransferInfo` and `SDL_GPUTextureRegion`. Ensure your transfer buffer is large enough for the raw bytes.
*   **Cycle:** When reusing a texture/buffer in a loop (like a compute simulation), set `.cycle = true` in the binding info. This tells SDL to handle potential data hazards or double-buffering logic internally if required.

---

## 8. Summary Checklist for Agents

When generating code using `SDL_GPU`:
1.  **Init:** Device -> Window -> Claim.
2.  **Load:** Shaders (check formats).
3.  **Build:** Pipelines (Graphics or Compute).
4.  **Loop:**
    *   Acquire Command Buffer.
    *   Acquire Swapchain Texture.
    *   If uploading: Begin Copy Pass -> Upload -> End.
    *   If drawing: Begin Render Pass -> Bind Pipeline -> Bind Resources -> Draw -> End.
    *   Submit.
5.  **Clean:** Release all created objects at shutdown.
