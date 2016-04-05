/**
 * Copyright (C) Mellanox Technologies Ltd. 2001-2016.  ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */

#include <common/test.h>
extern "C" {
#include <ucs/arch/atomic.h>
#include <ucs/datastruct/callbackq.h>
}

class test_callbackq : public ucs::test {
protected:

    enum {
        COMMAND_NONE,
        COMMAND_REMOVE_SELF,
        COMMAND_ADD_ANOTHER
    };

    struct callback_ctx {
        test_callbackq *test;
        uint32_t       count;
        int            command;
        callback_ctx   *to_add;
    };

    virtual void init() {
        ucs::test::init();
        ucs_status_t status = ucs_callbackq_init(&m_cbq, 64);
        ASSERT_UCS_OK(status);
    }

    virtual void cleanup() {
        ucs_callbackq_cleanup(&m_cbq);
        ucs::test::cleanup();
    }

    static void callback_proxy(void *arg)
    {
        callback_ctx *ctx = reinterpret_cast<callback_ctx*>(arg);
        ctx->test->callback(ctx);
    }

    void callback(callback_ctx *ctx)
    {
        ucs_atomic_add32(&ctx->count, 1);

        switch (ctx->command) {
        case COMMAND_REMOVE_SELF:
            remove_sync(ctx);
            break;
        case COMMAND_ADD_ANOTHER:
            add_sync(ctx->to_add);
            break;
        case COMMAND_NONE:
        default:
            break;
        }
    }

    void init_ctx(callback_ctx *ctx)
    {
        ctx->test    = this;
        ctx->count   = 0;
        ctx->command = COMMAND_NONE;
    }

    void add_sync(callback_ctx *ctx)
    {
        ucs_status_t status = ucs_callbackq_add_sync(&m_cbq, callback_proxy,
                                                     reinterpret_cast<void*>(ctx));
        ASSERT_UCS_OK(status);
    }

    void remove_sync(callback_ctx *ctx)
    {
        ucs_status_t status = ucs_callbackq_remove_sync(&m_cbq, callback_proxy,
                                                        reinterpret_cast<void*>(ctx));
        ASSERT_UCS_OK(status);
    }

    void add_async(callback_ctx *ctx)
    {
        ucs_callbackq_add_async(&m_cbq, callback_proxy,
                                reinterpret_cast<void*>(ctx));
    }

    void remove_async(callback_ctx *ctx)
    {
        ucs_callbackq_remove_async(&m_cbq, callback_proxy,
                                   reinterpret_cast<void*>(ctx));
    }

    void dispatch(unsigned count = 1)
    {
        for (unsigned i = 0; i < count; ++i) {
            ucs_callbackq_dispatch(&m_cbq);
        }
    }

    ucs_callbackq_t m_cbq;
};


UCS_TEST_F(test_callbackq, single) {
    callback_ctx ctx;

    init_ctx(&ctx);
    add_sync(&ctx);
    dispatch();
    remove_sync(&ctx);
    EXPECT_EQ(1u, ctx.count);
}

UCS_TEST_F(test_callbackq, refcount) {
    callback_ctx ctx;

    init_ctx(&ctx);
    add_sync(&ctx);
    add_sync(&ctx);

    dispatch();
    EXPECT_EQ(1u, ctx.count);

    remove_sync(&ctx);
    dispatch();
    EXPECT_EQ(2u, ctx.count);

    remove_sync(&ctx);
    dispatch();
    EXPECT_EQ(2u, ctx.count);
}

UCS_TEST_F(test_callbackq, multi) {
    static const unsigned COUNT = 3;

    callback_ctx ctx[COUNT];

    for (unsigned i = 0; i < COUNT; ++i) {
        init_ctx(&ctx[i]);
        add_sync(&ctx[i]);
    }

    dispatch();
    dispatch();

    for (unsigned i = 0; i < COUNT; ++i) {
        remove_sync(&ctx[i]);
        EXPECT_EQ(2u, ctx[i].count);
    }
}

UCS_TEST_F(test_callbackq, remove_self) {
    callback_ctx ctx;

    init_ctx(&ctx);
    ctx.command = COMMAND_REMOVE_SELF;
    add_sync(&ctx);
    dispatch();
    EXPECT_EQ(1u, ctx.count);

    dispatch();
    dispatch();
    EXPECT_EQ(1u, ctx.count);
}

UCS_TEST_F(test_callbackq, add_another) {
    callback_ctx ctx, ctx2;

    init_ctx(&ctx);
    init_ctx(&ctx2);
    ctx.command = COMMAND_ADD_ANOTHER;
    ctx.to_add  = &ctx2;

    add_sync(&ctx);

    dispatch();
    EXPECT_EQ(1u, ctx.count);
    unsigned count = ctx.count;

    dispatch();
    EXPECT_EQ(2u, ctx.count);
    EXPECT_EQ(count + 1, ctx2.count);

    remove_sync(&ctx);
    dispatch();
    EXPECT_EQ(2u, ctx.count);
    EXPECT_EQ(count + 2, ctx2.count);

    remove_sync(&ctx2);
    remove_sync(&ctx2);
    dispatch();
    EXPECT_EQ(count + 2, ctx2.count);
}

UCS_MT_TEST_F(test_callbackq, threads, 10) {

    static unsigned COUNT = 2000;

    if (barrier()) {
        for (unsigned i = 0; i < COUNT; ++i) {
            /* part 1 */
            dispatch(100); /* simulate race */
            barrier(); /*1*/
            dispatch(5);
            barrier(); /*2*/

            /* part 2 */
            dispatch(100); /* simulate race */
            barrier(); /*3*/
            dispatch(5);
            barrier(); /*4*/
            dispatch(100);
            barrier(); /*5*/
        }
    } else {
        for (unsigned i = 0; i < COUNT; ++i) {
            /* part 1 */
            callback_ctx ctx;
            init_ctx(&ctx);
            add_async(&ctx);
            barrier(); /*1*/
            barrier(); /*2*/  /* dispatch which seen the add command already called */
            EXPECT_GE(ctx.count, 1u);

            /* part 2 */
            remove_async(&ctx);
            barrier(); /*3*/
            barrier(); /*4*/ /* dispatch which seen the remove command already called */
            unsigned count = ctx.count;
            barrier(); /*5*/
            EXPECT_EQ(count, ctx.count);
        }
    }
}

