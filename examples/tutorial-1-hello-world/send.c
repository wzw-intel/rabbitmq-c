/*
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MIT
 *
 * Portions created by Alan Antonuk are Copyright (c) 2017 Alan Antonuk.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * ***** END LICENSE BLOCK *****
 */

#include <amqp.h>
#include <amqp_tcp_socket.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
  const char* QUEUE_NAME = "hello";
  const char* MESSAGE = "Hello World!";

  amqp_connection_state_t conn;
  amqp_socket_t *socket;
  amqp_channel_t chan;
  int res;
  amqp_rpc_reply_t reply;

  conn = amqp_new_connection();
  if (!conn) {
    fprintf(stderr, "failed to allocate connection object\n");
    abort();
  }

  /* socket is owned by the conn object, its lifetime is managed by conn */
  socket = amqp_tcp_socket_new(conn);
  if (!socket) {
    fprintf(stderr, "failed to allocate socket object\n");
    abort();
  }

  res = amqp_socket_open(socket, "localhost", AMQP_PROTOCOL_PORT);
  if (res != AMQP_STATUS_OK) {
    fprintf(stderr, "failed to connect to the broker: %s\n",
            amqp_error_string2(res));
    abort();
  }

  reply =
      amqp_login(conn, AMQP_DEFAULT_VHOST, AMQP_DEFAULT_MAX_CHANNELS,
                 AMQP_DEFAULT_FRAME_SIZE, AMQP_DEFAULT_HEARTBEAT,
                 AMQP_SASL_METHOD_PLAIN,
                 "guest", /* default guest account */
                 "guest" /* default guest password */);

  if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
    /* TODO: print error information */
    fprintf(stderr, "failed handshake with broker\n");
    abort();
  }

  /* Channel numbers are any arbitrary number between 1 and the maximum
   * number of channels requested. AMQP_DEFAULT_MAX_CHANNELS requests 2^16-1
   * channels. A good method to select channel numbers is to start at 1 and
   * go up from there, once a channel has been close (amqp_channel_close) it can
   * be reused again.
   */
  chan = 1;
  /* rabbitmq-c that return a pointer to a struct indicate failure by returning
   * NULL. Details on the error can be retried by calling amqp_get_rpc_reply */
  if (!amqp_channel_open(conn, chan)) {
    /* TODO: print error information */
    fprintf(stderr, "failed to open channel: \n");
    abort();
  }

  {
    /* Some functions return additional information in a struct. The queue
     * struct below is owned by the conn object. It is released by calling
     * amqp_maybe_release_buffers or when the whole connection object is
     * released when amqp_destroy_connection is called.
     */
    amqp_queue_declare_ok_t *queue = amqp_queue_declare(
        conn, chan, amqp_cstring_bytes(QUEUE_NAME),
        0, /* Not passive, actually declare the queue */
        0, /* Not durable, won't survive broker restart */
        0, /* Not exclusive to this connection, maybe used by connections */
        0, /* Not automatically deleted upon disconnection */
        amqp_empty_table);
    if (!queue) {
      /* TODO: print error information */
      fprintf(stderr, "failed to declare queue: \n");
      /* TODO: error handling */
      abort();
    }
  }

  {
    amqp_basic_properties_t props;
    memset(&props, 0, sizeof(props));

    props.content_type = amqp_cstring_bytes("text/plain");
    /* props._flags is a bitmask indicating which header fields are set. */
    props._flags |= AMQP_BASIC_CONTENT_TYPE_FLAG;

    res = amqp_basic_publish(
        conn, chan, amqp_empty_bytes,   /* Use the default exchange */
        amqp_cstring_bytes(QUEUE_NAME), /* routing key */
        0, /* Not mandatory, no error if the message is not routed */
        0, /* Not immediate, RabbitMQ does not support this */
        &props, amqp_cstring_bytes(MESSAGE));
    if (res != AMQP_STATUS_OK) {
      fprintf(stderr, "failed to publish message: %s\n",
              amqp_error_string2(res));
      abort();
    }
  }

  printf(" [x] Sent '%s'\n", MESSAGE);

  /* close the connection to the broker, this implicitly closes all channels,
   * there is no need to explicitly close any channels when tearing down a
   * connection */
  reply = amqp_connection_close(conn, AMQP_REPLY_SUCCESS);
  if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
    /* TODO: print error information */
    fprintf(stderr, "failed to close connection: \n");
    /* TODO: error handling */
    abort();
  }

  amqp_destroy_connection(conn);

  return 0;
}
