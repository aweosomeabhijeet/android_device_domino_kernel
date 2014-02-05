// This is a dummy C file to prevent the build fail.
// We can remove this file after we put some source code in this directory(/kernel/kernel/fih_prop/src)

#include <linux/bottom_half.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/cache.h>
#include <linux/jhash.h>
#include <linux/init.h>
#include <linux/times.h>


#ifdef CONFIG_FIH_FXX
int dummy_function(void)
{
    printk("d");
    return 0;
}
#endif
