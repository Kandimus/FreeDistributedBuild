
#include "tcp_protobufnode.h"

TcpProtobufNode::TcpProtobufNode(uint32_t magic, int32_t id, su::Log* plog) :
    su::Net::PacketNode(magic, 0, sockaddr_in(), id, plog)
{
}

TcpProtobufNode::TcpProtobufNode(uint32_t magic, SOCKET socket, const sockaddr_in& addr, int32_t id, su::Log* plog) :
    su::Net::PacketNode(magic, socket, addr, id, plog)
{
}

size_t TcpProtobufNode::send(const ::google::protobuf::MessageLite& message)
{
    size_t size = message.ByteSizeLong();

    if (!size || !message.IsInitialized())
    {
        return false;
    }

    std::string data;
    message.SerializeToString(&data);

    return su::Net::PacketNode::send(data.data(), data.size());
}

bool TcpProtobufNode::onRecivedMessage(::google::protobuf::MessageLite& message)
{
    return true;
}

