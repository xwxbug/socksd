#define _GNU_SOURCE
#ifndef _WIN32
#include <sys/prctl.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "uv.h"
#include "util.h"
#include "common.h"
#include "udprelay.h"
#include "resolver.h"
#include "consumer.h"


extern uv_key_t thread_resolver_key;
extern void close_loop(uv_loop_t *loop);

static void
ipc_read_cb(uv_stream_t *handle, ssize_t nread, const uv_buf_t *buf) {
    struct ipc_client_ctx* ctx;
    uv_loop_t *loop;
    uv_handle_type type;
    uv_pipe_t *ipc_pipe;

    ipc_pipe = (uv_pipe_t*)handle;
    ctx = container_of(ipc_pipe, struct ipc_client_ctx, ipc_pipe);
    loop = handle->loop;

    assert(1 == uv_pipe_pending_count(ipc_pipe));
    type = uv_pipe_pending_type(ipc_pipe);

    if (type == UV_TCP) {
        uv_tcp_init(loop, (uv_tcp_t*) ctx->server_handle);
    } else if (type == UV_NAMED_PIPE) {
        uv_pipe_init(loop, (uv_pipe_t*) ctx->server_handle, 0);
    } else {
        assert(0 && "invalid type");
    }

    uv_accept(handle, ctx->server_handle);
    uv_close((uv_handle_t*) &ctx->ipc_pipe, NULL);
}

static void
ipc_alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    struct ipc_client_ctx *ctx;
    ctx = container_of(handle, struct ipc_client_ctx, ipc_pipe);
    buf->base = ctx->scratch;
    buf->len = sizeof(ctx->scratch);
}

static void
ipc_connect_cb(uv_connect_t *req, int status) {
    struct ipc_client_ctx *ctx;
    ctx = container_of(req, struct ipc_client_ctx, connect_req);
    uv_read_start((uv_stream_t*)&ctx->ipc_pipe, ipc_alloc_cb, ipc_read_cb);
}

static void
consumer_close(uv_async_t *handle) {
    struct server_context *ctx = container_of(handle, struct server_context, async_handle);

    uv_close((uv_handle_t*) &ctx->server_handle, NULL);
    uv_close((uv_handle_t*) &ctx->async_handle, NULL);

    if (ctx->udprelay) {
        udprelay_close(ctx);
    }

    struct resolver_context *dns = uv_key_get(&thread_resolver_key);
    assert(dns != NULL);
    resolver_shutdown(dns);
}

static void
get_listen_handle(uv_loop_t *loop, uv_stream_t *server_handle) {
    struct ipc_client_ctx ctx;

    ctx.server_handle = server_handle;
    ctx.server_handle->data = "server handle";

    uv_pipe_init(loop, &ctx.ipc_pipe, 1);
    uv_pipe_connect(&ctx.connect_req, &ctx.ipc_pipe, IPC_PIPE_NAME, ipc_connect_cb);
    uv_run(loop, UV_RUN_DEFAULT);
}

void
consumer_start(void *arg) {
    uv_loop_t *loop;
    struct server_context *ctx;

    ctx = arg;
#ifndef _WIN32
	char name[24] = { 0 };
	sprintf(name, "consumer-%d", ctx->index + 1);
	prctl(PR_SET_NAME, name, 0, 0, 0);
#endif
    loop = malloc(sizeof(uv_loop_t));
    uv_loop_init(loop);
    listener_event_loops[ctx->index] = *loop;

    uv_barrier_wait(listeners_created_barrier);

    uv_async_init(loop, &ctx->async_handle, consumer_close);
    uv_unref((uv_handle_t*)&ctx->async_handle);

    /* Wait until the dispatcher thread is ready. */
    uv_sem_wait(&ctx->semaphore);

    get_listen_handle(loop, (uv_stream_t*)&ctx->server_handle);

    struct resolver_context *dns =
      resolver_init(loop, MODE_IPV4,
        ctx->nameserver_num == 0 ? NULL : ctx->nameservers, ctx->nameserver_num);

    uv_key_set(&thread_resolver_key, dns);

    uv_listen((uv_stream_t*)&ctx->server_handle, 128, ctx->accept_cb);

    if (ctx->udprelay) {
        udprelay_start(loop, ctx);
    }

    uv_run(loop, UV_RUN_DEFAULT);

    close_loop(loop);
    free(loop);
    resolver_destroy(dns);

    uv_sem_post(&ctx->semaphore);
}
