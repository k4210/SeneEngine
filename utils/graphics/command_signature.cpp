//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard 
//

#include "stdafx.h"
#include "command_signature.h"
#include "root_signature.h"
#include "../base_app_helper.h"
ID3D12CommandSignature* CommandSignature::Finalize(ID3D12Device* device, ID3D12RootSignature* root_signature)
{
    assert(device);
    assert(!m_Finalized);
    if (m_Finalized)
        return m_Signature.Get();

    m_ByteStride = 0;
    bool RequiresRootSignature = false;

    for (UINT i = 0; i < m_NumParameters; ++i)
    {
        switch (m_ParamArray[i].GetDesc().Type)
        {
            case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW:
                m_ByteStride += sizeof(D3D12_DRAW_ARGUMENTS);
                break;
            case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED:
                m_ByteStride += sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
                break;
            case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH:
                m_ByteStride += sizeof(D3D12_DISPATCH_ARGUMENTS);
                break;
            case D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT:
                m_ByteStride += m_ParamArray[i].GetDesc().Constant.Num32BitValuesToSet * 4;
                RequiresRootSignature = true;
                break;
            case D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW:
                m_ByteStride += sizeof(D3D12_VERTEX_BUFFER_VIEW);
                break;
            case D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW:
                m_ByteStride += sizeof(D3D12_INDEX_BUFFER_VIEW);
                break;
            case D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW:
            case D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW:
            case D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW:
                m_ByteStride += 8;
                RequiresRootSignature = true;
                break;
        }
    }

    D3D12_COMMAND_SIGNATURE_DESC CommandSignatureDesc;
    CommandSignatureDesc.ByteStride = m_ByteStride;
    CommandSignatureDesc.NumArgumentDescs = m_NumParameters;
    CommandSignatureDesc.pArgumentDescs = (const D3D12_INDIRECT_ARGUMENT_DESC*)m_ParamArray.get();
    CommandSignatureDesc.NodeMask = 1;

    Microsoft::WRL::ComPtr<ID3DBlob> pOutBlob, pErrorBlob;

    if (RequiresRootSignature)
    {
        assert(root_signature != nullptr);
    }
    else
    {
        root_signature = nullptr;
    }

	ThrowIfFailed(device->CreateCommandSignature(&CommandSignatureDesc, root_signature, IID_PPV_ARGS(&m_Signature)));

    m_Finalized = TRUE;

    return m_Signature.Get();
}
