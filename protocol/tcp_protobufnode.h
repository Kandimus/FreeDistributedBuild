
#pragma once

#include "net/packetnode.h"

#pragma warning(disable:4251)
#include "google/protobuf/message_lite.h"
#pragma warning(default:4251)

class TcpProtobufNode : public su::Net::PacketNode
{
public:
    TcpProtobufNode(uint32_t magic, int32_t id = -1, su::Log* plog = nullptr);
    TcpProtobufNode(uint32_t magic, SOCKET socket, const sockaddr_in& addr, int32_t id = -1, su::Log* plog = nullptr);
    virtual ~TcpProtobufNode() = default;

    // TcpProtobufNode
    virtual size_t send(const ::google::protobuf::MessageLite& message);
    virtual bool onRecivedMessage(::google::protobuf::MessageLite& message);

public:
    int32_t m_freeCores = 0;
};
