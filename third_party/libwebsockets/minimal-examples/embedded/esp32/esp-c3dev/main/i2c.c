#include "i2c.h"
	
int
lws_i2c_command(lws_i2c_ops_t *ctx, uint8_t ads, uint8_t c)
{
	if (ctx->start(ctx))
		return 1;

	if (ctx->write(ctx, ads << 1)) {
		ctx->stop(ctx);

		return 1;
	}

	ctx->write(ctx, 0);
	ctx->write(ctx, c);
	ctx->stop(ctx);

	return 0;
}

int
lws_i2c_command_list(lws_i2c_ops_t *ctx, uint8_t ads, const uint8_t *buf, size_t len)
{
	while (len--)
		if (lws_i2c_command(ctx, ads, *buf++))
			return 1;

	return 0;
}


