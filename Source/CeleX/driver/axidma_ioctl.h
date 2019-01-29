#ifndef AXIDMA_IOCTL_H_
#define AXIDMA_IOCTL_H_

#include <asm/ioctl.h>              // IOCTL macros

/*----------------------------------------------------------------------------
 * IOCTL Defintions
 *----------------------------------------------------------------------------*/

// The standard name for the AXI DMA device
#define AXIDMA_DEV_NAME     "axidma"

// The standard path to the AXI DMA device
#define AXIDMA_DEV_PATH     ("/dev/" AXIDMA_DEV_NAME)

/*----------------------------------------------------------------------------
 * IOCTL Argument Definitions
 *----------------------------------------------------------------------------*/

// Forward declaration of the kernel's DMA channel type (opaque to userspace)
struct dma_chan;

// Direction from the persepctive of the processor
/**
 * Enumeration for direction in a DMA transfer.
 *
 * The enumeration has two directions: write is from the processor to the
 * FPGA, and read is from the FPGA to the processor.
 **/
enum axidma_dir {
    AXIDMA_WRITE,                   ///< Transmits from memory to a device.
    AXIDMA_READ                     ///< Transmits from a device to memory.
};


// TODO: Channel really should not be here
struct axidma_chan {
    enum axidma_dir dir;            // The DMA direction of the channel
    int channel_id;                 // The identifier for the device
    const char *name;               // Name of the channel (ignore)
    struct dma_chan *chan;          // The DMA channel (ignore)
};

struct axidma_num_channels {
    int num_channels;               // Total DMA channels in the system
    int num_dma_tx_channels;        // DMA transmit channels available
    int num_dma_rx_channels;        // DMA receive channels available
};

struct axidma_channel_info {
    struct axidma_chan *channels;   // Metadata about all available channels
};

struct axidma_transaction {
    int timeout;                      // Indicates if the call is blocking
    int channel_id;                 // The id of the DMA channel to use
    void *buf;                      // The buffer used for the transaction
    size_t buf_len;                 // The length of the buffer
    size_t period_len;
};

struct axidma_inout_transaction {
    int timeout;                      // Indicates if the call is blocking
    int tx_channel_id;              // The id of the transmit DMA channel
    void *tx_buf;                   // The buffer containing the data to send
    size_t tx_buf_len;              // The length of the transmit buffer
    int rx_channel_id;              // The id of the receive DMA channel
    void *rx_buf;                   // The buffer to place the data in
    size_t rx_buf_len;              // The length of the receive buffer
};

struct camera_register_buffer {
    unsigned int address;             // Rigister Address
    unsigned int mask;                // The size of the external DMA buffer
    unsigned int value;               // User virtual address of the buffer
};

/*----------------------------------------------------------------------------
 * IOCTL Interface
 *----------------------------------------------------------------------------*/

// The magic number used to distinguish IOCTL's for our device
#define AXIDMA_IOCTL_MAGIC              'W'

// The number of IOCTL's implemented, used for verification
#define AXIDMA_NUM_IOCTLS               15

/**
 * Returns the number of available DMA channels in the system.
 *
 * This writes to the structure given as input by the user, telling the
 * numbers for all DMA channels in the system.
 *
 * Outputs:
 *  - num_channels - The total number of DMA channels in the system.
 *  - num_dma_tx_channels - The number of transmit AXI DMA channels
 *  - num_dma_rx_channels - The number of receive AXI DMA channels
 **/
#define AXIDMA_GET_NUM_DMA_CHANNELS     _IOW(AXIDMA_IOCTL_MAGIC, 0, \
                                             struct axidma_num_channels)

/**
 * Returns information on all DMA channels in the system.
 *
 * This function writes to the array specified in the pointer given to the
 * struct structures representing all data about a given DMA channel (device id,
 * direction, etc.). The array must be able to hold at least the number of
 * elements returned by AXIDMA_GET_NUM_DMA_CHANNELS.
 *
 * The important piece of information returned in the id for the channels.
 * This, along with the direction and type, uniquely identifies a DMA channel
 * in the system, and this is how you refer to a channel in later calls.
 *
 * Inputs:
 *  - channels - A pointer to a region of memory that can hold at least
 *               num_channels * sizeof(struct axidma_chan) bytes.
 *
 * Outputs:
 *  - channels - An array of structures of the following format:
 *  - An array of structures with the following fields:
 *       - dir - The direction of the channel (either read or write).
 *       - type - The type of the channel (either normal DMA or video DMA).
 *       - channel_id - The integer id for the channel.
 *       - chan - This field has no meaning and can be safely ignored.
 **/
#define AXIDMA_GET_DMA_CHANNELS         _IOR(AXIDMA_IOCTL_MAGIC, 1, \
                                             struct axidma_channel_info)

/**
 * Register the given signal to be sent when DMA transactions complete.
 *
 * This function sets up an asynchronous signal to be delivered to the invoking
 * process any DMA subsystem completes a transaction. If the user dispatches
 * an asynchronous transaction, and wants to know when it completes, they must
 * register a signal to be delivered.
 *
 * The signal must be one of the POSIX real time signals. So, it must be
 * between the signals SIGRTMIN and SIGRTMAX. The kernel will deliver the
 * channel id back to the userspace signal handler.
 *
 * This can be used to have a user callback function, effectively emulating an
 * interrupt in userspace. The user must register their signal handler for
 * the specified signal for this to happen.
 *
 * Inputs:
 *  - signal - The signal to send upon transaction completion.
 **/
#define AXIDMA_SET_DMA_SIGNAL           _IO(AXIDMA_IOCTL_MAGIC, 2)

/**
 * Receives the data from the logic fabric into the processing system.
 *
 * This function receives data from a device on the PL fabric through
 * AXI DMA into memory. The device id should be an id that is returned by the
 * get dma channels ioctl. The user can specify if the call should wait for the
 * transfer to complete, or if it should return immediately.
 *
 * The specified buffer must be within an address range that was allocated by a
 * call to mmap with the AXI DMA device. Also, the buffer must be able to hold
 * at least `buf_len` bytes.
 *
 * Inputs:
 *  - wait - Indicates if the call should be blocking or non-blocking
 *  - channel_id - The id for the channel you want receive data over.
 *  - buf - The address of the buffer you want to receive the data in.
 *  - buf_len - The number of bytes to receive.
 **/
#define AXIDMA_DMA_READ                 _IOR(AXIDMA_IOCTL_MAGIC, 3, \
                                             struct axidma_transaction)

/**
 * Sends the given data from the processing system to the logic fabric.
 *
 * This function sends data from memory to a device on the PL fabric through
 * AXI DMA. The device id should be an id that is returned by the get dma
 * channels ioctl. The user can specify if the call should wait for the transfer
 * to complete, or if it should return immediately.
 *
 * The specified buffer must be within an address range that was allocated by a
 * call to mmap with the AXI DMA device. Also, the buffer must be able to hold
 * at least `buf_len` bytes.
 *
 * Inputs:
 *  - wait - Indicates if the call should be blocking or non-blocking
 *  - channel_id - The id for the channel you want to send data over.
 *  - buf - The address of the data you want to send.
 *  - buf_len - The number of bytes to send.
 **/
#define AXIDMA_DMA_WRITE                _IOR(AXIDMA_IOCTL_MAGIC, 4, \
                                             struct axidma_transaction)

/**
 * Performs a two-way transfer between the logic fabric and processing system.
 *
 * This function both sends data to the PL and receives data from the PL fabric.
 * It is intended for DMA transfers that are tightly coupled together
 * (e.g. converting an image to grayscale on the PL fabric). The device id's for
 * both channels should be ones that are returned by the get dma ioctl. The user
 * can specify if the call should block. If it blocks, it will wait until the
 * receive transaction completes.
 *
 * The specified buffers must be within an address range that was allocated by a
 * call to mmap with the AXI DMA device. Also, each buffer must be able to hold
 * at least the number of bytes that are being transfered.
 *
 * Inputs:
 *  - wait - Indicates if the call should be blocking or non-blocking
 *  - tx_channel_id - The id for the channel you want transmit data on.
 *  - tx_buf - The address of the data you want to send.
 *  - tx_buf_len - The number of bytes you want to send.
 *  - rx_buf - The address of the buffer you want to receive data in.
 *  - rx_buf_len - The number of bytes you want to receive.
 **/
#define AXIDMA_DMA_READWRITE            _IOR(AXIDMA_IOCTL_MAGIC, 5, \
                                             struct axidma_inout_transaction)


/**
 * Stops all transactions on the given DMA channel.
 *
 * This function flushes all in-progress transactions, and discards all pending
 * transactions on the given DMA channel. The specified id should be one that
 * was returned by the get dma channels ioctl.
 *
 * Inputs:
 *  - dir - The direction of the channel (either read or write).
 *  - type - The type of the channel (either normal DMA or video DMA).
 *  - channel_id - The integer id for the channel.
 *  - chan - This field is unused an can be safely left uninitialized.
 */
#define AXIDMA_STOP_DMA_CHANNEL         _IOR(AXIDMA_IOCTL_MAGIC, 6, \
                                             struct axidma_chan)

#define CAMERA_SET_BRIGHTNESS           _IO(AXIDMA_IOCTL_MAGIC, 7)

#define CAMERA_SET_CONTRAST             _IO(AXIDMA_IOCTL_MAGIC, 8)

#define CAMERA_SET_THERSHOLD            _IO(AXIDMA_IOCTL_MAGIC, 9)

#define CAMERA_SET_MODE                 _IO(AXIDMA_IOCTL_MAGIC, 10)

#define CAMERA_SET_REGISTER             _IOR(AXIDMA_IOCTL_MAGIC, 11, \
                                             struct camera_register_buffer)

#define CAMERA_GET_REGISTER             _IOR(AXIDMA_IOCTL_MAGIC, 12, \
                                             struct camera_register_buffer)

#define CAMERA_GET_BUFPOS               _IOR(AXIDMA_IOCTL_MAGIC, 13, \
                                             int)
#define CAMERA_SET_ZHENGLV             _IO(AXIDMA_IOCTL_MAGIC, 14)

#endif /* AXIDMA_IOCTL_H_ */
