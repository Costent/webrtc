/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>

#include <list>
#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/rtp_rtcp/include/fec_receiver.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/mocks/mock_rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/rtp_rtcp/source/fec_test_helper.h"
#include "webrtc/modules/rtp_rtcp/source/forward_error_correction.h"

namespace webrtc {

namespace {
constexpr uint8_t kFecPayloadType = 96;
}  // namespace

using ::testing::_;
using ::testing::Args;
using ::testing::ElementsAreArray;
using ::testing::Return;

using ::webrtc::test::fec::FrameGenerator;
using Packet = ::webrtc::ForwardErrorCorrection::Packet;
using ::webrtc::test::fec::RawRtpPacket;

class ReceiverFecTest : public ::testing::Test {
 protected:
  ReceiverFecTest()
      : fec_(ForwardErrorCorrection::CreateUlpfec()),
        receiver_fec_(FecReceiver::Create(&rtp_data_callback_)) {}

  void EncodeFec(ForwardErrorCorrection::PacketList* media_packets,
                 std::list<ForwardErrorCorrection::Packet*>* fec_packets,
                 unsigned int num_fec_packets) {
    uint8_t protection_factor = num_fec_packets * 255 / media_packets->size();
    EXPECT_EQ(0, fec_->EncodeFec(*media_packets, protection_factor, 0, false,
                                 kFecMaskBursty, fec_packets));
    ASSERT_EQ(num_fec_packets, fec_packets->size());
  }

  void GenerateFrame(int num_media_packets,
                     int frame_offset,
                     std::list<RawRtpPacket*>* media_rtp_packets,
                     ForwardErrorCorrection::PacketList* media_packets) {
    generator_.NewFrame(num_media_packets);
    for (int i = 0; i < num_media_packets; ++i) {
      std::unique_ptr<RawRtpPacket> next_packet(
          generator_.NextPacket(frame_offset + i, kRtpHeaderSize + 10));
      media_rtp_packets->push_back(next_packet.get());
      media_packets->push_back(std::move(next_packet));
    }
  }

  void VerifyReconstructedMediaPacket(const RawRtpPacket* packet, int times) {
    // Verify that the content of the reconstructed packet is equal to the
    // content of |packet|, and that the same content is received |times| number
    // of times in a row.
    EXPECT_CALL(rtp_data_callback_, OnRecoveredPacket(_, packet->length))
        .With(Args<0, 1>(ElementsAreArray(packet->data,
                                          packet->length)))
        .Times(times).WillRepeatedly(Return(true));
  }

  void BuildAndAddRedMediaPacket(RawRtpPacket* packet) {
    std::unique_ptr<RawRtpPacket> red_packet(
        generator_.BuildMediaRedPacket(packet));
    EXPECT_EQ(0, receiver_fec_->AddReceivedRedPacket(
                     red_packet->header.header, red_packet->data,
                     red_packet->length, kFecPayloadType));
  }

  void BuildAndAddRedFecPacket(Packet* packet) {
    std::unique_ptr<RawRtpPacket> red_packet(
        generator_.BuildFecRedPacket(packet));
    EXPECT_EQ(0, receiver_fec_->AddReceivedRedPacket(
                     red_packet->header.header, red_packet->data,
                     red_packet->length, kFecPayloadType));
  }

  void InjectGarbagePacketLength(size_t fec_garbage_offset);
  static void SurvivesMaliciousPacket(const uint8_t* data,
                                      size_t length,
                                      uint8_t ulpfec_payload_type);

  MockRtpData rtp_data_callback_;
  std::unique_ptr<ForwardErrorCorrection> fec_;
  std::unique_ptr<FecReceiver> receiver_fec_;
  FrameGenerator generator_;
};

TEST_F(ReceiverFecTest, TwoMediaOneFec) {
  const unsigned int kNumFecPackets = 1u;
  std::list<RawRtpPacket*> media_rtp_packets;
  ForwardErrorCorrection::PacketList media_packets;
  GenerateFrame(2, 0, &media_rtp_packets, &media_packets);
  std::list<ForwardErrorCorrection::Packet*> fec_packets;
  EncodeFec(&media_packets, &fec_packets, kNumFecPackets);

  // Recovery
  auto it = media_rtp_packets.begin();
  BuildAndAddRedMediaPacket(*it);
  VerifyReconstructedMediaPacket(*it, 1);
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());
  // Drop one media packet.
  auto fec_it = fec_packets.begin();
  BuildAndAddRedFecPacket(*fec_it);
  ++it;
  VerifyReconstructedMediaPacket(*it, 1);
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());

  FecPacketCounter counter = receiver_fec_->GetPacketCounter();
  EXPECT_EQ(2U, counter.num_packets);
  EXPECT_EQ(1U, counter.num_fec_packets);
  EXPECT_EQ(1U, counter.num_recovered_packets);
}

void ReceiverFecTest::InjectGarbagePacketLength(size_t fec_garbage_offset) {
  EXPECT_CALL(rtp_data_callback_, OnRecoveredPacket(_, _))
      .WillRepeatedly(Return(true));

  const unsigned int kNumFecPackets = 1u;
  std::list<RawRtpPacket*> media_rtp_packets;
  ForwardErrorCorrection::PacketList media_packets;
  GenerateFrame(2, 0, &media_rtp_packets, &media_packets);
  std::list<ForwardErrorCorrection::Packet*> fec_packets;
  EncodeFec(&media_packets, &fec_packets, kNumFecPackets);
  ByteWriter<uint16_t>::WriteBigEndian(
      &fec_packets.front()->data[fec_garbage_offset], 0x4711);

  // Inject first media packet, then first FEC packet, skipping the second media
  // packet to cause a recovery from the FEC packet.
  BuildAndAddRedMediaPacket(media_rtp_packets.front());
  BuildAndAddRedFecPacket(fec_packets.front());
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());

  FecPacketCounter counter = receiver_fec_->GetPacketCounter();
  EXPECT_EQ(2u, counter.num_packets);
  EXPECT_EQ(1u, counter.num_fec_packets);
  EXPECT_EQ(0u, counter.num_recovered_packets);
}

TEST_F(ReceiverFecTest, InjectGarbageFecHeaderLengthRecovery) {
  // Byte offset 8 is the 'length recovery' field of the FEC header.
  InjectGarbagePacketLength(8);
}

TEST_F(ReceiverFecTest, InjectGarbageFecLevelHeaderProtectionLength) {
  // Byte offset 10 is the 'protection length' field in the first FEC level
  // header.
  InjectGarbagePacketLength(10);
}

TEST_F(ReceiverFecTest, TwoMediaTwoFec) {
  const unsigned int kNumFecPackets = 2u;
  std::list<RawRtpPacket*> media_rtp_packets;
  ForwardErrorCorrection::PacketList media_packets;
  GenerateFrame(2, 0, &media_rtp_packets, &media_packets);
  std::list<ForwardErrorCorrection::Packet*> fec_packets;
  EncodeFec(&media_packets, &fec_packets, kNumFecPackets);

  // Recovery
  // Drop both media packets.
  auto it = media_rtp_packets.begin();
  auto fec_it = fec_packets.begin();
  BuildAndAddRedFecPacket(*fec_it);
  VerifyReconstructedMediaPacket(*it, 1);
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());
  ++fec_it;
  BuildAndAddRedFecPacket(*fec_it);
  ++it;
  VerifyReconstructedMediaPacket(*it, 1);
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());
}

TEST_F(ReceiverFecTest, TwoFramesOneFec) {
  const unsigned int kNumFecPackets = 1u;
  std::list<RawRtpPacket*> media_rtp_packets;
  ForwardErrorCorrection::PacketList media_packets;
  GenerateFrame(1, 0, &media_rtp_packets, &media_packets);
  GenerateFrame(1, 1, &media_rtp_packets, &media_packets);
  std::list<ForwardErrorCorrection::Packet*> fec_packets;
  EncodeFec(&media_packets, &fec_packets, kNumFecPackets);

  // Recovery
  auto it = media_rtp_packets.begin();
  BuildAndAddRedMediaPacket(media_rtp_packets.front());
  VerifyReconstructedMediaPacket(*it, 1);
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());
  // Drop one media packet.
  BuildAndAddRedFecPacket(fec_packets.front());
  ++it;
  VerifyReconstructedMediaPacket(*it, 1);
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());
}

TEST_F(ReceiverFecTest, OneCompleteOneUnrecoverableFrame) {
  const unsigned int kNumFecPackets = 1u;
  std::list<RawRtpPacket*> media_rtp_packets;
  ForwardErrorCorrection::PacketList media_packets;
  GenerateFrame(1, 0, &media_rtp_packets, &media_packets);
  GenerateFrame(2, 1, &media_rtp_packets, &media_packets);

  std::list<ForwardErrorCorrection::Packet*> fec_packets;
  EncodeFec(&media_packets, &fec_packets, kNumFecPackets);

  // Recovery
  auto it = media_rtp_packets.begin();
  BuildAndAddRedMediaPacket(*it);  // First frame: one packet.
  VerifyReconstructedMediaPacket(*it, 1);
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());
  ++it;
  BuildAndAddRedMediaPacket(*it);  // First packet of second frame.
  VerifyReconstructedMediaPacket(*it, 1);
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());
}

TEST_F(ReceiverFecTest, MaxFramesOneFec) {
  const unsigned int kNumFecPackets = 1u;
  const unsigned int kNumMediaPackets = 48u;
  std::list<RawRtpPacket*> media_rtp_packets;
  ForwardErrorCorrection::PacketList media_packets;
  for (unsigned int i = 0; i < kNumMediaPackets; ++i) {
    GenerateFrame(1, i, &media_rtp_packets, &media_packets);
  }
  std::list<ForwardErrorCorrection::Packet*> fec_packets;
  EncodeFec(&media_packets, &fec_packets, kNumFecPackets);

  // Recovery
  auto it = media_rtp_packets.begin();
  ++it;  // Drop first packet.
  for (; it != media_rtp_packets.end(); ++it) {
    BuildAndAddRedMediaPacket(*it);
    VerifyReconstructedMediaPacket(*it, 1);
    EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());
  }
  BuildAndAddRedFecPacket(fec_packets.front());
  it = media_rtp_packets.begin();
  VerifyReconstructedMediaPacket(*it, 1);
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());
}

TEST_F(ReceiverFecTest, TooManyFrames) {
  const unsigned int kNumFecPackets = 1u;
  const unsigned int kNumMediaPackets = 49u;
  std::list<RawRtpPacket*> media_rtp_packets;
  ForwardErrorCorrection::PacketList media_packets;
  for (unsigned int i = 0; i < kNumMediaPackets; ++i) {
    GenerateFrame(1, i, &media_rtp_packets, &media_packets);
  }
  std::list<ForwardErrorCorrection::Packet*> fec_packets;
  EXPECT_EQ(-1, fec_->EncodeFec(media_packets,
                                kNumFecPackets * 255 / kNumMediaPackets, 0,
                                false, kFecMaskBursty, &fec_packets));
}

TEST_F(ReceiverFecTest, PacketNotDroppedTooEarly) {
  // 1 frame with 2 media packets and one FEC packet. One media packet missing.
  // Delay the FEC packet.
  Packet* delayed_fec = NULL;
  const unsigned int kNumFecPacketsBatch1 = 1u;
  const unsigned int kNumMediaPacketsBatch1 = 2u;
  std::list<RawRtpPacket*> media_rtp_packets_batch1;
  ForwardErrorCorrection::PacketList media_packets_batch1;
  GenerateFrame(kNumMediaPacketsBatch1, 0, &media_rtp_packets_batch1,
                &media_packets_batch1);
  std::list<ForwardErrorCorrection::Packet*> fec_packets;
  EncodeFec(&media_packets_batch1, &fec_packets, kNumFecPacketsBatch1);

  BuildAndAddRedMediaPacket(media_rtp_packets_batch1.front());
  EXPECT_CALL(rtp_data_callback_, OnRecoveredPacket(_, _))
      .Times(1).WillRepeatedly(Return(true));
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());
  delayed_fec = fec_packets.front();

  // Fill the FEC decoder. No packets should be dropped.
  const unsigned int kNumMediaPacketsBatch2 = 46u;
  std::list<RawRtpPacket*> media_rtp_packets_batch2;
  ForwardErrorCorrection::PacketList media_packets_batch2;
  for (unsigned int i = 0; i < kNumMediaPacketsBatch2; ++i) {
    GenerateFrame(1, i, &media_rtp_packets_batch2, &media_packets_batch2);
  }
  for (auto it = media_rtp_packets_batch2.begin();
       it != media_rtp_packets_batch2.end(); ++it) {
    BuildAndAddRedMediaPacket(*it);
    EXPECT_CALL(rtp_data_callback_, OnRecoveredPacket(_, _))
        .Times(1).WillRepeatedly(Return(true));
    EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());
  }

  // Add the delayed FEC packet. One packet should be reconstructed.
  BuildAndAddRedFecPacket(delayed_fec);
  EXPECT_CALL(rtp_data_callback_, OnRecoveredPacket(_, _))
      .Times(1).WillRepeatedly(Return(true));
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());
}

TEST_F(ReceiverFecTest, PacketDroppedWhenTooOld) {
  // 1 frame with 2 media packets and one FEC packet. One media packet missing.
  // Delay the FEC packet.
  Packet* delayed_fec = NULL;
  const unsigned int kNumFecPacketsBatch1 = 1u;
  const unsigned int kNumMediaPacketsBatch1 = 2u;
  std::list<RawRtpPacket*> media_rtp_packets_batch1;
  ForwardErrorCorrection::PacketList media_packets_batch1;
  GenerateFrame(kNumMediaPacketsBatch1, 0, &media_rtp_packets_batch1,
                &media_packets_batch1);
  std::list<ForwardErrorCorrection::Packet*> fec_packets;
  EncodeFec(&media_packets_batch1, &fec_packets, kNumFecPacketsBatch1);

  BuildAndAddRedMediaPacket(media_rtp_packets_batch1.front());
  EXPECT_CALL(rtp_data_callback_, OnRecoveredPacket(_, _))
      .Times(1).WillRepeatedly(Return(true));
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());
  delayed_fec = fec_packets.front();

  // Fill the FEC decoder and force the last packet to be dropped.
  const unsigned int kNumMediaPacketsBatch2 = 48u;
  std::list<RawRtpPacket*> media_rtp_packets_batch2;
  ForwardErrorCorrection::PacketList media_packets_batch2;
  for (unsigned int i = 0; i < kNumMediaPacketsBatch2; ++i) {
    GenerateFrame(1, i, &media_rtp_packets_batch2, &media_packets_batch2);
  }
  for (auto it = media_rtp_packets_batch2.begin();
       it != media_rtp_packets_batch2.end(); ++it) {
    BuildAndAddRedMediaPacket(*it);
    EXPECT_CALL(rtp_data_callback_, OnRecoveredPacket(_, _))
        .Times(1).WillRepeatedly(Return(true));
    EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());
  }

  // Add the delayed FEC packet. No packet should be reconstructed since the
  // first media packet of that frame has been dropped due to being too old.
  BuildAndAddRedFecPacket(delayed_fec);
  EXPECT_CALL(rtp_data_callback_, OnRecoveredPacket(_, _))
      .Times(0);
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());
}

TEST_F(ReceiverFecTest, OldFecPacketDropped) {
  // 49 frames with 2 media packets and one FEC packet. All media packets
  // missing.
  const unsigned int kNumMediaPackets = 49 * 2;
  std::list<RawRtpPacket*> media_rtp_packets;
  ForwardErrorCorrection::PacketList media_packets;
  for (unsigned int i = 0; i < kNumMediaPackets / 2; ++i) {
    std::list<RawRtpPacket*> frame_media_rtp_packets;
    ForwardErrorCorrection::PacketList frame_media_packets;
    std::list<ForwardErrorCorrection::Packet*> fec_packets;
    GenerateFrame(2, 0, &frame_media_rtp_packets, &frame_media_packets);
    EncodeFec(&frame_media_packets, &fec_packets, 1);
    for (auto it = fec_packets.begin(); it != fec_packets.end(); ++it) {
      // Only FEC packets inserted. No packets recoverable at this time.
      BuildAndAddRedFecPacket(*it);
      EXPECT_CALL(rtp_data_callback_, OnRecoveredPacket(_, _))
          .Times(0);
      EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());
    }
    // Move unique_ptr's to media_packets for lifetime management.
    media_packets.insert(media_packets.end(),
                         std::make_move_iterator(frame_media_packets.begin()),
                         std::make_move_iterator(frame_media_packets.end()));
    media_rtp_packets.insert(media_rtp_packets.end(),
                             frame_media_rtp_packets.begin(),
                             frame_media_rtp_packets.end());
  }
  // Insert the oldest media packet. The corresponding FEC packet is too old
  // and should've been dropped. Only the media packet we inserted will be
  // returned.
  BuildAndAddRedMediaPacket(media_rtp_packets.front());
  EXPECT_CALL(rtp_data_callback_, OnRecoveredPacket(_, _))
      .Times(1).WillRepeatedly(Return(true));
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFec());
}

void ReceiverFecTest::SurvivesMaliciousPacket(const uint8_t* data,
                                              size_t length,
                                              uint8_t ulpfec_payload_type) {
  webrtc::RTPHeader header;
  std::unique_ptr<webrtc::RtpHeaderParser> parser(
      webrtc::RtpHeaderParser::Create());
  ASSERT_TRUE(parser->Parse(data, length, &header));

  webrtc::NullRtpData null_callback;
  std::unique_ptr<webrtc::FecReceiver> receiver_fec(
      webrtc::FecReceiver::Create(&null_callback));

  receiver_fec->AddReceivedRedPacket(header, data, length, ulpfec_payload_type);
}

TEST_F(ReceiverFecTest, TruncatedPacketWithFBitSet) {
  const uint8_t kTruncatedPacket[] = {0x80,
                                      0x2a,
                                      0x68,
                                      0x71,
                                      0x29,
                                      0xa1,
                                      0x27,
                                      0x3a,
                                      0x29,
                                      0x12,
                                      0x2a,
                                      0x98,
                                      0xe0,
                                      0x29};

  SurvivesMaliciousPacket(kTruncatedPacket, sizeof(kTruncatedPacket), 100);
}

TEST_F(ReceiverFecTest, TruncatedPacketWithFBitSetEndingAfterFirstRedHeader) {
  const uint8_t kPacket[] = {0x89,
                             0x27,
                             0x3a,
                             0x83,
                             0x27,
                             0x3a,
                             0x3a,
                             0xf3,
                             0x67,
                             0xbe,
                             0x2a,
                             0xa9,
                             0x27,
                             0x54,
                             0x3a,
                             0x3a,
                             0x2a,
                             0x67,
                             0x3a,
                             0xf3,
                             0x67,
                             0xbe,
                             0x2a,
                             0x27,
                             0xe6,
                             0xf6,
                             0x03,
                             0x3e,
                             0x29,
                             0x27,
                             0x21,
                             0x27,
                             0x2a,
                             0x29,
                             0x21,
                             0x4b,
                             0x29,
                             0x3a,
                             0x28,
                             0x29,
                             0xbf,
                             0x29,
                             0x2a,
                             0x26,
                             0x29,
                             0xae,
                             0x27,
                             0xa6,
                             0xf6,
                             0x00,
                             0x03,
                             0x3e};
  SurvivesMaliciousPacket(kPacket, sizeof(kPacket), 100);
}

TEST_F(ReceiverFecTest, TruncatedPacketWithoutDataPastFirstBlock) {
  const uint8_t kPacket[] = {0x82,
                             0x38,
                             0x92,
                             0x38,
                             0x92,
                             0x38,
                             0xde,
                             0x2a,
                             0x11,
                             0xc8,
                             0xa3,
                             0xc4,
                             0x82,
                             0x38,
                             0x2a,
                             0x21,
                             0x2a,
                             0x28,
                             0x92,
                             0x38,
                             0x92,
                             0x00,
                             0x00,
                             0x0a,
                             0x3a,
                             0xc8,
                             0xa3,
                             0x3a,
                             0x27,
                             0xc4,
                             0x2a,
                             0x21,
                             0x2a,
                             0x28};
  SurvivesMaliciousPacket(kPacket, sizeof(kPacket), 100);
}

}  // namespace webrtc
