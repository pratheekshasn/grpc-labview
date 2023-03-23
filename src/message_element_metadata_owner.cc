//---------------------------------------------------------------------
//---------------------------------------------------------------------
#include <grpc_server.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <thread>
#include <sstream>
#include <fstream>
#include <iostream>
#include <future>

//---------------------------------------------------------------------
//---------------------------------------------------------------------
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

namespace grpc_labview {
    //---------------------------------------------------------------------
    //---------------------------------------------------------------------
    int ClusterElementSize(LVMessageMetadataType type, bool repeated)
    {
        if (repeated)
        {
            return sizeof(void*);
        }
        switch (type)
        {
        case LVMessageMetadataType::BoolValue:
            return 1;
        case LVMessageMetadataType::Int32Value:
        case LVMessageMetadataType::UInt32Value:
        case LVMessageMetadataType::EnumValue:
        case LVMessageMetadataType::FloatValue:
        case LVMessageMetadataType::SFixed32Value:
        case LVMessageMetadataType::SInt32Value:
        case LVMessageMetadataType::Fixed32Value:
            return 4;
        case LVMessageMetadataType::Int64Value:
        case LVMessageMetadataType::UInt64Value:
        case LVMessageMetadataType::DoubleValue:
        case LVMessageMetadataType::SFixed64Value:
        case LVMessageMetadataType::SInt64Value:
        case LVMessageMetadataType::Fixed64Value:
            return 8;
        case LVMessageMetadataType::StringValue:
        case LVMessageMetadataType::BytesValue:
        case LVMessageMetadataType::MessageValue:
            return sizeof(void*);
        }
        return 0;
    }

    //---------------------------------------------------------------------
    //---------------------------------------------------------------------
    int AlignClusterOffset(int clusterOffset, LVMessageMetadataType type, bool repeated)
    {
        if (clusterOffset == 0)
        {
            return 0;
        }
        auto multiple = ClusterElementSize(type, repeated);
        return AlignClusterOffset(clusterOffset, multiple);
    }

    //---------------------------------------------------------------------
    //---------------------------------------------------------------------
    void MessageElementMetadataOwner::RegisterMetadata(std::shared_ptr<MessageMetadata> requestMetadata)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        
        _registeredMessageMetadata.insert({requestMetadata->messageName, requestMetadata});
    }

    //---------------------------------------------------------------------
    //---------------------------------------------------------------------
    void MessageElementMetadataOwner::RegisterMetadata(std::shared_ptr<EnumMetadata> requestMetadata)
    {
        std::lock_guard<std::mutex> lock(_mutex);

        _registeredEnumMetadata.insert({ requestMetadata->messageName, requestMetadata });
    }

    //---------------------------------------------------------------------
    //---------------------------------------------------------------------
    std::shared_ptr<MessageMetadata> MessageElementMetadataOwner::FindMetadata(const std::string& name)
    {
        auto it = _registeredMessageMetadata.find(name);
        if (it != _registeredMessageMetadata.end())
        {
            return (*it).second;
        }
        return nullptr;
    }

    //---------------------------------------------------------------------
    //---------------------------------------------------------------------
    std::shared_ptr<EnumMetadata> MessageElementMetadataOwner::FindEnumMetadata(const std::string& name)
    {
        auto it = _registeredEnumMetadata.find(name);
        if (it != _registeredEnumMetadata.end())
        {
            return (*it).second;
        }
        return nullptr;
    }

    //---------------------------------------------------------------------
    //---------------------------------------------------------------------
    void MessageElementMetadataOwner::UpdateMetadataClusterLayout(std::shared_ptr<MessageMetadata>& metadata)
    {
        if (metadata->clusterSize != 0 || metadata->_elements.size() == 0)
        {
            return;
        }
        int clusterOffset = 0;
        int maxAlignmentRequirement = 0;
        for (auto element: metadata->_elements)
        {
            if (element->type == LVMessageMetadataType::MessageValue)
            {
                auto nestedMetadata = FindMetadata(element->embeddedMessageName);
                UpdateMetadataClusterLayout(nestedMetadata);
                int alignmentRequirement = 0;
                int elementSize = 0;
                if (element->isRepeated)
                {
                    alignmentRequirement = elementSize = ClusterElementSize(element->type, element->isRepeated);
                }
                else
                {
                    alignmentRequirement = nestedMetadata->alignmentRequirement;
                    elementSize = nestedMetadata->clusterSize;
                }
                clusterOffset = AlignClusterOffset(clusterOffset, alignmentRequirement);
                element->clusterOffset = clusterOffset;
                clusterOffset += elementSize;
                if (maxAlignmentRequirement < alignmentRequirement)
                {
                    maxAlignmentRequirement = alignmentRequirement;
                }
            }
            else
            {
                //maxAlignmentRequirement = GetMaxAlignmentRequirement(element->type, element->isRepeated);

                clusterOffset = AlignClusterOffset(clusterOffset, element->type, element->isRepeated);
                element->clusterOffset = clusterOffset;
                int elementSize = ClusterElementSize(element->type, element->isRepeated);
                clusterOffset += elementSize;
                if (maxAlignmentRequirement < elementSize)
                {
                    maxAlignmentRequirement = elementSize;
                }
            }
        }
        metadata->alignmentRequirement = maxAlignmentRequirement;
        metadata->clusterSize = AlignClusterOffset(clusterOffset, maxAlignmentRequirement);
    }

    int MessageElementMetadataOwner::GetMaxAlignmentRequirement(LVMessageMetadataType elementType, bool elementIsRepeated)
    {
        int clusterOffset = AlignClusterOffset(clusterOffset, elementType, elementIsRepeated);
        //int maxAlignmentRequirement = 0;
        //element->clusterOffset = clusterOffset;
        int elementSize = ClusterElementSize(elementType, elementIsRepeated);
        clusterOffset += elementSize;       

        return elementSize;
    }

    //---------------------------------------------------------------------
    //---------------------------------------------------------------------
    void MessageElementMetadataOwner::UpdateMetadataClusterLayout(std::shared_ptr<EnumMetadata>& metadata)
    {
        if (metadata->clusterSize != 0)
        {
            return;
        }
        int clusterOffset = 0;
        int maxAlignmentRequirement = 0;
        //maxAlignmentRequirement = GetMaxAlignmentRequirement((LVMessageMetadataType)9 /*TODO: Clean up*/, false /*element->isRepeated*/); // This should only be the enum definition, so the "repeated" part should get handled in the field of the message. Is this code required at all?
        metadata->alignmentRequirement = maxAlignmentRequirement;
        metadata->clusterSize = AlignClusterOffset(clusterOffset, maxAlignmentRequirement);
    }

    //---------------------------------------------------------------------
    //---------------------------------------------------------------------
    void MessageElementMetadataOwner::FinalizeMetadata()
    {
        for (auto metadata: _registeredMessageMetadata)
        {
            UpdateMetadataClusterLayout(metadata.second);
        }
        /*for (auto metadata : _registeredEnumMetadata)
        {
            UpdateMetadataClusterLayout(metadata.second);
        }*/
    }
}
