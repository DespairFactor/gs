// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Samsung Electronics.
 *
 */

#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/spi/spi.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>

#include "include/gnss.h"
#include "gnss_spi.h"

struct gnss_spi {
	struct spi_device *spi;
	void *dma_rx_buffer;
	dma_addr_t dma_rx_handle;
	void *dma_tx_buffer;
	dma_addr_t dma_tx_handle;
};

static struct gnss_spi gnss_if;
static DEFINE_MUTEX(gnss_spi_lock);

static void gnss_spi_dma_deinit(struct device *dev)
{
	if (gnss_if.dma_rx_buffer) {
		dma_free_coherent(dev, MAX_SPI_RX_SIZE, gnss_if.dma_rx_buffer,
				  gnss_if.dma_rx_handle);
		gnss_if.dma_rx_buffer = NULL;
		gnss_if.dma_rx_handle = 0;
	}
	if (gnss_if.dma_tx_buffer) {
		dma_free_coherent(dev, DEFAULT_SPI_TX_SIZE, gnss_if.dma_tx_buffer,
				  gnss_if.dma_tx_handle);
		gnss_if.dma_tx_buffer = NULL;
		gnss_if.dma_tx_handle = 0;
	}
}

static void gnss_spi_dma_init(struct device *dev)
{
	int ret;

	dev->coherent_dma_mask = DMA_BIT_MASK(64);

	if (!dev->dma_mask)
		dev->dma_mask = &dev->coherent_dma_mask;

	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(64));
	if (ret) {
		gif_err("DMA set mask and coherent failed ret=%d\n", ret);
		return;
	}
	gnss_if.dma_rx_buffer =
		dma_alloc_coherent(dev, MAX_SPI_RX_SIZE, &gnss_if.dma_rx_handle, GFP_KERNEL);
	gnss_if.dma_tx_buffer =
		dma_alloc_coherent(dev, DEFAULT_SPI_TX_SIZE, &gnss_if.dma_tx_handle, GFP_KERNEL);

	if (!gnss_if.dma_rx_buffer || !gnss_if.dma_tx_buffer) {
		gif_err("Failed to allocate coherent memory for gnss-spi\n");
		gnss_spi_dma_deinit(dev);
	}
}

int gnss_spi_send(char *buff, unsigned int size, char *rx_buff)
{
	int ret;
	struct spi_message msg;
	struct spi_transfer tx;
	bool can_dma;
	bool can_tx_dma;
	bool can_rx_dma;

	if (!buff || size == 0)
		return -EINVAL;

	mutex_lock(&gnss_spi_lock);
	if (!gnss_if.spi) {
		mutex_unlock(&gnss_spi_lock);
		return -ENODEV;
	}

	can_tx_dma = gnss_if.dma_tx_buffer && size <= DEFAULT_SPI_TX_SIZE;
	can_rx_dma = !rx_buff || (gnss_if.dma_rx_buffer && size <= MAX_SPI_RX_SIZE);
	can_dma = can_tx_dma && can_rx_dma;

	memset(&tx, 0, sizeof(tx));
	spi_message_init(&msg);
	tx.len = size;
	tx.bits_per_word = SPI_BITS_PER_WORD;

	if (can_dma) {
		/* Assign to the coherent DMA-Safe buffer */
		memcpy(gnss_if.dma_tx_buffer, buff, size);
		tx.tx_dma = gnss_if.dma_tx_handle;
		tx.tx_buf = gnss_if.dma_tx_buffer;
		if (rx_buff) {
			tx.rx_buf = gnss_if.dma_rx_buffer;
			tx.rx_dma = gnss_if.dma_rx_handle;
		}
	} else {
		/* Cannot get DMA-Safe buffer */
		tx.tx_buf = buff;
		if (rx_buff)
			tx.rx_buf = rx_buff;
	}

	spi_message_add_tail(&tx, &msg);
	ret = spi_sync(gnss_if.spi, &msg);
	if (ret < 0)
		gif_err("spi_sync() error:%d\n", ret);

	if (ret == 0 && can_dma && rx_buff)
		memcpy(rx_buff, gnss_if.dma_rx_buffer, size);

	mutex_unlock(&gnss_spi_lock);
	return ret;
}
EXPORT_SYMBOL(gnss_spi_send);

int gnss_spi_recv(char *buff, unsigned int size)
{
	int ret;
	struct spi_message msg;
	struct spi_transfer rx;
	bool can_dma;

	if (!buff || size == 0)
		return -EINVAL;

	mutex_lock(&gnss_spi_lock);
	if (!gnss_if.spi) {
		mutex_unlock(&gnss_spi_lock);
		return -ENODEV;
	}

	can_dma = (gnss_if.dma_rx_buffer && size <= MAX_SPI_RX_SIZE);

	memset(&rx, 0, sizeof(rx));
	spi_message_init(&msg);
	rx.len = size;
	rx.bits_per_word = SPI_BITS_PER_WORD;

	if (can_dma) {
		rx.rx_dma = gnss_if.dma_rx_handle;
		rx.rx_buf = gnss_if.dma_rx_buffer;
	} else {
		rx.rx_buf = buff;
	}
	spi_message_add_tail(&rx, &msg);
	ret = spi_sync(gnss_if.spi, &msg);
	if (ret < 0)
		gif_err("spi_sync() error:%d\n", ret);

	if (ret == 0 && can_dma)
		memcpy(buff, gnss_if.dma_rx_buffer, size);

	mutex_unlock(&gnss_spi_lock);
	return ret;
}
EXPORT_SYMBOL(gnss_spi_recv);

static int gnss_spi_probe(struct spi_device *spi)
{
	if (spi_setup(spi)) {
		gif_err("ERR! spi_setup fail\n");
		return -EINVAL;
	}

	mutex_lock(&gnss_spi_lock);
	if (gnss_if.spi) {
		gif_err("GNSS SPI device already registered!\n");
		mutex_unlock(&gnss_spi_lock);
		return -EBUSY;
	}

	gnss_if.spi = spi;
	if (IS_ENABLED(CONFIG_EXYNOS_GNSS_IF_SPI_DMA_SAFE_BUFFER))
		gnss_spi_dma_init(&spi->dev);
	mutex_unlock(&gnss_spi_lock);

	return 0;
}

static void gnss_spi_remove(struct spi_device *spi)
{
	mutex_lock(&gnss_spi_lock);
	gnss_if.spi = NULL;
	if (IS_ENABLED(CONFIG_EXYNOS_GNSS_IF_SPI_DMA_SAFE_BUFFER))
		gnss_spi_dma_deinit(&spi->dev);
	mutex_unlock(&gnss_spi_lock);
}

static const struct spi_device_id gnss_spi_id[] = {
	{ "gnss-spi", 0 },
	{}
};
MODULE_DEVICE_TABLE(spi, gnss_spi_id);

static const struct of_device_id gnss_spi_dt_match[] = {
	{ .compatible = "samsung,gnss-spi" },
	{},
};
MODULE_DEVICE_TABLE(of, gnss_spi_dt_match);

static struct spi_driver gnss_spi_driver = {
	.probe = gnss_spi_probe,
	.remove = gnss_spi_remove,
	.driver = {
		.name = "gnss_spi",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(gnss_spi_dt_match),
		.suppress_bind_attrs = true,
	},
	.id_table = gnss_spi_id,
};

module_spi_driver(gnss_spi_driver);

MODULE_DESCRIPTION("Exynos SPI driver for GNSS communication");
MODULE_LICENSE("GPL");
