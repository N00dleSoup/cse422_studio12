#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/sched.h>
#include <linux/gfp.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/time.h>
#include <linux/kthread.h>
#include <linux/mm.h>

#include <asm/uaccess.h>

static uint nr_structs = 2000;
module_param(nr_structs, uint, 0644); 

static struct task_struct * kthread = NULL;
static struct page * pages = NULL;

unsigned int nr_pages, order, nr_structs_per_page;


typedef struct {
	unsigned int array[8];
} datatype_t;

static unsigned int
my_get_order(unsigned int value)
{
    unsigned int shifts = 0;

    if (!value)
        return 0;

    if (!(value & (value - 1)))
        value--;

    while (value > 0) {
        value >>= 1;
        shifts++;
    }

    return shifts;
}

static int
thread_fn(void * data)
{
    printk("Hello from thread %s. nr_structs=%u\n", current->comm, nr_structs);
	printk("Page_size: %d, Datatype_t size: %d, datatype_t/page: %d\n", 
				PAGE_SIZE, sizeof(datatype_t), PAGE_SIZE/sizeof(datatype_t));

	nr_structs_per_page = PAGE_SIZE/sizeof(datatype_t);
	nr_pages = nr_structs / nr_structs_per_page;
	order = my_get_order(nr_pages);
	printk("Structs/page: %d, nr_pages: %d, order: %d\n", nr_structs_per_page, nr_pages, order);

	pages = alloc_pages(GFP_KERNEL, order);
	if(pages == NULL) {
		printk(KERN_ERR "Unable to alloc pages\n");
	}
	
	int i,j,k;
	for(i = 0; i < nr_pages; ++i) {
		struct page * curr_page = &pages[i];
		//printk(KERN_DEBUG "curr page addr: %p\n", curr_page);
		unsigned long pfn = page_to_pfn(curr_page);
		datatype_t * base = (datatype_t *)__va(PFN_PHYS(pfn)); //first struct in curr_page
		for(j=0; j < nr_structs_per_page; ++j) {
			datatype_t * this_struct = &base[j];
			for(k = 0; k < 8; ++k) {
				this_struct->array[k] = i*nr_structs_per_page*8 + j*8 + k;
				if (j==0 && k==0) {
					//printk("%x\n", this_struct->array[k]);
				}
			}
		}
	}
//TODO: start 5

    while (!kthread_should_stop()) {
        schedule();
    }




    return 0;
}

static int
kernel_memory_init(void)
{
    printk(KERN_INFO "Loaded kernel_memory module\n");

    kthread = kthread_create(thread_fn, NULL, "k_memory");
    if (IS_ERR(kthread)) {
        printk(KERN_ERR "Failed to create kernel thread\n");
        return PTR_ERR(kthread);
    }
    
    wake_up_process(kthread);

    return 0;
}

static void 
kernel_memory_exit(void)
{
    kthread_stop(kthread);

	int i,j,k;
	for(i = 0; i < nr_pages; ++i) {
		datatype_t * base = (datatype_t *) page_address(&pages[i]);
		for(j = 0; j < nr_structs_per_page; ++j) {
			datatype_t * curr_struct = &base[j];
			for(k = 0; k < 8; ++k) {
				if (curr_struct->array[k] != (i*nr_structs_per_page*8 + j*8 + k)) {
					printk(KERN_ERR "Wrong value at addr %p (i=%d, j=%d, k=%d)\n", 
							&curr_struct->array[k], i, j, k);
				}
			}
		}
	}
	__free_pages(pages, order);
	printk(KERN_DEBUG "Successful unload!\n");
}


module_init(kernel_memory_init);
module_exit(kernel_memory_exit);

MODULE_LICENSE ("GPL");
