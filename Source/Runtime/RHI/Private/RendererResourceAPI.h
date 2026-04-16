#pragma once

#include "RHI/IGraphics.h"

struct SubresourceDataDesc;

void FORGE_CALLCONV getBufferSizeAlign(Renderer* pRenderer, const BufferDesc* pDesc, ResourceSizeAlign* pOut);
void FORGE_CALLCONV getTextureSizeAlign(Renderer* pRenderer, const TextureDesc* pDesc, ResourceSizeAlign* pOut);
void FORGE_CALLCONV addBuffer(Renderer* pRenderer, const BufferDesc* pDesc, Buffer** ppBuffer);
void FORGE_CALLCONV removeBuffer(Renderer* pRenderer, Buffer* pBuffer);
void FORGE_CALLCONV mapBuffer(Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange);
void FORGE_CALLCONV unmapBuffer(Renderer* pRenderer, Buffer* pBuffer);
void FORGE_CALLCONV cmdUpdateBuffer(Cmd* pCmd, Buffer* pBuffer, uint64_t dstOffset, Buffer* pSrcBuffer, uint64_t srcOffset, uint64_t size);
void FORGE_CALLCONV cmdUpdateSubresource(Cmd* pCmd, Texture* pTexture, Buffer* pSrcBuffer, const SubresourceDataDesc* pSubresourceDesc);
void FORGE_CALLCONV cmdCopySubresource(Cmd* pCmd, Buffer* pDstBuffer, Texture* pTexture, const SubresourceDataDesc* pSubresourceDesc);
void FORGE_CALLCONV addTexture(Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture);
void FORGE_CALLCONV removeTexture(Renderer* pRenderer, Texture* pTexture);
