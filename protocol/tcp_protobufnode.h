
#pragma once

#include "net/packetnode.h"
#include "google/protobuf/message_lite.h"

class TcpProtobufNode : public su::Net::PacketNode
{
public:
    TcpProtobufNode(uint32_t magic, int32_t id = -1, su::Log* plog = nullptr);
    TcpProtobufNode(uint32_t magic, SOCKET socket, const sockaddr_in& addr, int32_t id = -1, su::Log* plog = nullptr);
    virtual ~TcpProtobufNode() = default;

    // TcpProtobufNode
    virtual size_t send(const ::google::protobuf::MessageLite& message);
    virtual bool onRecivedMessage(::google::protobuf::MessageLite& message);
};
