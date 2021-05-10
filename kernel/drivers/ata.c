#include <drivers/ata.h>
#include <x86/interrupt.h>
#include <x86/io.h>
#include <mm/mm.h>
#include <stderr.h>
#include <string.h>
#include <stdio.h>

#define MAX_ATA_DEVICE      4

/*
 * ata devices */
static struct ata_device_t ata_devices[MAX_ATA_DEVICE];

/*
 * Polling an ata device.
 */
static uint8_t ata_polling(struct ata_device_t *device)
{
  uint8_t status;

  while (1) {
    status = inb(device->io_base + ATA_REG_STATUS);

    if (!(status & ATA_SR_BSY) || (status & ATA_SR_DRQ))
      return 0;

    if ((status & ATA_SR_ERR) || (status & ATA_SR_DF))
      return -ENXIO;
  }
}

/*
 * Write a sector from an ata device.
 */
static int ata_write_one_sector(struct ata_device_t *device, uint32_t lba, uint16_t *buffer)
{
  uint8_t cmd;
  int i;

  if (device->drive == ATA_MASTER)
    cmd = 0xE0;
  else
    cmd = 0xF0;

  /* wait for disk to be ready */
  if (ata_polling(device) != 0)
    return -ENXIO;

  /* send write command */
  outb(device->io_base + ATA_REG_HDDEVSEL, cmd | ((lba >> 24) & 0x0F));

  /* set write parameters */
  outb(device->io_base + 1, 0x00);
  outb(device->io_base + ATA_REG_SECCOUNT0, 1);
  outb(device->io_base + ATA_REG_LBA0, (uint8_t) lba);
  outb(device->io_base + ATA_REG_LBA1, (uint8_t) (lba >> 8));
  outb(device->io_base + ATA_REG_LBA2, (uint8_t) (lba >> 16));
  outb(device->io_base + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);

  /* wait for disk to be ready */
  if (ata_polling(device) != 0)
    return -ENXIO;

  /* write sector word by word */
  for (i = 0; i < 256; i++) {
    outw(device->io_base, buffer[i]);
    outw(device->io_base + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
  }

  return 0;
}

/*
 * Write from an ata device.
 */
int ata_write(struct ata_device_t *device, uint32_t lba, uint32_t nb_sectors, uint16_t *buffer)
{
  int ret = 0;
  uint32_t i;

  /* write sector by sector */
  for (i = 0; i < nb_sectors; i++, buffer += 256) {
    ret = ata_write_one_sector(device, lba + i, buffer);
    if (ret != 0)
      return ret;
  }

  return ret;
}

/*
 * Read a sector from an ata device.
 */
static int ata_read_one_sector(struct ata_device_t *device, uint32_t lba, uint16_t *buffer)
{
  uint8_t cmd;
  int i;

  if (device->drive == ATA_MASTER)
    cmd = 0xE0;
  else
    cmd = 0xF0;

  /* wait for disk to be ready */
  if (ata_polling(device) != 0)
    return -ENXIO;

  /* send read command */
  outb(device->io_base + ATA_REG_HDDEVSEL, cmd | ((lba >> 24) & 0x0F));

  /* set read parameters */
  outb(device->io_base + 1, 0x00);
  outb(device->io_base + ATA_REG_SECCOUNT0, 1);
  outb(device->io_base + ATA_REG_LBA0, (uint8_t) lba);
  outb(device->io_base + ATA_REG_LBA1, (uint8_t) (lba >> 8));
  outb(device->io_base + ATA_REG_LBA2, (uint8_t) (lba >> 16));
  outb(device->io_base + ATA_REG_COMMAND, ATA_CMD_READ_PIO);

  /* wait for disk to be ready */
  if (ata_polling(device) != 0)
    return -ENXIO;

  /* read sector word by word */
  for (i = 0; i < 256; i++)
    buffer[i] = inw(device->io_base + ATA_REG_DATA);

  return 0;
}

/*
 * Read from an ata device.
 */
int ata_read(struct ata_device_t *device, uint32_t lba, uint32_t nb_sectors, uint16_t *buffer)
{
  uint32_t i;
  int ret = 0;

  /* read sector by sector */
  for (i = 0; i < nb_sectors; i++, buffer += 256) {
    ret = ata_read_one_sector(device, lba + i, buffer);
    if (ret != 0)
      return ret;
  }

  return ret;
}

/*
 * Identify a device.
 */
static uint8_t ata_identify(struct ata_device_t *device)
{
  uint8_t status;
  int i;

  /* select drive */
  if (device->drive == ATA_MASTER)
    outb(device->io_base + ATA_REG_HDDEVSEL, 0xA0);
  else
    outb(device->io_base + ATA_REG_HDDEVSEL, 0xB0);

  /* reset ata registers */
  outb(device->io_base + ATA_REG_SECCOUNT0, 0);
  outb(device->io_base + ATA_REG_LBA0, 0);
  outb(device->io_base + ATA_REG_LBA2, 0);
  outb(device->io_base + ATA_REG_LBA0, 0);

  /* identify drive */
  outb(device->io_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

  /* wait until BSY is clear */
  while (1) {
    status = inb(device->io_base + ATA_REG_STATUS);
    if (!status)
      return -ENXIO;

    if (!(status & ATA_SR_BSY))
      break;
  }

  /* wait until DRQ (drive has data to transfer) is clear */
  while (1) {
    status = inb(device->io_base + ATA_REG_STATUS);
    if (status & ATA_SR_ERR)
      return -EFAULT;

    if (status & ATA_SR_DRQ)
      break;
  }

  /* read identified drive data */
  for (i = 0; i < 256; i += 2)
    inw(device->io_base + ATA_REG_DATA);

  return 0;
}

/*
 * Detect an ATA device.
 */
static int ata_detect(int id, uint16_t bus, uint8_t drive)
{
  int ret;

  if (id > MAX_ATA_DEVICE)
    return -ENXIO;

  /* set device */
  ata_devices[id].bus = bus;
  ata_devices[id].drive = drive;
  ata_devices[id].io_base = bus == ATA_PRIMARY ? ATA_PRIMARY_IO : ATA_SECONDARY_IO;

  /* identify device */
  ret = ata_identify(&ata_devices[id]);

  /* reset device on error */
  if (ret)
    memset(&ata_devices[id], 0, sizeof(struct ata_device_t));

  return ret;
}

/*
 * Irq handler.
 */
static void ata_irq_handler(struct registers_t *regs)
{
  UNUSED(regs);
}

/*
 * Get an ata device.
 */
struct ata_device_t *ata_get_device(int dev_num)
{
  if (dev_num >= MAX_ATA_DEVICE)
    return NULL;

  return &ata_devices[dev_num];
}

/*
 * Init ata devices.
 */
void init_ata()
{
  /* reset ata devices */
  memset(ata_devices, 0, sizeof(ata_devices));

  /* register interrupt handlers */
  register_interrupt_handler(IRQ14, ata_irq_handler);
  register_interrupt_handler(IRQ15, ata_irq_handler);

  /* detect hard drives */
  if (ata_detect(0, ATA_PRIMARY, ATA_MASTER) == 0)
    printf("[Kernel] Primary ATA master drive detected\n");
  if (ata_detect(1, ATA_PRIMARY, ATA_SLAVE) == 0)
    printf("[Kernel] Primary ATA slave drive detected\n");
  if (ata_detect(2, ATA_SECONDARY, ATA_MASTER) == 0)
    printf("[Kernel] Secondary ATA master drive detected\n");
  if (ata_detect(3, ATA_SECONDARY, ATA_SLAVE) == 0)
    printf("[Kernel] Secondary ATA slave drive detected\n");
}
