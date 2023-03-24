//---------------------------------------------------------------------
//---------------------------------------------------------------------
#pragma once

//---------------------------------------------------------------------
//---------------------------------------------------------------------
#include <message_metadata.h>
#include <enum_metadata.h>

namespace grpc_labview 
{
    //---------------------------------------------------------------------
    //---------------------------------------------------------------------
    class IMessageElementMetadataOwner
    {
    public:
        virtual std::shared_ptr<MessageMetadata> FindMetadata(const std::string& name) = 0;
    };

    //---------------------------------------------------------------------
    //---------------------------------------------------------------------
    class MessageElementMetadataOwner : public IMessageElementMetadataOwner
    {    
    public:
        void RegisterMetadata(std::shared_ptr<MessageMetadata> requestMetadata); // Repeat for enum?
        void RegisterMetadata(std::shared_ptr<EnumMetadata> requestMetadata); // Repeat for enum?
        std::shared_ptr<MessageMetadata> FindMetadata(const std::string& name) override;
        std::shared_ptr<EnumMetadata> FindEnumMetadata(const std::string& name);
        void FinalizeMetadata();

    private:
        std::mutex _mutex;
        std::map<std::string, std::shared_ptr<MessageMetadata>> _registeredMessageMetadata;
        std::map<std::string, std::shared_ptr<EnumMetadata>> _registeredEnumMetadata;
        void UpdateMetadataClusterLayout(std::shared_ptr<MessageMetadata>& metadata);
        void UpdateMetadataClusterLayout(std::shared_ptr<EnumMetadata>& metadata);
        int GetMaxAlignmentRequirement(LVMessageMetadataType elementType, bool elementIsRepeated);
    };
}
