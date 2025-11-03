/*
 * Basic io_uring File Read Example
 * 
 * This program demonstrates the fundamental usage of io_uring for file I/O operations.
 * It initializes an io_uring instance, reads a file using the io_uring interface,
 * and displays the contents.
 * 
 * Target: Linux kernel 6.17+
 * Dependencies: liburing (2.11+)
 * 
 * Author: io_uring_play project
 * Date: 2025-10-20
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <liburing.h>

/* Configuration constants */
#define QUEUE_DEPTH 32
#define BUFFER_SIZE 4096
#define TEST_FILE "test_data.txt"

/* Error handling macro - fail fast with descriptive messages */
#define DIE(msg) do { \
    fprintf(stderr, "Error: %s: %s\n", msg, strerror(errno)); \
    exit(EXIT_FAILURE); \
} while(0)

/*
 * Initialize io_uring instance
 * 
 * Creates and initializes an io_uring queue with the specified depth.
 * Uses standard initialization without advanced flags for simplicity.
 * 
 * @param ring: Pointer to io_uring structure to initialize
 * @return: 0 on success, negative error code on failure
 */
static int setup_io_uring(struct io_uring *ring)
{
    int ret;
    
    /* Initialize io_uring with queue depth of 32 */
    ret = io_uring_queue_init(QUEUE_DEPTH, ring, 0);
    if (ret < 0) {
        fprintf(stderr, "Error: io_uring_queue_init failed: %s\n", 
                strerror(-ret));
        return ret;
    }
    
    printf("[+] io_uring initialized successfully (queue depth: %d)\n", 
           QUEUE_DEPTH);
    return 0;
}

/*
 * Perform file read using io_uring
 * 
 * This function demonstrates the core io_uring workflow:
 * 1. Get a submission queue entry (SQE)
 * 2. Prepare the read operation
 * 3. Submit the operation to the kernel
 * 4. Wait for completion
 * 5. Process the completion queue entry (CQE)
 * 
 * @param ring: Initialized io_uring instance
 * @param fd: File descriptor to read from
 * @param buffer: Buffer to read data into
 * @param size: Size of buffer
 * @return: Number of bytes read, or negative error code
 */
static int read_file_with_uring(struct io_uring *ring, int fd, 
                                 char *buffer, size_t size)
{
    struct io_uring_sqe *sqe;
    struct io_uring_cqe *cqe;
    int ret;
    
    /* Step 1: Get a submission queue entry */
    sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        fprintf(stderr, "Error: Could not get SQE\n");
        return -1;
    }
    
    printf("[+] Got submission queue entry\n");
    
    /* Step 2: Prepare the read operation
     * io_uring_prep_read() sets up the SQE for a read operation:
     * - sqe: The submission queue entry to prepare
     * - fd: File descriptor to read from
     * - buffer: Where to store the read data
     * - size: How many bytes to read
     * - offset: File offset to read from (0 = start of file)
     */
    io_uring_prep_read(sqe, fd, buffer, size, 0);
    
    printf("[+] Prepared read operation (buffer size: %zu bytes)\n", size);
    
    /* Step 3: Submit the operation to the kernel
     * This tells the kernel to process the queued operations
     */
    ret = io_uring_submit(ring);
    if (ret < 0) {
        fprintf(stderr, "Error: io_uring_submit failed: %s\n", 
                strerror(-ret));
        return ret;
    }
    
    printf("[+] Submitted operation to kernel (%d operation(s))\n", ret);
    
    /* Step 4: Wait for completion
     * This blocks until at least one operation completes
     */
    ret = io_uring_wait_cqe(ring, &cqe);
    if (ret < 0) {
        fprintf(stderr, "Error: io_uring_wait_cqe failed: %s\n", 
                strerror(-ret));
        return ret;
    }
    
    printf("[+] Operation completed\n");
    
    /* Step 5: Check the result from the completion queue entry
     * cqe->res contains the return value of the read operation:
     * - Positive value: Number of bytes read
     * - Zero: End of file
     * - Negative value: Error code
     */
    if (cqe->res < 0) {
        fprintf(stderr, "Error: Read operation failed: %s\n", 
                strerror(-cqe->res));
        io_uring_cqe_seen(ring, cqe);
        return cqe->res;
    }
    
    ret = cqe->res;
    
    /* Mark the completion queue entry as processed
     * This frees it for reuse
     */
    io_uring_cqe_seen(ring, cqe);
    
    return ret;
}

/*
 * Main program entry point
 */
int main(void)
{
    struct io_uring ring;
    char buffer[BUFFER_SIZE];
    int fd, bytes_read;
    
    printf("=== io_uring Basic File Read Example ===\n\n");
    
    /* Initialize io_uring */
    if (setup_io_uring(&ring) < 0) {
        return EXIT_FAILURE;
    }
    
    /* Open the test file */
    printf("[+] Opening file: %s\n", TEST_FILE);
    fd = open(TEST_FILE, O_RDONLY);
    if (fd < 0) {
        DIE("Failed to open file");
    }
    printf("[+] File opened successfully (fd: %d)\n", fd);
    
    /* Clear the buffer */
    memset(buffer, 0, BUFFER_SIZE);
    
    /* Perform the read operation using io_uring */
    printf("\n--- Performing io_uring read operation ---\n");
    bytes_read = read_file_with_uring(&ring, fd, buffer, BUFFER_SIZE - 1);
    
    if (bytes_read < 0) {
        close(fd);
        io_uring_queue_exit(&ring);
        return EXIT_FAILURE;
    }
    
    /* Ensure null termination for printing */
    buffer[bytes_read] = '\0';
    
    /* Display results */
    printf("\n--- Read Results ---\n");
    printf("Bytes read: %d\n", bytes_read);
    printf("\n--- File Contents ---\n");
    printf("%s", buffer);
    if (bytes_read > 0 && buffer[bytes_read - 1] != '\n') {
        printf("\n");  /* Add newline if file doesn't end with one */
    }
    printf("--- End of Contents ---\n\n");
    
    /* Cleanup */
    printf("[+] Cleaning up resources\n");
    close(fd);
    io_uring_queue_exit(&ring);
    
    printf("[+] Success! io_uring example completed.\n");
    
    return EXIT_SUCCESS;
}
