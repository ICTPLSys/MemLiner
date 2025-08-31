#include <linux/syscalls.h>
#include <linux/printk.h>
#include <linux/swap_global_struct_mem_layer.h>
#include <linux/vmalloc.h>

//
// Variables
//

// For debug
// Declared in linux/swap_global_struct_mem_layer.h
struct epoch_struct *user_kernel_shared_data = NULL;




//
// Functions
//


/**
 * @brief Create the user-kernel shared memory pool
 * Let user space and kernel space share a range of memory.
 * We also provide a lock to synchronize the read/write between user space
 * and kernel space (!TODO)
 *
 */

// check the structure of the epoch_struct
//  |--4 bytes for eppch --|-- 4 bytes for legnth --|-- unsigned char array --|
void intialize_epoch_struct(struct epoch_struct* cur_epoch, unsigned long byte_size)
{
  memset(cur_epoch, 0, byte_size); // 0 is MAPPED
  cur_epoch->epoch = 0;
  cur_epoch->length = (byte_size - 8)/sizeof(char);

  //debug
  pr_warn("%s, get epoch_struct with legnth 0x%x items\n", __func__, cur_epoch->length);
}

// Only monitor the page status for the range of Java heap.
bool within_memliner_range(unsigned long user_addr)
{
	if( user_addr >= SEMERU_START_ADDR && user_addr < SEMERU_END_ADDR  ){
		return 1;
	}

	return 0;
}


unsigned long virt_addr_to_page_stat_offset(unsigned long virt_addr){
	if(within_memliner_range(virt_addr)){
  		return (virt_addr - SEMERU_START_ADDR)>>PAGE_SHIFT;
	}
	return -1; // the max value of the unsignged long
}

// Is this necessary to be an atomic operation?
// Caller must solve the concurrency problem.
void mark_page_stat(unsigned long user_virt_addr, enum page_stat state){
	unsigned long page_index;

	page_index = virt_addr_to_page_stat_offset(user_virt_addr);
	if(page_index!= (unsigned long)-1){

  		user_kernel_shared_data->page_stats[page_index] = state;
	}

#if defined(DEBUG_SWAP_SLOW_BRIEF)
	pr_err("%s, user_virt_addr 0x%lx exceed the Java heap range[0x%lx, 0x%lx)",
		__func__, user_virt_addr, SEMERU_START_ADDR, SEMERU_END_ADDR);
#endif
}


/**
 * @brief Pass the allocated user space virtual memory range to kernel.
 *	The shared memory range is used to store the struct, epoch_struct.
 *
 *  Structure of the epoch_struct
 *  |--4 bytes for eppch --|-- 4 bytes for legnth --|-- unsigned char array --|
 *
 * @param uaddr the user space start addr of the user-kernel shared buffer.
 * @param size  byte size of the user-kernel shared buffer.
 */
SYSCALL_DEFINE2(mmap_user_kernel_shared_mem, int __user *, user_buf,
		unsigned long, size)
{
	struct vm_area_struct *vma;
	struct task_struct *tsk;
	struct mm_struct *mm;
	unsigned long uaddr = (unsigned long)user_buf;
	int ret = 0;

	// 1) Build the corresponding vm_area_struct of the allocated
	// user space virtual memory range
	pr_warn("%s, Entered mmap_user_kernel_shared_mem \n with parameters: uaddr: 0x%lx, size 0x%lx \n",
		__func__, uaddr, size );

	// Prefetch the cache line at X (Prefetch the mm_struct->mmap_lock)
	prefetchw(&current->mm->mmap_lock); // do we need to aquire this lock?

	// Confrim the virtual memory range is in user space
	//if (unlikely(fault_in_kernel_space(uaddr)))
	//	goto err;

	tsk = current; // Get the task_struct of the user process
	mm = tsk->mm;

	if(unlikely(!down_write_trylock(&mm->mmap_lock))) {
		pr_err("%s, failed  to acquire the read lock of mm->mmap_lock\n", __func__);
		ret = -1;
		goto out;
	}

	vma = find_vma(mm, uaddr);
	// VM_MIXEDMAP is needed for remap_vmalloc_range_partial
	// The physical pages are controlled by both the MMU and I/O devices
	vma->vm_flags |= VM_MIXEDMAP;
	// remove the VM_PFNMAP for remap_vmalloc_range_partial
	// the physical pages are only controlled by I/O devices
	vma->vm_flags &= ~VM_PFNMAP;

	// release the readlock of mm->mmap_lock
	up_write(&mm->mmap_lock);

	// saint checks
	if (unlikely(!vma)) {
		pr_err("%s: Cannot find vma for the virtual address 0x%lx\n", __func__, uaddr);
		ret = -1;
		goto out;
	}
	if(unlikely(vma->vm_start > uaddr)){
		pr_err("%s, start addr 0x%lx exceed the range of the vma[0x%lx, 0x%lx)\n",
			__func__, uaddr,  vma->vm_start, vma->vm_end);
		ret = -1;
		goto out;
	}
	pr_warn("%s, Pass the vma [start 0x%lx, end 0x%lx), vm_pgoff 0x%lx \n",
		__func__, vma->vm_start, vma->vm_end, vma->vm_pgoff);

	// 2) Kernel allocates memory for the user application
	user_kernel_shared_data = vmalloc_user(size);
	if (!user_kernel_shared_data) {
		ret = -ENOMEM;
		goto out;
	}
	// touch the range to allocate physical memory
	// Warning: the init value (char 8 bytes), 2, is for debuging.
	//memset(user_kernel_shared_data, 2, size);
	//pr_warn("%s, kernel allocated bufer [0x%lx, 0x%lx), and initialize to 2\n",
	//		__func__, (unsigned long)user_kernel_shared_data, size);
	intialize_epoch_struct(user_kernel_shared_data, size);

	// 3) Fill the vm_area_struct with kernel pages
	// offset within the kernel space range is 0.
	if(unlikely( ret=remap_vmalloc_range(vma, (void*)user_kernel_shared_data, 0))){
		pr_err("%s, remap_vmalloc_range failed with err code %d\n", __func__, ret );
		goto out;
	}

	pr_warn("%s, user space memory[0x%lx, 0x%lx) is mapped to kernel space memory [0x%lx, 0x%lx)\n",
		__func__, uaddr, uaddr+size, (unsigned long)user_kernel_shared_data, (unsigned long)(user_kernel_shared_data + size) );

out:
	return ret;
}
