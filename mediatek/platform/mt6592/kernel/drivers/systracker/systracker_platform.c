#include "mach/mt_reg_base.h"
#include "mach/systracker.h"
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <mach/sync_write.h>
#include <linux/kallsyms.h>
#include <linux/init.h>


static struct platform_device systracker_device = 
{    
    .name = "systracker",
    .id = 0,
    .dev = 
    {
    },
};

/*
 * mt_systracker_init: initialize driver.
 * Always return 0.
 */

static int __init mt_systracker_init(void)
{
	int err;
	
  err = platform_device_register(&(systracker_device));
  if (err) {
      pr_err("Fail to register_systracker_device");
      return err;
  }else{
      printk("systracker init done\n");
  }  
	
  return 0;
}

arch_initcall(mt_systracker_init);
MODULE_AUTHOR("Sten Liao <sten.liao@mediatek.com>");
MODULE_DESCRIPTION("MT6592 system tracker Driver v1.0");
