// Backend nad SDL 3 GPU.
// DVUI: src/backends/sdl3gpu.zig
//
// Vrcholy se převádějí do NDC na CPU (stejně jako v DVUI), takže shadery
// nepotřebují uniformy. Shadery jsou předkompilované shadery z DVUI
// (SPIR-V pro Vulkan, MSL pro Metal, DXIL pro D3D12).
#include "dvui/backend_sdl3gpu.h"

#include <stdlib.h>
#include <string.h>

// Shadery převzaté z DVUI (shared/shaders/) vložené přes C23 #embed. Cesta je
// relativní k tomuto souboru. Zarovnání kvůli SPIR-V (slova po 4 bajtech).
alignas(8) static const unsigned char dvui_shader_vertex_spv[] = {
#embed "../../shaders/default.vertex.spv"
};
static const size_t dvui_shader_vertex_spv_size = sizeof dvui_shader_vertex_spv;
alignas(8) static const unsigned char dvui_shader_vertex_msl[] = {
#embed "../../shaders/default.vertex.msl"
};
static const size_t dvui_shader_vertex_msl_size = sizeof dvui_shader_vertex_msl;
alignas(8) static const unsigned char dvui_shader_vertex_dxil[] = {
#embed "../../shaders/default.vertex.dxil"
};
static const size_t dvui_shader_vertex_dxil_size = sizeof dvui_shader_vertex_dxil;
alignas(8) static const unsigned char dvui_shader_fragment_spv[] = {
#embed "../../shaders/default.fragment.spv"
};
static const size_t dvui_shader_fragment_spv_size = sizeof dvui_shader_fragment_spv;
alignas(8) static const unsigned char dvui_shader_fragment_msl[] = {
#embed "../../shaders/default.fragment.msl"
};
static const size_t dvui_shader_fragment_msl_size = sizeof dvui_shader_fragment_msl;
alignas(8) static const unsigned char dvui_shader_fragment_dxil[] = {
#embed "../../shaders/default.fragment.dxil"
};
static const size_t dvui_shader_fragment_dxil_size = sizeof dvui_shader_fragment_dxil;

// DVUI: sdl3gpu.zig – Vertex (pozice float4 v NDC, barva float4, uv float2)
typedef struct GpuVertex {
    float position[4];
    float color[4];
    float texcoord[2];
} GpuVertex;

typedef struct DrawCall {
    uint32_t index_start;
    uint32_t index_count;
    bool has_scissor;
    SDL_Rect scissor;
} DrawCall;

struct dvui_Sdl3GpuBackend {
    SDL_Window* window;
    SDL_GPUDevice* device;
    SDL_GPUShaderFormat shader_format;
    const char* shader_entrypoint;
    SDL_GPUGraphicsPipeline* pipeline;
    SDL_GPUSampler* sampler;
    SDL_GPUTexture* white_texture;
    dvui_Color clear_color;

    SDL_GPUCommandBuffer* cmd;
    SDL_GPUTexture* swapchain_texture;

    // data snímku na CPU
    GpuVertex* vertices;
    size_t vertices_len, vertices_cap;
    uint32_t* indices;
    size_t indices_len, indices_cap;
    DrawCall* draws;
    size_t draws_len, draws_cap;

    // GPU buffery (rostou podle potřeby)
    SDL_GPUBuffer* vertex_buffer;
    uint32_t vertex_buffer_size;
    SDL_GPUBuffer* index_buffer;
    uint32_t index_buffer_size;
    SDL_GPUTransferBuffer* transfer_buffer;
    uint32_t transfer_buffer_size;

    dvui_SizePhysical last_pixel_size;
    dvui_SizeNatural last_window_size;

    SDL_Cursor* cursors[DVUI_CURSOR_ARROW_W_E + 1];
    dvui_Cursor cursor_last;
    bool cursor_set_once;
};

static void* grow(void* items, size_t* cap, size_t elem, size_t needed) {
    if (needed <= *cap) return items;
    size_t ncap = *cap ? *cap * 2 : 256;
    while (ncap < needed) ncap *= 2;
    items = realloc(items, ncap * elem);
    if (!items) abort();
    *cap = ncap;
    return items;
}

// DVUI: SDLBackend.detectShaderFormat
static void detect_shader_format(dvui_Sdl3GpuBackend* b) {
    const SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(b->device);
    if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        b->shader_format = SDL_GPU_SHADERFORMAT_SPIRV;
        b->shader_entrypoint = "main";
    } else if (formats & SDL_GPU_SHADERFORMAT_MSL) {
        b->shader_format = SDL_GPU_SHADERFORMAT_MSL;
        b->shader_entrypoint = "main0";
    } else if (formats & SDL_GPU_SHADERFORMAT_DXIL) {
        b->shader_format = SDL_GPU_SHADERFORMAT_DXIL;
        b->shader_entrypoint = "main";
    } else {
        SDL_Log("dvui: no supported shader format found");
    }
}

// DVUI: SDLBackend.loadShader
static SDL_GPUShader* load_shader(dvui_Sdl3GpuBackend* b, const unsigned char* code, size_t size, SDL_GPUShaderStage stage, uint32_t num_samplers) {
    const SDL_GPUShaderCreateInfo info = {
        .code_size = size,
        .code = code,
        .entrypoint = b->shader_entrypoint,
        .format = b->shader_format,
        .stage = stage,
        .num_samplers = num_samplers,
    };
    SDL_GPUShader* shader = SDL_CreateGPUShader(b->device, &info);
    if (!shader) SDL_Log("dvui: failed to create shader: %s", SDL_GetError());
    return shader;
}

// DVUI: SDLBackend.createPipeline
static bool create_pipeline(dvui_Sdl3GpuBackend* b) {
    const unsigned char* vcode = nullptr, *fcode = nullptr;
    size_t vsize = 0, fsize = 0;
    switch (b->shader_format) {
    case SDL_GPU_SHADERFORMAT_SPIRV:
        vcode = dvui_shader_vertex_spv, vsize = dvui_shader_vertex_spv_size;
        fcode = dvui_shader_fragment_spv, fsize = dvui_shader_fragment_spv_size;
        break;
    case SDL_GPU_SHADERFORMAT_MSL:
        vcode = dvui_shader_vertex_msl, vsize = dvui_shader_vertex_msl_size;
        fcode = dvui_shader_fragment_msl, fsize = dvui_shader_fragment_msl_size;
        break;
    case SDL_GPU_SHADERFORMAT_DXIL:
        vcode = dvui_shader_vertex_dxil, vsize = dvui_shader_vertex_dxil_size;
        fcode = dvui_shader_fragment_dxil, fsize = dvui_shader_fragment_dxil_size;
        break;
    default: return false;
    }

    // vertex shader: bez samplerů; fragment shader: 1 sampler
    SDL_GPUShader* vs = load_shader(b, vcode, vsize, SDL_GPU_SHADERSTAGE_VERTEX, 0);
    SDL_GPUShader* fs = load_shader(b, fcode, fsize, SDL_GPU_SHADERSTAGE_FRAGMENT, 1);
    if (!vs || !fs) {
        if (vs) SDL_ReleaseGPUShader(b->device, vs);
        if (fs) SDL_ReleaseGPUShader(b->device, fs);
        return false;
    }

    // premultiplikované alfa míchání
    const SDL_GPUColorTargetDescription color_target = {
        .format = SDL_GetGPUSwapchainTextureFormat(b->device, b->window),
        .blend_state = {
            .enable_blend = true,
            .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
            .color_blend_op = SDL_GPU_BLENDOP_ADD,
            .src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
            .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
            .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        },
    };

    const SDL_GPUVertexBufferDescription vbd = {
        .slot = 0,
        .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
        .pitch = sizeof(GpuVertex),
    };
    const SDL_GPUVertexAttribute attrs[3] = {
        {.location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, .offset = offsetof(GpuVertex, position)},
        {.location = 1, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, .offset = offsetof(GpuVertex, color)},
        {.location = 2, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = offsetof(GpuVertex, texcoord)},
    };

    const SDL_GPUGraphicsPipelineCreateInfo info = {
        .vertex_shader = vs,
        .fragment_shader = fs,
        .vertex_input_state = {
            .vertex_buffer_descriptions = &vbd,
            .num_vertex_buffers = 1,
            .vertex_attributes = attrs,
            .num_vertex_attributes = 3,
        },
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .rasterizer_state = {.fill_mode = SDL_GPU_FILLMODE_FILL},
        .target_info = {
            .color_target_descriptions = &color_target,
            .num_color_targets = 1,
        },
    };
    b->pipeline = SDL_CreateGPUGraphicsPipeline(b->device, &info);
    SDL_ReleaseGPUShader(b->device, vs);
    SDL_ReleaseGPUShader(b->device, fs);
    if (!b->pipeline) {
        SDL_Log("dvui: failed to create graphics pipeline: %s", SDL_GetError());
        return false;
    }
    return true;
}

// DVUI: SDLBackend.createWhiteTexture – shader vždy vzorkuje texturu,
// obdélníky používají bílou 1×1.
static bool create_white_texture(dvui_Sdl3GpuBackend* b) {
    b->white_texture = SDL_CreateGPUTexture(b->device, &(SDL_GPUTextureCreateInfo){
                                                           .type = SDL_GPU_TEXTURETYPE_2D,
                                                           .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                                                           .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
                                                           .width = 1,
                                                           .height = 1,
                                                           .layer_count_or_depth = 1,
                                                           .num_levels = 1,
                                                       });
    if (!b->white_texture) return false;

    SDL_GPUTransferBuffer* tb = SDL_CreateGPUTransferBuffer(b->device, &(SDL_GPUTransferBufferCreateInfo){.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = 4});
    if (!tb) return false;
    uint8_t* mapped = SDL_MapGPUTransferBuffer(b->device, tb, false);
    memset(mapped, 255, 4);
    SDL_UnmapGPUTransferBuffer(b->device, tb);

    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(b->device);
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
    SDL_UploadToGPUTexture(copy, &(SDL_GPUTextureTransferInfo){.transfer_buffer = tb, .pixels_per_row = 1, .rows_per_layer = 1},
                           &(SDL_GPUTextureRegion){.texture = b->white_texture, .w = 1, .h = 1, .d = 1}, false);
    SDL_EndGPUCopyPass(copy);
    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    if (fence) {
        SDL_WaitForGPUFences(b->device, true, &fence, 1);
        SDL_ReleaseGPUFence(b->device, fence);
    }
    SDL_ReleaseGPUTransferBuffer(b->device, tb);
    return true;
}

dvui_Sdl3GpuBackend* dvui_sdl3gpu_create(dvui_Sdl3GpuInitOptions options) {
    SDL_SetHint(SDL_HINT_MAC_SCROLL_MOMENTUM, "1");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) return nullptr;

    dvui_Sdl3GpuBackend* b = calloc(1, sizeof *b);
    b->clear_color = options.clear_color ? *options.clear_color : DVUI_RGB(0xf0, 0xf0, 0xf0);

    const SDL_WindowFlags flags = SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE | (options.hidden ? SDL_WINDOW_HIDDEN : 0);
    b->window = SDL_CreateWindow(options.title ? options.title : "dvui", (int)options.size.w, (int)options.size.h, flags);
    if (!b->window) goto fail;

    b->device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL, true, nullptr);
    if (!b->device) goto fail;
    if (!SDL_ClaimWindowForGPUDevice(b->device, b->window)) goto fail;

    // přizpůsobení velikosti okna měřítku displeje (DVUI: initial_scale)
    const float scale = SDL_GetDisplayContentScale(SDL_GetDisplayForWindow(b->window));
    if (scale > 0 && scale != 1.0f) SDL_SetWindowSize(b->window, (int)(scale * options.size.w), (int)(scale * options.size.h));
    if (options.min_size) {
        const float s = scale > 0 ? scale : 1.0f;
        SDL_SetWindowMinimumSize(b->window, (int)(s * options.min_size->w), (int)(s * options.min_size->h));
    }

    if (options.no_vsync) {
        SDL_SetGPUSwapchainParameters(b->device, b->window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_IMMEDIATE);
    }

    detect_shader_format(b);
    if (!create_pipeline(b)) goto fail;

    b->sampler = SDL_CreateGPUSampler(b->device, &(SDL_GPUSamplerCreateInfo){
                                                     .min_filter = SDL_GPU_FILTER_NEAREST,
                                                     .mag_filter = SDL_GPU_FILTER_NEAREST,
                                                     .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
                                                     .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
                                                     .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
                                                     .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
                                                 });
    if (!b->sampler) goto fail;
    if (!create_white_texture(b)) goto fail;

    return b;

fail:
    SDL_Log("dvui: SDL3 GPU backend init failed: %s", SDL_GetError());
    dvui_sdl3gpu_destroy(b);
    return nullptr;
}

void dvui_sdl3gpu_destroy(dvui_Sdl3GpuBackend* b) {
    if (!b) return;
    if (b->device) {
        SDL_WaitForGPUIdle(b->device);
        if (b->vertex_buffer) SDL_ReleaseGPUBuffer(b->device, b->vertex_buffer);
        if (b->index_buffer) SDL_ReleaseGPUBuffer(b->device, b->index_buffer);
        if (b->transfer_buffer) SDL_ReleaseGPUTransferBuffer(b->device, b->transfer_buffer);
        if (b->white_texture) SDL_ReleaseGPUTexture(b->device, b->white_texture);
        if (b->sampler) SDL_ReleaseGPUSampler(b->device, b->sampler);
        if (b->pipeline) SDL_ReleaseGPUGraphicsPipeline(b->device, b->pipeline);
        if (b->window) SDL_ReleaseWindowFromGPUDevice(b->device, b->window);
        SDL_DestroyGPUDevice(b->device);
    }
    for (size_t i = 0; i < sizeof b->cursors / sizeof b->cursors[0]; i++) {
        if (b->cursors[i]) SDL_DestroyCursor(b->cursors[i]);
    }
    if (b->window) SDL_DestroyWindow(b->window);
    free(b->vertices);
    free(b->indices);
    free(b->draws);
    free(b);
    SDL_Quit();
}

SDL_Window* dvui_sdl3gpu_window(dvui_Sdl3GpuBackend* b) { return b->window; }
SDL_GPUDevice* dvui_sdl3gpu_device(dvui_Sdl3GpuBackend* b) { return b->device; }

// ---------------------------------------------------------------------------
// Rozhraní dvui_Backend
// ---------------------------------------------------------------------------

// DVUI: SDLBackend.begin – command buffer a swapchain textura pro snímek
static void be_begin(void* ctx) {
    dvui_Sdl3GpuBackend* b = ctx;
    b->vertices_len = 0;
    b->indices_len = 0;
    b->draws_len = 0;
    b->cmd = SDL_AcquireGPUCommandBuffer(b->device);
    if (!b->cmd) {
        SDL_Log("dvui: failed to acquire GPU command buffer: %s", SDL_GetError());
        return;
    }
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(b->cmd, b->window, &b->swapchain_texture, nullptr, nullptr)) {
        SDL_Log("dvui: failed to acquire swapchain texture: %s", SDL_GetError());
        b->swapchain_texture = nullptr;
    }
}

static bool ensure_buffer(dvui_Sdl3GpuBackend* b, SDL_GPUBuffer** buf, uint32_t* size, uint32_t needed, SDL_GPUBufferUsageFlags usage) {
    if (*buf && *size >= needed) return true;
    uint32_t nsize = *size ? *size : 64 * 1024;
    while (nsize < needed) nsize *= 2;
    if (*buf) SDL_ReleaseGPUBuffer(b->device, *buf);
    *buf = SDL_CreateGPUBuffer(b->device, &(SDL_GPUBufferCreateInfo){.usage = usage, .size = nsize});
    *size = *buf ? nsize : 0;
    return *buf != nullptr;
}

// DVUI: SDLBackend.finishRenderingCurrentTarget – nahraje vrcholy a vykreslí
static void be_end(void* ctx) {
    dvui_Sdl3GpuBackend* b = ctx;
    if (!b->cmd || !b->swapchain_texture) return;

    const uint32_t vbytes = (uint32_t)(b->vertices_len * sizeof(GpuVertex));
    const uint32_t ibytes = (uint32_t)(b->indices_len * sizeof(uint32_t));

    if (b->draws_len > 0) {
        bool ok = ensure_buffer(b, &b->vertex_buffer, &b->vertex_buffer_size, vbytes, SDL_GPU_BUFFERUSAGE_VERTEX) &&
                  ensure_buffer(b, &b->index_buffer, &b->index_buffer_size, ibytes, SDL_GPU_BUFFERUSAGE_INDEX);
        if (ok && (!b->transfer_buffer || b->transfer_buffer_size < vbytes + ibytes)) {
            if (b->transfer_buffer) SDL_ReleaseGPUTransferBuffer(b->device, b->transfer_buffer);
            b->transfer_buffer_size = b->vertex_buffer_size + b->index_buffer_size;
            b->transfer_buffer = SDL_CreateGPUTransferBuffer(b->device, &(SDL_GPUTransferBufferCreateInfo){.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = b->transfer_buffer_size});
            ok = b->transfer_buffer != nullptr;
        }
        if (!ok) {
            SDL_Log("dvui: failed to create GPU buffers: %s", SDL_GetError());
            b->draws_len = 0;
        } else {
            uint8_t* mapped = SDL_MapGPUTransferBuffer(b->device, b->transfer_buffer, true);
            memcpy(mapped, b->vertices, vbytes);
            memcpy(mapped + vbytes, b->indices, ibytes);
            SDL_UnmapGPUTransferBuffer(b->device, b->transfer_buffer);

            // všechna nahrání musí proběhnout před render passem
            SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(b->cmd);
            SDL_UploadToGPUBuffer(copy, &(SDL_GPUTransferBufferLocation){.transfer_buffer = b->transfer_buffer, .offset = 0},
                                  &(SDL_GPUBufferRegion){.buffer = b->vertex_buffer, .offset = 0, .size = vbytes}, true);
            SDL_UploadToGPUBuffer(copy, &(SDL_GPUTransferBufferLocation){.transfer_buffer = b->transfer_buffer, .offset = vbytes},
                                  &(SDL_GPUBufferRegion){.buffer = b->index_buffer, .offset = 0, .size = ibytes}, true);
            SDL_EndGPUCopyPass(copy);
        }
    }

    const SDL_GPUColorTargetInfo color_target = {
        .texture = b->swapchain_texture,
        .clear_color = {b->clear_color.r / 255.0f, b->clear_color.g / 255.0f, b->clear_color.b / 255.0f, 1.0f},
        .load_op = SDL_GPU_LOADOP_CLEAR,
        .store_op = SDL_GPU_STOREOP_STORE,
    };
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(b->cmd, &color_target, 1, nullptr);
    if (!pass) {
        SDL_Log("dvui: failed to begin GPU render pass: %s", SDL_GetError());
        return;
    }

    if (b->draws_len > 0) {
        SDL_BindGPUGraphicsPipeline(pass, b->pipeline);
        SDL_BindGPUVertexBuffers(pass, 0, &(SDL_GPUBufferBinding){.buffer = b->vertex_buffer, .offset = 0}, 1);
        SDL_BindGPUIndexBuffer(pass, &(SDL_GPUBufferBinding){.buffer = b->index_buffer, .offset = 0}, SDL_GPU_INDEXELEMENTSIZE_32BIT);
        SDL_BindGPUFragmentSamplers(pass, 0, &(SDL_GPUTextureSamplerBinding){.texture = b->white_texture, .sampler = b->sampler}, 1);

        const SDL_Rect full = {0, 0, (int)b->last_pixel_size.w, (int)b->last_pixel_size.h};
        for (size_t i = 0; i < b->draws_len; i++) {
            const DrawCall* d = &b->draws[i];
            SDL_SetGPUScissor(pass, d->has_scissor ? &d->scissor : &full);
            SDL_DrawGPUIndexedPrimitives(pass, d->index_count, 1, d->index_start, 0, 0);
        }
    }
    SDL_EndGPURenderPass(pass);
}

static dvui_SizePhysical be_pixel_size(void* ctx) {
    dvui_Sdl3GpuBackend* b = ctx;
    int w, h;
    if (SDL_GetWindowSizeInPixels(b->window, &w, &h)) b->last_pixel_size = (dvui_SizePhysical){(float)w, (float)h};
    return b->last_pixel_size;
}

static dvui_SizeNatural be_window_size(void* ctx) {
    dvui_Sdl3GpuBackend* b = ctx;
    int w, h;
    if (SDL_GetWindowSize(b->window, &w, &h)) b->last_window_size = (dvui_SizeNatural){(float)w, (float)h};
    return b->last_window_size;
}

static float be_content_scale(void* ctx) {
    dvui_Sdl3GpuBackend* b = ctx;
    const float s = SDL_GetDisplayContentScale(SDL_GetDisplayForWindow(b->window));
    return s > 0 ? s : 1.0f;
}

// DVUI: SDLBackend.drawClippedTriangles
static void be_draw(void* ctx, const dvui_Vertex* vtx, size_t vtx_count, const uint32_t* idx, size_t idx_count, const dvui_RectPhysical* clip) {
    dvui_Sdl3GpuBackend* b = ctx;
    const float w = b->last_pixel_size.w > 0 ? b->last_pixel_size.w : 1;
    const float h = b->last_pixel_size.h > 0 ? b->last_pixel_size.h : 1;

    const uint32_t base = (uint32_t)b->vertices_len;
    b->vertices = grow(b->vertices, &b->vertices_cap, sizeof(GpuVertex), b->vertices_len + vtx_count);
    for (size_t i = 0; i < vtx_count; i++) {
        const dvui_Vertex* v = &vtx[i];
        b->vertices[b->vertices_len++] = (GpuVertex){
            .position = {v->pos.x / w * 2 - 1.0f, -(v->pos.y / h * 2 - 1.0f), 0, 1},
            .color = {v->col.r / 255.0f, v->col.g / 255.0f, v->col.b / 255.0f, v->col.a / 255.0f},
            .texcoord = {v->uv[0], v->uv[1]},
        };
    }

    const uint32_t start = (uint32_t)b->indices_len;
    b->indices = grow(b->indices, &b->indices_cap, sizeof(uint32_t), b->indices_len + idx_count);
    for (size_t i = 0; i < idx_count; i++) b->indices[b->indices_len++] = base + idx[i];

    DrawCall d = {.index_start = start, .index_count = (uint32_t)idx_count};
    if (clip) {
        d.has_scissor = true;
        d.scissor = (SDL_Rect){(int)clip->x, (int)clip->y, (int)clip->w, (int)clip->h};
    }
    // sloučíme s předchozím voláním, pokud má stejný ořez
    if (b->draws_len > 0) {
        DrawCall* prev = &b->draws[b->draws_len - 1];
        if (prev->has_scissor == d.has_scissor && prev->index_start + prev->index_count == start &&
            (!d.has_scissor || memcmp(&prev->scissor, &d.scissor, sizeof d.scissor) == 0)) {
            prev->index_count += d.index_count;
            return;
        }
    }
    b->draws = grow(b->draws, &b->draws_cap, sizeof(DrawCall), b->draws_len + 1);
    b->draws[b->draws_len++] = d;
}

// DVUI: SDLBackend.setCursor
static void be_set_cursor(void* ctx, dvui_Cursor cursor) {
    dvui_Sdl3GpuBackend* b = ctx;
    if (b->cursor_set_once && cursor == b->cursor_last) return;
    b->cursor_set_once = true;
    b->cursor_last = cursor;
    SDL_SystemCursor sc = SDL_SYSTEM_CURSOR_DEFAULT;
    switch (cursor) {
    case DVUI_CURSOR_ARROW: sc = SDL_SYSTEM_CURSOR_DEFAULT; break;
    case DVUI_CURSOR_HAND: sc = SDL_SYSTEM_CURSOR_POINTER; break;
    case DVUI_CURSOR_ARROW_ALL: sc = SDL_SYSTEM_CURSOR_MOVE; break;
    case DVUI_CURSOR_ARROW_NW_SE: sc = SDL_SYSTEM_CURSOR_NWSE_RESIZE; break;
    case DVUI_CURSOR_ARROW_NE_SW: sc = SDL_SYSTEM_CURSOR_NESW_RESIZE; break;
    case DVUI_CURSOR_ARROW_N_S: sc = SDL_SYSTEM_CURSOR_NS_RESIZE; break;
    case DVUI_CURSOR_ARROW_W_E: sc = SDL_SYSTEM_CURSOR_EW_RESIZE; break;
    }
    if (!b->cursors[cursor]) b->cursors[cursor] = SDL_CreateSystemCursor(sc);
    if (b->cursors[cursor]) SDL_SetCursor(b->cursors[cursor]);
}

// DVUI: SDLBackend.renderPresent
static void be_render_present(void* ctx) {
    dvui_Sdl3GpuBackend* b = ctx;
    if (b->cmd) {
        if (!SDL_SubmitGPUCommandBuffer(b->cmd)) SDL_Log("dvui: submit GPU command buffer failed: %s", SDL_GetError());
    }
    b->cmd = nullptr;
    b->swapchain_texture = nullptr;
}

// DVUI: sdl3gpu.zig – sleep
static void be_sleep(void* ctx, uint64_t ns) {
    (void)ctx;
    SDL_DelayNS(ns);
}

static int64_t be_nano_time(void* ctx) {
    (void)ctx;
    return (int64_t)SDL_GetTicksNS();
}

dvui_Backend dvui_sdl3gpu_backend(dvui_Sdl3GpuBackend* b) {
    return (dvui_Backend){
        .ctx = b,
        .begin = be_begin,
        .end = be_end,
        .pixel_size = be_pixel_size,
        .window_size = be_window_size,
        .content_scale = be_content_scale,
        .draw_clipped_triangles = be_draw,
        .set_cursor = be_set_cursor,
        .render_present = be_render_present,
        .nano_time = be_nano_time,
        .sleep = be_sleep,
    };
}

// ---------------------------------------------------------------------------
// Události (DVUI: SDLBackend.addEvent a převodní funkce)
// ---------------------------------------------------------------------------

static dvui_Button sdl_mouse_button_to_dvui(uint8_t button) {
    switch (button) {
    case SDL_BUTTON_LEFT: return DVUI_BUTTON_LEFT;
    case SDL_BUTTON_MIDDLE: return DVUI_BUTTON_MIDDLE;
    case SDL_BUTTON_RIGHT: return DVUI_BUTTON_RIGHT;
    case SDL_BUTTON_X1: return DVUI_BUTTON_FOUR;
    case SDL_BUTTON_X2: return DVUI_BUTTON_FIVE;
    default: return DVUI_BUTTON_NONE;
    }
}

static unsigned sdl_keymod_to_dvui(SDL_Keymod keymod) {
    unsigned m = 0;
    if (keymod & SDL_KMOD_LSHIFT) m |= DVUI_MOD_LSHIFT;
    if (keymod & SDL_KMOD_RSHIFT) m |= DVUI_MOD_RSHIFT;
    if (keymod & SDL_KMOD_LCTRL) m |= DVUI_MOD_LCONTROL;
    if (keymod & SDL_KMOD_RCTRL) m |= DVUI_MOD_RCONTROL;
    if (keymod & SDL_KMOD_LALT) m |= DVUI_MOD_LALT;
    if (keymod & SDL_KMOD_RALT) m |= DVUI_MOD_RALT;
    if (keymod & SDL_KMOD_LGUI) m |= DVUI_MOD_LCOMMAND;
    if (keymod & SDL_KMOD_RGUI) m |= DVUI_MOD_RCOMMAND;
    return m;
}

static dvui_Key sdl_keysym_to_dvui(SDL_Keycode key) {
    switch (key) {
    case SDLK_RETURN:
    case SDLK_KP_ENTER: return DVUI_KEY_ENTER;
    case SDLK_SPACE: return DVUI_KEY_SPACE;
    case SDLK_ESCAPE: return DVUI_KEY_ESCAPE;
    case SDLK_TAB: return DVUI_KEY_TAB;
    case SDLK_UP: return DVUI_KEY_UP;
    case SDLK_DOWN: return DVUI_KEY_DOWN;
    case SDLK_LEFT: return DVUI_KEY_LEFT;
    case SDLK_RIGHT: return DVUI_KEY_RIGHT;
    case SDLK_PAGEUP: return DVUI_KEY_PAGE_UP;
    case SDLK_PAGEDOWN: return DVUI_KEY_PAGE_DOWN;
    case SDLK_HOME: return DVUI_KEY_HOME;
    case SDLK_END: return DVUI_KEY_END;
    default: return DVUI_KEY_UNKNOWN;
    }
}

bool dvui_sdl3gpu_add_event(dvui_Sdl3GpuBackend* b, dvui_Window* win, const SDL_Event* event) {
    switch (event->type) {
    case SDL_EVENT_KEY_DOWN:
        return dvui_window_add_event_key(win, sdl_keysym_to_dvui(event->key.key), event->key.repeat ? DVUI_KEY_ACTION_REPEAT : DVUI_KEY_ACTION_DOWN,
                                         sdl_keymod_to_dvui(event->key.mod));
    case SDL_EVENT_KEY_UP:
        return dvui_window_add_event_key(win, sdl_keysym_to_dvui(event->key.key), DVUI_KEY_ACTION_UP, sdl_keymod_to_dvui(event->key.mod));
    case SDL_EVENT_MOUSE_MOTION: {
        // SDL dává souřadnice v bodech okna, dvui chce fyzické pixely
        const float scale = be_pixel_size(b).w / be_window_size(b).w;
        return dvui_window_add_event_mouse_motion(win, (dvui_PointPhysical){event->motion.x * scale, event->motion.y * scale});
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        return dvui_window_add_event_mouse_button(win, sdl_mouse_button_to_dvui(event->button.button), DVUI_MOUSE_PRESS);
    case SDL_EVENT_MOUSE_BUTTON_UP:
        return dvui_window_add_event_mouse_button(win, sdl_mouse_button_to_dvui(event->button.button), DVUI_MOUSE_RELEASE);
    case SDL_EVENT_MOUSE_WHEEL: {
        bool ret = false;
        if (event->wheel.x != 0) ret = dvui_window_add_event_mouse_wheel(win, -event->wheel.x * dvui_scroll_speed, DVUI_HORIZONTAL);
        if (event->wheel.y != 0) ret = dvui_window_add_event_mouse_wheel(win, event->wheel.y * dvui_scroll_speed, DVUI_VERTICAL);
        return ret;
    }
    case SDL_EVENT_WINDOW_MOUSE_LEAVE: dvui_window_add_event_window(win, DVUI_WINDOW_LEAVE); return false;
    case SDL_EVENT_QUIT: dvui_window_add_event_app_quit(win); return false;
    default: return false;
    }
}

bool dvui_sdl3gpu_add_all_events(dvui_Sdl3GpuBackend* b, dvui_Window* win) {
    // modifikátory nastavíme předem, aby je měly i události myši
    dvui_window_modifiers_set(win, sdl_keymod_to_dvui(SDL_GetModState()));
    bool quit = false;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        dvui_sdl3gpu_add_event(b, win, &event);
        if (event.type == SDL_EVENT_QUIT) quit = true;
    }
    return quit;
}

// DVUI: sdl3gpu.zig – waitEventTimeout
bool dvui_sdl3gpu_wait_event_timeout(dvui_Sdl3GpuBackend* b, uint32_t timeout_micros) {
    (void)b;
    if (timeout_micros == UINT32_MAX) {
        // bez časového limitu
        SDL_WaitEvent(nullptr);
        return false;
    }

    if (timeout_micros > 0) {
        // čekání s časovým limitem
        uint32_t timeout = (timeout_micros + 999) / 1000;
        if (timeout > INT32_MAX) timeout = INT32_MAX;
        const bool ret = SDL_WaitEventTimeout(nullptr, (Sint32)timeout);

        // DVUI: obejití https://github.com/libsdl-org/SDL/issues/6539
        // (ponecháno kvůli starším verzím SDL)
        SDL_PollEvent(nullptr);

        return ret;
    }

    // vůbec nečekat
    return false;
}
