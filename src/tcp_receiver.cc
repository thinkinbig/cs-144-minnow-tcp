#include <cassert>
#include <cstdint>

#include "tcp_receiver.hh"
#include "debug.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  // 1. 错误处理：RST 标志会立即终止连接
  if (message.RST) {
    reassembler_.set_error();
    reassembler_.close();
    return;
  }

  // 2. 连接建立：处理 SYN
  if (!initialized_) {
    if (!message.SYN) {
      return;  // 未初始化时，忽略非 SYN 包
    }
    zero_point_ = message.seqno;
    initialized_ = true;
  }

  // 3. 数据处理
  uint64_t checkpoint = reassembler_.writer().bytes_pushed();
  uint64_t stream_index = message.seqno.unwrap(zero_point_, checkpoint);

  // 调整非 SYN 包的起始位置
  if (!message.SYN) {
    stream_index -= 1;
  }

  // 检查数据是否已经处理过
  bool already_processed = stream_index + message.payload.size() <= reassembler_.writer().bytes_pushed();
  bool empty_fin = message.payload.empty() && message.FIN;
  
  // 已处理的数据包直接丢弃，但空的 FIN 包除外
  if (!message.SYN && already_processed && !empty_fin) {
    return;
  }

  // 重组数据
  reassembler_.insert(stream_index, message.payload, message.FIN);
}

TCPReceiverMessage TCPReceiver::send() const
{
  if (reassembler_.has_error()) {
    return TCPReceiverMessage{
      .ackno = {},
      .window_size = 0,
      .RST = true,
    };
  }

  uint64_t available_capacity = reassembler_.writer().available_capacity();
  uint16_t window_size = available_capacity > UINT16_MAX ? UINT16_MAX : static_cast<uint16_t>(available_capacity);

  if (!initialized_) {
    return TCPReceiverMessage{
      .ackno = {},
      .window_size = window_size,
      .RST = false,
    };
  }

  // 计算下一个期望的序列号
  uint64_t next_seqno = 1;  // SYN 占用第一个序列号
  next_seqno += reassembler_.writer().bytes_pushed();  // 加上已推送的字节数
  if (reassembler_.writer().is_closed()) {
    next_seqno += 1;  // FIN 占用最后一个序列号
  }

  return TCPReceiverMessage{
    .ackno = Wrap32::wrap(next_seqno, zero_point_),
    .window_size = window_size,
    .RST = false,
  };
}
