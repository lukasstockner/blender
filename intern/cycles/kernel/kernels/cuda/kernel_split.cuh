#define CUDA_MULTIPRESSOR_MAX_REGISTERS 65536
#define CUDA_MULTIPROCESSOR_MAX_BLOCKS 16
#define CUDA_BLOCK_MAX_THREADS 1024
#define CUDA_THREAD_MAX_REGISTERS 63

/* tunable parameters */
#define CUDA_THREADS_BLOCK_WIDTH 16
#define CUDA_SPLIT_KERNEL_MAX_REGISTERS 63

#define CUDA_LAUNCH_BOUNDS(threads_block_width, threads_block_height, thread_num_registers) \
        __launch_bounds__( \
                threads_block_width*threads_block_height, \
                CUDA_MULTIPRESSOR_MAX_REGISTERS/(threads_block_width*threads_block_height*thread_num_registers) \
                )

#define SPLIT_KERNEL_BOUNDS CUDA_LAUNCH_BOUNDS(32, 8, CUDA_SPLIT_KERNEL_MAX_REGISTERS)
