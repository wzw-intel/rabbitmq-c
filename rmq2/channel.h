/*
 * Portions created by Alan Antonuk are Copyright (c) 2014 Alan Antonuk.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#ifndef RMQ2_CHANNEL_H
#define RMQ2_CHANNEL_H

#include <stdint.h>

#include "rmq2/status.h"

#include <functional>
#include <memory>
#include <string>

namespace rmq2 {

class Envelope;
class Message;
class Table;

class Channel {
 public:
  Channel(const Channel&) = delete;
  Channel& operator=(const Channel&) = delete;
  virtual ~Channel();

  /**
   * Close this channel.
   */
  virtual Status Close() = 0;


  enum class DeclareExchangeFlags {
    kNone = 0,
    kPassive = 1,
    kDurable = 2,
    kAutoDelete = 4,
    kInternal = 8
  };
  virtual Status DeclareExchange(const std::string& exchange_name,
                                 const std::string& exchange_type,
                                 DeclareExchangeFlags flags,
                                 const Table& args) = 0;

  enum class DeleteExchangeFlags {
    kNone = 0,
    kIfUnused = 1
  };
  virtual Status DeleteExchange(const std::string& exchange_name,
                                DeleteExchangeFlags if_unused) = 0;

  virtual Status BindExchange(const std::string& destination,
                              const std::string& source,
                              const std::string& routing_key,
                              const Table& args) = 0;

  virtual Status UnbindExchange(const std::string& destination,
                                const std::string& source,
                                const std::string& routing_key,
                                const Table& args) = 0;

  enum class DeclareQueueFlags {
    kNone = 0,
    kPassive = 1,
    kDurable = 2,
    kExclusive = 4,
    kAutoDelete = 8
  };
  struct DeclareQueueInfo {
    std::string queue_name;
    int64_t message_count;
    int32_t consumer_count;
  };
  virtual Status DeclareQueue(const std::string& queue_name,
                              DeclareQueueFlags flags, const Table& args,
                              DeclareQueueInfo* info) = 0;

  enum class DeleteQueueFlags {
    kNone = 0,
    kIfUnused = 1,
    kIfEmpty = 2
  };
  virtual Status DeleteQueue(const std::string& queue_name,
                             DeleteQueueFlags flags,
                             int64_t* message_count) = 0;

  virtual Status BindQueue(const std::string& queue,
                           const std::string& exchange,
                           const std::string& routing_key,
                           const Table& args) = 0;

  virtual Status UnbindQueue(const std::string& queue,
                             const std::string& exchange,
                             const std::string& routing_key,
                             const Table& args) = 0;

  virtual Status PurgeQueue(const std::string& queue,
                            int64_t message_count) = 0;

  enum class PublishFlags {
    kNone = 0,
    kMandatory = 1,
    kImmediate = 2
  };
  virtual Status Publish(const Message& message, const std::string& exchange,
                         const std::string& routing_key,
                         PublishFlags flags) = 0;

  typedef std::function<void(int64_t)> PublishConfirmFunc;
  virtual Status AddConfirmCallback(PublishConfirmFunc func) = 0;

  enum class ConsumeFlags {
    kNone = 0,
    kNoLocal = 1,
    kNoAck = 2,
    kExclusive = 4,
    kNoWait = 8
  };
  typedef std::function<void(std::unique_ptr<Envelope>)> ConsumeFunc;
  virtual Status Consume(const std::string& queue,
                         const std::string& consumer_tag, ConsumeFlags flags,
                         const Table& args, ConsumeFunc consumer,
                         const std::string* tag) = 0;

  typedef std::function<void(const std::string&)> ConsumerCancelFunc;
  virtual Status AddConsumerCancelationCallback(ConsumerCancelFunc func) = 0;

  virtual Status Cancel(const std::string& consumer_tag) = 0;

  enum class GetFlags {
    kNone,
    kNoAck = 1
  };
  virtual Status Get(const std::string& queue, GetFlags flags,
                     Envelope* envelope, int64_t* message_count) = 0;

  enum class AckFlags {
    kNone = 0,
    kMultiple = 1
  };
  virtual Status Ack(int64_t delivery_tag, AckFlags flags) = 0;

  enum class NackFlags {
    kNone = 0,
    kMultiple = 1,
    kRequeue = 2
  };
  virtual Status Nack(int64_t delivery_tag, NackFlags flags) = 0;

  enum class RecoverFlags {
    kNone = 0,
    kRequeue = 1
  };
  virtual Status Recover(RecoverFlags flags) = 0;

  virtual Status TransactionBegin() = 0;
  virtual Status TransactionCommit() = 0;
  virtual Status TransactionRollback() = 0;
};  // class Channel

}  // namespace rmq2
#endif  /* RMQ2_CHANNEL_H */
