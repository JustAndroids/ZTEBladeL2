#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kallsyms.h>
#include <linux/cpu.h>
#include <linux/smp.h>
#include <asm/cacheflush.h>
#include <asm/outercache.h>
#include <asm/system.h>
#include <asm/delay.h>
#include <mach/mt_reg_base.h>
#include <mach/mt_clkmgr.h>
#include <mach/mt_freqhopping.h>
//#include <mach/emi_bwl.h>
#include <mach/mt_typedefs.h>
#include <mach/memory.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <mach/dfe_drv.h>
#include <mach/mt6333.h>
#include <mach/mt_clkmgr.h>
#include <mach/dma.h>
#include <linux/dma-mapping.h>
#include <asm/io.h>
#include "mach/sync_write.h"
#include <mach/dma.h>
#include <linux/memblock.h>
#include <linux/mm.h>
#include <asm/sizes.h>
#include <mach/mt_dramc.h>
#include <linux/spinlock.h>
#include <mach/mt_gpt.h>
#include <mach/mt_reg_base.h>
#include <asm/sched_clock.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <mach/mt_clkmgr.h>
#include <linux/wakelock.h>
#include <mach/mt_cpufreq.h>
#include <mach/emi_bwl.h>
//#define mt6333_config_interface
static struct dfs_configuration dfs_con;
static unsigned char OTA_Not_used;
#if 1

#ifdef CONFIG_CLKSRC_64_BIT
#define GPT_CLKSRC_ID       (GPT6)
#else
#define GPT_CLKSRC_ID       (GPT2)
#endif

static int dram_clk;
static DEFINE_SPINLOCK(lock);
volatile unsigned char *dst_array_v;
volatile unsigned char *src_array_v;
volatile unsigned int dst_array_p;
volatile unsigned int src_array_p;
volatile int DMA_done;
int channel;
volatile DRAM_INFO dram_info;
volatile unsigned int Voltage_DMA_counter;
struct wake_lock DMA_suspend_lock;
__attribute__ ((__section__ (".sram.func"))) int sram_set_dram(int clk)
{
   DRAMC_WRITE_REG(DRAMC_READ_REG(0x04) | (0x1<<26), 0x04);
   while ( (DRAMC_READ_REG(0x3b8) & (0x01<<16))==0);

    /* set ac timing */
    if(clk == 293) {
        DRAMC_WRITE_REG( 0x447844A4    , 0x0  );
        DRAMC_WRITE_REG( 0xA00642D1     , 0x7C );
        DRAMC_WRITE_REG( 0xBF0B0401     , 0x44 );
        DRAMC_WRITE_REG( 0x03406346     , 0x8  );
        DRAMC_WRITE_REG( 0xD1642742     , 0x1DC);
        DRAMC_WRITE_REG( 0x01000710     , 0x1E8);
        DRAMC_WRITE_REG( 0x000002F5     , 0x1f8 );
    }
    if(clk == 367)
     {
        DRAMC_WRITE_REG( 0x66AB46F7    , 0x0  );
        DRAMC_WRITE_REG( 0xF00487C3     , 0x4 );
        DRAMC_WRITE_REG( 0xBF020401     , 0x44 );
        DRAMC_WRITE_REG( 0x03406358     , 0x8  );
        DRAMC_WRITE_REG( 0xD1644E42     , 0x1DC);
        DRAMC_WRITE_REG( 0x11001640     , 0x1E8);
        DRAMC_WRITE_REG( 0x04002605     , 0x1f8 );
     }

    DRAMC_WRITE_REG(DRAMC_READ_REG(0x04) & ~(0x1<<26), 0x04);
    while ( (DRAMC_READ_REG(0x3b8) & (0x01<<16))==1);
    return 0;
}
static void enable_gpu(void)
{
/*
    enable_clock(MT_CG_MFG_HYD, "MFG");
    enable_clock(MT_CG_MFG_G3D, "MFG");
    enable_clock(MT_CG_MFG_MEM, "MFG");
    enable_clock(MT_CG_MFG_AXI, "MFG");
*/
}
static void disable_gpu(void)
{
/*
    disable_clock(MT_CG_MFG_AXI, "MFG");
    disable_clock(MT_CG_MFG_MEM, "MFG");
    disable_clock(MT_CG_MFG_G3D, "MFG");
    disable_clock(MT_CG_MFG_HYD, "MFG");
*/
}
static ssize_t dram_overclock_show(struct device_driver *driver, char *buf)
{
      int dram_type;
      if(get_ddr_type)
      {
        dram_type=get_ddr_type();
        if(dram_type==LPDDR2)
        {
          dram_type=2;
        }
        else if(dram_type==LPDDR3)
        {
          dram_type=3;
        }
        else if(dram_type==DDR3_16)
        {
          dram_type=4;
        }
        else if(dram_type==DDR3_32)
        {
          dram_type=5;
        }
        else
        {
            dram_type=-1;
            printk("We do not support this DRAM Type\n");
            ASSERT(0);
        }
        return snprintf(buf, PAGE_SIZE, "%d,%d\n",dram_type, mt_fh_get_dramc());
      }
      else
      {
        return  snprintf(buf,PAGE_SIZE,"do not support dram_overclock show\n");
        ASSERT(0);
      }
   return  snprintf(buf,PAGE_SIZE,"dram_overclock failed\n");
}
static ssize_t dram_overclock_store(struct device_driver *driver, const char *buf, size_t count)
{
    int clk, ret = 0;
    clk = simple_strtol(buf, 0, 10);
    dram_clk = mt_fh_get_dramc();
    if(clk == dram_clk) {
        printk(KERN_ERR "dram_clk:%d, is equal to user inpu clk:%d\n", dram_clk, clk);
        return count;
    }
    spin_lock(&lock);
    ret = sram_set_dram(clk);
    if(ret < 0)
        printk(KERN_ERR "dram overclock in sram failed:%d, clk:%d\n", ret, clk);
    spin_unlock(&lock);
    if(get_ddr_type){
      if(get_ddr_type()==LPDDR3)
      {
          mt6333_config_interface(0x6B, Vcore_NV_1333, 0x7F,0);  //1.1
          mt6333_config_interface(0x6C, Vcore_NV_1333, 0x7F,0);
      }
      else
      { 
          mt6333_config_interface(0x6B, Vcore_NV_1066, 0x7F,0);  //1.1
          mt6333_config_interface(0x6C, Vcore_NV_1066, 0x7F,0);
      }
    }
    else
    {
       printk("do not support get_ddr_type\n");
       ASSERT(0);
    }
    ret = mt_fh_dram_overclock(clk);
    if(ret < 0)
        printk(KERN_ERR "dram overclock failed:%d, clk:%d\n", ret, clk);
    printk(KERN_INFO "In %s pass, dram_clk:%d, clk:%d\n", __func__, dram_clk, clk);
    return count;

}
extern unsigned int RESERVED_MEM_SIZE_FOR_TEST_3D;
extern unsigned int FB_SIZE_EXTERN;
extern unsigned int get_max_DRAM_size (void);
static ssize_t ftm_dram_3d_show(struct device_driver *driver, char *buf)
{
    unsigned int pa_3d_base = PHYS_OFFSET + get_max_DRAM_size() - RESERVED_MEM_SIZE_FOR_TEST_3D - FB_SIZE_EXTERN;
    return snprintf(buf, PAGE_SIZE, "%u\n", pa_3d_base);
}
static ssize_t ftm_dram_3d_store(struct device_driver *driver, const char *buf, size_t count)
{
    return count;
}
static ssize_t ftm_dram_mtcmos_show(struct device_driver *driver, char *buf)
{
    return 0;
}
static ssize_t ftm_dram_mtcmos_store(struct device_driver *driver, const char *buf, size_t count)
{
    int enable;
    enable = simple_strtol(buf, 0, 10);
    if(enable == 1) {
        enable_gpu();
        printk(KERN_INFO "enable in %s, enable:%d\n", __func__, enable);
    } else if(enable == 0) {
        disable_gpu();
        printk(KERN_INFO "enable in %s, disable:%d\n", __func__, enable);
    } else
        printk(KERN_ERR "dram overclock failed:%s, enable:%d\n", __func__, enable);
    return count;
}

int Binning_DRAM_complex_mem_test (void)
{
    unsigned char *MEM8_BASE;
    unsigned short *MEM16_BASE;
    unsigned int *MEM32_BASE;
    unsigned int *MEM_BASE;
    unsigned char pattern8;
    unsigned short pattern16;
    unsigned int i, j, size, pattern32;
    unsigned int value;
    unsigned int len=MEM_TEST_SIZE;
    void *ptr;   
    ptr = vmalloc(PAGE_SIZE*2);
    MEM8_BASE=(unsigned char *)ptr;
    MEM16_BASE=(unsigned short *)ptr;
    MEM32_BASE=(unsigned int *)ptr;
    MEM_BASE=(unsigned int *)ptr;
    printk("Test DRAM start address 0x%x\n",(unsigned int)ptr);
    printk("Test DRAM SIZE 0x%x\n",MEM_TEST_SIZE);
    size = len >> 2;

    /* === Verify the tied bits (tied high) === */
    for (i = 0; i < size; i++)
    {
        MEM32_BASE[i] = 0;
    }

    for (i = 0; i < size; i++)
    {
        if (MEM32_BASE[i] != 0)
        {
            vfree(ptr);
            return -1;
        }
        else
        {
            MEM32_BASE[i] = 0xffffffff;
        }
    }

    /* === Verify the tied bits (tied low) === */
    for (i = 0; i < size; i++)
    {
        if (MEM32_BASE[i] != 0xffffffff)
        {
            vfree(ptr);
            return -2;
        }
        else
            MEM32_BASE[i] = 0x00;
    }

    /* === Verify pattern 1 (0x00~0xff) === */
    pattern8 = 0x00;
    for (i = 0; i < len; i++)
        MEM8_BASE[i] = pattern8++;
    pattern8 = 0x00;
    for (i = 0; i < len; i++)
    {
        if (MEM8_BASE[i] != pattern8++)
        { 
            vfree(ptr);
            return -3;
        }
    }

    /* === Verify pattern 2 (0x00~0xff) === */
    pattern8 = 0x00;
    for (i = j = 0; i < len; i += 2, j++)
    {
        if (MEM8_BASE[i] == pattern8)
            MEM16_BASE[j] = pattern8;
        if (MEM16_BASE[j] != pattern8)
        {
            vfree(ptr);
            return -4;
        }
        pattern8 += 2;
    }

    /* === Verify pattern 3 (0x00~0xffff) === */
    pattern16 = 0x00;
    for (i = 0; i < (len >> 1); i++)
        MEM16_BASE[i] = pattern16++;
    pattern16 = 0x00;
    for (i = 0; i < (len >> 1); i++)
    {
        if (MEM16_BASE[i] != pattern16++)                                                                                                    
        {
            vfree(ptr);
            return -5;
        }
    }

    /* === Verify pattern 4 (0x00~0xffffffff) === */
    pattern32 = 0x00;
    for (i = 0; i < (len >> 2); i++)
        MEM32_BASE[i] = pattern32++;
    pattern32 = 0x00;
    for (i = 0; i < (len >> 2); i++)
    {
        if (MEM32_BASE[i] != pattern32++)
        { 
            vfree(ptr);
            return -6;
        }
    }

    /* === Pattern 5: Filling memory range with 0x44332211 === */
    for (i = 0; i < size; i++)
        MEM32_BASE[i] = 0x44332211;

    /* === Read Check then Fill Memory with a5a5a5a5 Pattern === */
    for (i = 0; i < size; i++)
    {
        if (MEM32_BASE[i] != 0x44332211)
        {
            vfree(ptr);
            return -7;
        }
        else
        {
            MEM32_BASE[i] = 0xa5a5a5a5;
        }
    }

    /* === Read Check then Fill Memory with 00 Byte Pattern at offset 0h === */
    for (i = 0; i < size; i++)
    {
        if (MEM32_BASE[i] != 0xa5a5a5a5)
        { 
            vfree(ptr);
            return -8;  
        }
        else                                                                                                                              
        {
            MEM8_BASE[i * 4] = 0x00;
        }
    }

    /* === Read Check then Fill Memory with 00 Byte Pattern at offset 2h === */
    for (i = 0; i < size; i++)
    {
        if (MEM32_BASE[i] != 0xa5a5a500)
        {
            vfree(ptr);
            return -9;
        }
        else
        {
            MEM8_BASE[i * 4 + 2] = 0x00;
        }
    }

    /* === Read Check then Fill Memory with 00 Byte Pattern at offset 1h === */
    for (i = 0; i < size; i++)
    {
        if (MEM32_BASE[i] != 0xa500a500)
        {
            vfree(ptr);
            return -10;
        }
        else
        {
            MEM8_BASE[i * 4 + 1] = 0x00;
        }
    }

    /* === Read Check then Fill Memory with 00 Byte Pattern at offset 3h === */
    for (i = 0; i < size; i++)
    {
        if (MEM32_BASE[i] != 0xa5000000)
        {
            vfree(ptr);
            return -11;
        }
        else
        {
            MEM8_BASE[i * 4 + 3] = 0x00;
        }
    }

    /* === Read Check then Fill Memory with ffff Word Pattern at offset 1h == */
    for (i = 0; i < size; i++)
    {
        if (MEM32_BASE[i] != 0x00000000)
        {
            vfree(ptr);
            return -12;
        }
        else
        {
            MEM16_BASE[i * 2 + 1] = 0xffff;
        }
    }


    /* === Read Check then Fill Memory with ffff Word Pattern at offset 0h == */
    for (i = 0; i < size; i++)
    {
        if (MEM32_BASE[i] != 0xffff0000)
        {
            vfree(ptr);
            return -13;
        }
        else
        {
            MEM16_BASE[i * 2] = 0xffff;
        }
    }
    /*===  Read Check === */
    for (i = 0; i < size; i++)
    {
        if (MEM32_BASE[i] != 0xffffffff)
        {
            vfree(ptr);
            return -14;
        }
    }


    /************************************************
    * Additional verification
    ************************************************/
    /* === stage 1 => write 0 === */

    for (i = 0; i < size; i++)
    {
        MEM_BASE[i] = PATTERN1;
    }


    /* === stage 2 => read 0, write 0xF === */
    for (i = 0; i < size; i++)
    {
        value = MEM_BASE[i];

        if (value != PATTERN1)
        {
            vfree(ptr);
            return -15;
        }
        MEM_BASE[i] = PATTERN2;
    }


    /* === stage 3 => read 0xF, write 0 === */
    for (i = 0; i < size; i++)
    {
        value = MEM_BASE[i];
        if (value != PATTERN2)
        {
            vfree(ptr);
            return -16;
        }
        MEM_BASE[i] = PATTERN1;
    }


    /* === stage 4 => read 0, write 0xF === */
    for (i = 0; i < size; i++)
    {
        value = MEM_BASE[i];
        if (value != PATTERN1)
        {
            vfree(ptr);
            return -17;
        }
        MEM_BASE[i] = PATTERN2;
    }


    /* === stage 5 => read 0xF, write 0 === */
    for (i = 0; i < size; i++)
    {
        value = MEM_BASE[i];
        if (value != PATTERN2)
        { 
            vfree(ptr);
            return -18;
        }
        MEM_BASE[i] = PATTERN1;
    }


    /* === stage 6 => read 0 === */
    for (i = 0; i < size; i++)
    {
        value = MEM_BASE[i];
        if (value != PATTERN1)
        {
            vfree(ptr);
            return -19;
        }
    }


    /* === 1/2/4-byte combination test === */
    i = (unsigned int) MEM_BASE;
    while (i < (unsigned int) MEM_BASE + (size << 2))
    {
        *((unsigned char *) i) = 0x78;
        i += 1;
        *((unsigned char *) i) = 0x56;
        i += 1;
        *((unsigned short *) i) = 0x1234;
        i += 2;
        *((unsigned int *) i) = 0x12345678;
        i += 4;
        *((unsigned short *) i) = 0x5678;
        i += 2;
        *((unsigned char *) i) = 0x34;
        i += 1;
        *((unsigned char *) i) = 0x12;
        i += 1;
        *((unsigned int *) i) = 0x12345678;
        i += 4;
        *((unsigned char *) i) = 0x78;
        i += 1;
        *((unsigned char *) i) = 0x56;
        i += 1;
        *((unsigned short *) i) = 0x1234;
        i += 2;
        *((unsigned int *) i) = 0x12345678;
        i += 4;
        *((unsigned short *) i) = 0x5678;
        i += 2;
        *((unsigned char *) i) = 0x34;
        i += 1;
        *((unsigned char *) i) = 0x12;
        i += 1;
        *((unsigned int *) i) = 0x12345678;
        i += 4;
    }
    for (i = 0; i < size; i++)
    {
        value = MEM_BASE[i];
        if (value != 0x12345678)
        {
            vfree(ptr);
            return -20;
        }
    }


    /* === Verify pattern 1 (0x00~0xff) === */
    pattern8 = 0x00;
    MEM8_BASE[0] = pattern8;
    for (i = 0; i < size * 4; i++)
    {
        unsigned char waddr8, raddr8;
        waddr8 = i + 1;
        raddr8 = i;
        if (i < size * 4 - 1)
            MEM8_BASE[waddr8] = pattern8 + 1;
        if (MEM8_BASE[raddr8] != pattern8)
        {
            vfree(ptr);
            return -21;
        }
        pattern8++;
    }


    /* === Verify pattern 2 (0x00~0xffff) === */
    pattern16 = 0x00;
    MEM16_BASE[0] = pattern16;
    for (i = 0; i < size * 2; i++)
    {
        if (i < size * 2 - 1)
            MEM16_BASE[i + 1] = pattern16 + 1;
        if (MEM16_BASE[i] != pattern16)
        {
            vfree(ptr);
            return -22;
        }
        pattern16++;
    }
    /* === Verify pattern 3 (0x00~0xffffffff) === */
    pattern32 = 0x00;
    MEM32_BASE[0] = pattern32;
    for (i = 0; i < size; i++)
    {
        if (i < size - 1)
            MEM32_BASE[i + 1] = pattern32 + 1;
        if (MEM32_BASE[i] != pattern32)
        {
            vfree(ptr);
            return -23;
        }
        pattern32++;
    }
    printk("complex R/W mem test pass\n");
    vfree(ptr);
    return 1;
}

static ssize_t complex_mem_test_show(struct device_driver *driver, char *buf)
{
    int ret;
    ret=Binning_DRAM_complex_mem_test();
    if(ret>0)
    {
      return snprintf(buf, PAGE_SIZE, "MEM Test all pass\n");
    }
    else
    {
      return snprintf(buf, PAGE_SIZE, "MEM TEST failed %d \n", ret);
    }
}
static ssize_t complex_mem_test__store(struct device_driver *driver, const char *buf, size_t count)
{
    return count;
}
static ssize_t DFS_test_show(struct device_driver *driver, char *buf)
{
    int ret;
    int rank0_fine;
    int rank1_fine;
    if(dfs_con.Freq_Status==HIGH_FREQUENCY)
    {
      
        ret=mt_fh_dram_overclock(dfs_con.ddr_LF);
        if(ret<0)
        {
        return snprintf(buf, PAGE_SIZE, "hopping failed\n");
        }
        dfs_con.Freq_Status=LOW_FREQUENCY;        
        rank0_fine=DRAMC_READ_REG(0x374);
        rank1_fine=DRAMC_READ_REG(0x378);
       if(dfs_con.V_Status==HV)
       {
        DFS_phase_mt6333_config_interface(dfs_con.vcore_LF_HV);
        mt6333_config_interface(0x81, dfs_con.vm_LF_HV, 0x7F,0);
        mt6333_config_interface(0x82, dfs_con.vm_LF_HV, 0x7F,0);  //1.325

       }
       else if (dfs_con.V_Status==LV)
       {
        DFS_phase_mt6333_config_interface(dfs_con.vcore_LF_LV);
        mt6333_config_interface(0x81, dfs_con.vm_LF_LV, 0x7F,0); //1.14
        mt6333_config_interface(0x82, dfs_con.vm_LF_LV, 0x7F,0);

       }
       else if (dfs_con.V_Status==HVc_LVm)  //HVc_LVm
       {
        DFS_phase_mt6333_config_interface(dfs_con.vcore_LF_HV);
        mt6333_config_interface(0x81, dfs_con.vm_LF_LV, 0x7F,0); //1.16
        mt6333_config_interface(0x82, dfs_con.vm_LF_LV, 0x7F,0);
       }
       else if (dfs_con.V_Status==LVc_HVm) //LVc_HVm
       {
        DFS_phase_mt6333_config_interface(dfs_con.vcore_LF_LV);
        mt6333_config_interface(0x81, dfs_con.vm_LF_HV, 0x7F,0); //1.3
        mt6333_config_interface(0x82, dfs_con.vm_LF_HV, 0x7F,0);
       }
       else if (dfs_con.V_Status==HVc_NVm)
       {
        DFS_phase_mt6333_config_interface(dfs_con.vcore_LF_HV);
        mt6333_config_interface(0x81, dfs_con.vm_LF_NV, 0x7F,0); //1.22
        mt6333_config_interface(0x82, dfs_con.vm_LF_NV, 0x7F,0);
       }
       else
       {
        DFS_phase_mt6333_config_interface(dfs_con.vcore_LF_NV);
        mt6333_config_interface(0x81, dfs_con.vm_LF_NV, 0x7F,0); //1.22
        mt6333_config_interface(0x82, dfs_con.vm_LF_NV, 0x7F,0);
       }
        return snprintf(buf, PAGE_SIZE, "hopping to %d,\r\nrank0_fine=0x%x,rank1_fine=0x%x,\r\nDFS_DMA_Times=%d\nDFS_Voltage_DMA_counter %d\r\n", \
        dfs_con.ddr_LF*4,rank0_fine,rank1_fine,DMA_TIMES_RECORDER,Voltage_DMA_counter);
    }
    else
    { 

       if(dfs_con.V_Status==HV)
       {
        DFS_phase_mt6333_config_interface(dfs_con.vcore_HF_HV);
        mt6333_config_interface(0x81, dfs_con.vm_HF_HV, 0x7F,0); 
        mt6333_config_interface(0x82, dfs_con.vm_HF_HV, 0x7F,0);  //1.325
       }
       else if (dfs_con.V_Status==LV)
       {
        DFS_phase_mt6333_config_interface(dfs_con.vcore_HF_LV);
        mt6333_config_interface(0x6B, dfs_con.vcore_HF_LV, 0x7F,0);  //1.0
        mt6333_config_interface(0x6C, dfs_con.vcore_HF_LV, 0x7F,0);
        mt6333_config_interface(0x81, dfs_con.vm_HF_LV, 0x7F,0); //1.16
        mt6333_config_interface(0x82, dfs_con.vm_HF_LV, 0x7F,0);
       }

       else if (dfs_con.V_Status==HVc_LVm)  //HVc_LVm
       {
        DFS_phase_mt6333_config_interface(dfs_con.vcore_HF_HV);
        mt6333_config_interface(0x81, dfs_con.vm_HF_LV, 0x7F,0); //1.16
        mt6333_config_interface(0x82, dfs_con.vm_HF_LV, 0x7F,0);
       }
       else if (dfs_con.V_Status==LVc_HVm) //LVc_HVm
       {
        DFS_phase_mt6333_config_interface(dfs_con.vcore_HF_LV);
        mt6333_config_interface(0x81, dfs_con.vm_HF_HV, 0x7F,0); //1.3
        mt6333_config_interface(0x82, dfs_con.vm_HF_HV, 0x7F,0);
       }
       else if (dfs_con.V_Status==HVc_NVm)
       {
        DFS_phase_mt6333_config_interface(dfs_con.vcore_HF_HV);
        mt6333_config_interface(0x81, dfs_con.vm_HF_NV, 0x7F,0); //1.3
        mt6333_config_interface(0x82, dfs_con.vm_HF_NV, 0x7F,0);
       }
       else
       {
        DFS_phase_mt6333_config_interface(dfs_con.vcore_HF_NV);
        mt6333_config_interface(0x81, dfs_con.vm_HF_NV, 0x7F,0); //1.22
        mt6333_config_interface(0x82, dfs_con.vm_HF_NV, 0x7F,0);
       }
        ret=mt_fh_dram_overclock(dfs_con.ddr_HF);
        if(ret<0)
        {
        return snprintf(buf, PAGE_SIZE, "hopping failed\n");
        }
        dfs_con.Freq_Status=HIGH_FREQUENCY;
        rank0_fine=DRAMC_READ_REG(0x374);
        rank1_fine=DRAMC_READ_REG(0x378);
        return snprintf(buf, PAGE_SIZE, "hopping to %d,\r\nrank0_fine=0x%x,nrank1_fine=0x%x\r\nDFS_DMA_Times %d\nDFS_Voltage_DMA_counter %d\r\n", \
        dfs_con.ddr_HF*4,rank0_fine,rank1_fine,DMA_TIMES_RECORDER,Voltage_DMA_counter);
    }
}


/*
   0:LV
   1:NV
   2:HV
   3:HVc_LVm
   4:LVc_HVm
   5:HVc_NVm
*/
static ssize_t DFS_test_store(struct device_driver *driver, const char *buf, size_t count)
{
    int status;
    status = simple_strtol(buf, 0, 10);

    if (status==HVc_NVm)
    {
      dfs_con.V_Status=HVc_NVm;
    }
    else if (status==LVc_HVm)
    {
      dfs_con.V_Status=LVc_HVm;
    }
    else if(status==HVc_LVm)
    {
      dfs_con.V_Status=HVc_LVm;
    }
    else if(status==HV)
    {
      dfs_con.V_Status=HV;
    }
    else if(status==LV)
    {
      dfs_con.V_Status=LV;
    }
    else
    {
      dfs_con.V_Status=NV;
    }
    printk("dfs_con.V_Status 0x%x\n",dfs_con.V_Status);
    return count;
}
static ssize_t DFS_status_show(struct device_driver *driver, char *buf)
{
    int ret;
    int ret_val;
    unsigned int coarse_v_e0;
    unsigned int coarse_v_124;
    unsigned int rank0_coarse;
    unsigned int rank1_coarse;
    unsigned char OldVcore1 = 0;
    unsigned char OldVmem1 = 0;

    ret=DFS_Detection();
    coarse_v_e0=DRAMC_READ_REG(0xe0);
    coarse_v_124=DRAMC_READ_REG(0x124);
    rank0_coarse= ((coarse_v_e0 & 0x7<<24)>>22)|(coarse_v_124 & (0x3));
    rank1_coarse= ((coarse_v_e0 & 0x7<<24)>>22)|((coarse_v_124 & (0x3)<<2)>>2);
    ret_val=mt6333_read_interface(0x6B,&OldVcore1,0x7F,0);
    ret_val=mt6333_read_interface(0x81,&OldVmem1, 0x7F,0);
    get_DRAM_default_voltage();
    get_DRAM_default_freq();
   if(ret==3)
    {
      return snprintf(buf, PAGE_SIZE, "Version:%d,\nDFS:Enable(1066<->1466),\nR0_Coarse:0x%x,\nR1_Coarse=0x%x,\n[Vcore]0x6B=0x%x,\r\n[Vmem] 0x81=0x%x\r\nPHASE 0x%x\r\nRank num:0x%x\r\n", \
      Coding_Version,rank0_coarse,rank1_coarse,OldVcore1,OldVmem1,PHASE_NUMBER,get_rank_num());
    }
   else if(ret==2)
    {
      return snprintf(buf, PAGE_SIZE, "Version:%d,\nDFS:Enable(1200<->1466),\nR0_Coarse:0x%x,\nR1_Coarse=0x%x,\n[Vcore]0x6B=0x%x,\r\n[Vmem] 0x81=0x%x\r\nPHASE 0x%x\r\nRank num:0x%x\r\n", \
      Coding_Version,rank0_coarse,rank1_coarse,OldVcore1,OldVmem1,PHASE_NUMBER,get_rank_num());
    }
   else if(ret==1)
   {
     return snprintf(buf, PAGE_SIZE, "Version:%d,\nDFS:Enable(1066<->1333),\nR0_Coarse:0x%x,\nR1_Coarse=0x%x,\n[Vcore]0x6B=0x%x,\r\n[Vmem] 0x81=0x%x\r\nPHASE 0x%x\r\nRank num:0x%x\r\n", \
     Coding_Version,rank0_coarse,rank1_coarse,OldVcore1,OldVmem1,PHASE_NUMBER,get_rank_num());
   }
    else
    {
     return snprintf(buf, PAGE_SIZE, "Version:%d,\nDFS:DFS Disable,\nR0_Coarse:0x%x,\nR1_Coarse=0x%x,\n[Vcore]0x6B=0x%x,\r\n[Vmem] 0x81=0x%x\r\nPHASE 0x%x\r\nRank num:0x%x\r\n",  \
     Coding_Version,rank0_coarse,rank1_coarse,OldVcore1,OldVmem1,PHASE_NUMBER,get_rank_num());
    }

}
static ssize_t DFS_status_store(struct device_driver *driver, const char *buf, size_t count)
{
    return count;
}
#ifdef APDMA_TEST
static ssize_t DFS_APDMA_TEST_show(struct device_driver *driver, char *buf)
{
    int ret;
    printk("[Before  enable_clock]addr:0x%x, value:%x\n",(0xF0003018),*((volatile int *)0xF0003018));
    enable_clock(MT_CG_PERI_AP_DMA,"APDMA_MODULE");
    printk("[after  enable_clock]addr:0x%x, value:%x\n",(0xF0003018),*((volatile int *)0xF0003018));
    DFS_APDMA_early_init();
    ret=DFS_APDMA_Init();
    ret=DFS_APDMA_Enable();
    disable_clock(MT_CG_PERI_AP_DMA,"APDMA_MODULE");

    return snprintf(buf, PAGE_SIZE, "DFS APDMA Dummy Read Address &0x%x=0x%x \n",src_array_v,*((unsigned int *)src_array_v));
}
static ssize_t DFS_APDMA_TEST_store(struct device_driver *driver, const char *buf, size_t count)
{
    return count;
}
#endif





#ifdef Voltage_Debug
static ssize_t DFS_Voltage_show(struct device_driver *driver, char *buf)
{
    int ret_val = 0;
    unsigned char OldVcore1 = 0;
    unsigned char OldVcore2 = 0;
    unsigned char OldVmem1 = 0;
    unsigned char OldVmem2 = 0;

    printk("[PMIC]pmic_voltage_read : \r\n");
    ret_val=mt6333_read_interface(0x6B,&OldVcore1,0x7F,0);
    ret_val=mt6333_read_interface(0x81,&OldVmem1, 0x7F,0);
    printk("[Vcore]0x6B=0x%x,\r\n[Vmem] 0x81=0x%x \r\n", OldVcore1,OldVmem1);
    return snprintf(buf, PAGE_SIZE,"[Vcore]0x6B=0x%x,\r\n[Vmem] 0x81=0x%x,\r\nDMA times %d \r\n", OldVcore1,OldVmem1,Voltage_DMA_counter);
}
static ssize_t DFS_Voltage_store(struct device_driver *driver, const char *buf, size_t count)
{
    int status;
    int vcore;
    int vmem;
    int voltage;
    voltage = simple_strtol(buf, 0, 16);
    vcore= voltage >> 8;
    vmem = voltage & 0xFF;
    DFS_phase_mt6333_config_interface(vcore);
//    mt6333_config_interface(0x6B, vcore, 0x7F,0);
//    mt6333_config_interface(0x6C, vcore, 0x7F,0);
    mt6333_config_interface(0x81,vmem, 0x7F,0);
    mt6333_config_interface(0x82,vmem, 0x7F,0);
    return count;
}
#endif

static ssize_t DFS_HQA_Config_show(struct device_driver *driver, char *buf)
{
  return  snprintf(buf, PAGE_SIZE,"[HQA INFO] Freq_Status=0x%x\
                                   \n[HQA INFO] Vol_Status=0x%x \
                                   \n[HQA INFO] DRAM HF Freq=%d \
                                   \n[HQA INFO] DRAM HF HVc=0x%x \
                                   \n[HQA INFO] DRAM HF NVc=0x%x \
                                   \n[HQA INFO] DRAM HF LVc=0x%x \
                                   \n[HQA INFO] DRAM HF HVm=0x%x \
                                   \n[HQA INFO] DRAM HF NVm=0x%x \
                                   \n[HQA INFO] DRAM HF LVm=0x%x \                                    
                                   \n[HQA INFO] DRAM LF Freq=%d \
                                   \n[HQA INFO] DRAM LF HVc=0x%x \
                                   \n[HQA INFO] DRAM LF NVc=0x%x \
                                   \n[HQA INFO] DRAM LF LVc=0x%x \
                                   \n[HQA INFO] DRAM LF HVm=0x%x \
                                   \n[HQA INFO] DRAM LF NVm=0x%x \
                                   \n[HQA INFO] DRAM LF LVm=0x%x\n" \
                                   ,dfs_con.Freq_Status          \
                                   ,dfs_con.V_Status          \
                                   ,dfs_con.ddr_HF          \
                                   ,dfs_con.vcore_HF_HV          \
                                   ,dfs_con.vcore_HF_NV          \
                                   ,dfs_con.vcore_HF_LV          \
                                   ,dfs_con.vm_HF_HV            \
                                   ,dfs_con.vm_HF_NV            \
                                   ,dfs_con.vm_HF_LV            \
                                   ,dfs_con.ddr_LF          \
                                   ,dfs_con.vcore_LF_HV          \
                                   ,dfs_con.vcore_LF_NV          \
                                   ,dfs_con.vcore_LF_LV          \
                                   ,dfs_con.vm_LF_HV            \
                                   ,dfs_con.vm_LF_NV            \
                                   ,dfs_con.vm_LF_LV);

}
/*
  1:1466 HQA Configuration
  0:1333 HQA Configuration
*/
static ssize_t DFS_HQA_Config_store(struct device_driver *driver, const char *buf, size_t count)
{
    int sel;
    sel = simple_strtol(buf, 0, 16);
  if(sel==0x1){
    printk("Configure 1466 HQA Setting\n ");
    dfs_con.vcore_HF_HV=Vcore_HV_1466;
    dfs_con.vcore_HF_NV=Vcore_NV_1466;
    dfs_con.vcore_HF_LV=Vcore_LV_1466;
    dfs_con.vm_HF_HV=Vmem_HV_1466;
    dfs_con.vm_HF_NV=Vmem_NV_1466;
    dfs_con.vm_HF_LV=Vmem_LV_1466;
    dfs_con.vcore_LF_HV=Vcore_HV_1066;
    dfs_con.vcore_LF_NV=Vcore_NV_1066;
    dfs_con.vcore_LF_LV=Vcore_LV_1066;
    dfs_con.vm_LF_HV=Vmem_HV_1066;
    dfs_con.vm_LF_NV=Vmem_NV_1066;
    dfs_con.vm_LF_LV=Vmem_LV_1066;
    dfs_con.ddr_HF=DDR_1466;
    dfs_con.ddr_LF=DDR_1066;
    dfs_con.Freq_Status=HIGH_FREQUENCY;
    dfs_con.V_Status=NV;
  }
  else{
    printk("Configure 1333 HQA Setting\n");
    dfs_con.vcore_HF_HV=Vcore_HV_1333;
    dfs_con.vcore_HF_NV=Vcore_NV_1333;
    dfs_con.vcore_HF_LV=Vcore_LV_1333;
    dfs_con.vm_HF_HV=Vmem_HV_1333;
    dfs_con.vm_HF_NV=Vmem_NV_1333;
    dfs_con.vm_HF_LV=Vmem_LV_1333;
    dfs_con.vcore_LF_HV=Vcore_HV_1066;
    dfs_con.vcore_LF_NV=Vcore_NV_1066;
    dfs_con.vcore_LF_LV=Vcore_LV_1066;
    dfs_con.vm_LF_HV=Vmem_HV_1066;
    dfs_con.vm_LF_NV=Vmem_NV_1066;
    dfs_con.vm_LF_LV=Vmem_LV_1066;
    dfs_con.ddr_HF=DDR_1333;
    dfs_con.ddr_LF=DDR_1066;
    dfs_con.Freq_Status=HIGH_FREQUENCY;
    dfs_con.V_Status=NV;
  }
    return count;
}


DRIVER_ATTR(emi_clk_test, 0664, dram_overclock_show, dram_overclock_store);
DRIVER_ATTR(emi_clk_3d_test, 0664, ftm_dram_3d_show, ftm_dram_3d_store);
DRIVER_ATTR(emi_clk_mtcmos_test, 0664, ftm_dram_mtcmos_show, ftm_dram_mtcmos_store);
DRIVER_ATTR(emi_clk_mem_test, 0664, complex_mem_test_show, complex_mem_test__store);
DRIVER_ATTR(emi_DFS_test, 0664, DFS_test_show, DFS_test_store);
DRIVER_ATTR(emi_DFS_status, 0664, DFS_status_show, DFS_status_store);
#ifdef APDMA_TEST
DRIVER_ATTR(emi_DFS_APDMA_test, 0664, DFS_APDMA_TEST_show, DFS_APDMA_TEST_store);
#endif
#ifdef Voltage_Debug
DRIVER_ATTR(emi_DRAM_Voltage, 0664, DFS_Voltage_show,DFS_Voltage_store);
#endif
DRIVER_ATTR(emi_DFS_HQA_Config, 0664, DFS_HQA_Config_show,DFS_HQA_Config_store);
static struct device_driver dram_overclock_drv =
{
    .name = "emi_clk_test",
    .bus = &platform_bus_type,
    .owner = THIS_MODULE,
};

extern char __ssram_text, _sram_start, __esram_text;
int __init dram_overclock_init(void)
{
    int ret;
    unsigned char * dst = &__ssram_text;
    unsigned char * src =  &_sram_start;
    
    ret = driver_register(&dram_overclock_drv);
    if (ret) {
        printk(KERN_ERR "fail to create the dram_overclock driver\n");
        return ret;
    }
    ret = driver_create_file(&dram_overclock_drv, &driver_attr_emi_clk_test);
    if (ret) {
        printk(KERN_ERR "fail to create the dram_overclock sysfs files\n");
        return ret;
    }
    ret = driver_create_file(&dram_overclock_drv, &driver_attr_emi_clk_3d_test);
    if (ret) {
        printk(KERN_ERR "fail to create the ftm_dram_3d_drv sysfs files\n");
        return ret;
    }
    ret = driver_create_file(&dram_overclock_drv, &driver_attr_emi_clk_mtcmos_test);
    if (ret) {
        printk(KERN_ERR "fail to create the ftm_dram_mtcmos_drv sysfs files\n");
        return ret;
    }
    ret = driver_create_file(&dram_overclock_drv, &driver_attr_emi_clk_mem_test);
    if (ret) {
        printk(KERN_ERR "fail to create the ftm_dram_mtcmos_drv sysfs files\n");
        return ret;
    }
    ret = driver_create_file(&dram_overclock_drv, &driver_attr_emi_DFS_test);
    if (ret) {
        printk(KERN_ERR "fail to create the DFS sysfs files\n");
        return ret;
    }
    ret = driver_create_file(&dram_overclock_drv, &driver_attr_emi_DFS_status);
    if (ret) {
        printk(KERN_ERR "fail to create the DFS sysfs files\n");
        return ret;
    }
#ifdef APDMA_TEST
    ret = driver_create_file(&dram_overclock_drv, &driver_attr_emi_DFS_APDMA_test);
    if (ret) {
        printk(KERN_ERR "fail to create the DFS sysfs files\n");
        return ret;
    }
#endif
#ifdef Voltage_Debug
    ret = driver_create_file(&dram_overclock_drv, &driver_attr_emi_DRAM_Voltage);
    if (ret) {
        printk(KERN_ERR "fail to create the DFS sysfs files\n");
        return ret;
    }
#endif
    ret = driver_create_file(&dram_overclock_drv, &driver_attr_emi_DFS_HQA_Config);
    if (ret) {
        printk(KERN_ERR "fail to create the DFS sysfs files\n");
        return ret;
    }

    for (dst = &__ssram_text ; dst < (unsigned char *)&__esram_text ; dst++,src++) {
        *dst = *src;
    }
    wake_lock_init(&DMA_suspend_lock, WAKE_LOCK_SUSPEND, "3PHASE_DMA wakelock");
    DFS_Voltage_Configuration();
    return 0;
}

#endif

void DFS_Voltage_Configuration(void)
{
  int dfs_status;
  dfs_status=DFS_Detection();
  printk("HQA dfs_status 0x%x\n",dfs_status);
  if(dfs_status==3){
    printk("HQA Do 1466 configure");
    dfs_con.vcore_HF_HV=Vcore_HV_1466;
    dfs_con.vcore_HF_NV=Vcore_NV_1466;
    dfs_con.vcore_HF_LV=Vcore_LV_1466;
    dfs_con.vm_HF_HV=Vmem_HV_1466;
    dfs_con.vm_HF_NV=Vmem_NV_1466;
    dfs_con.vm_HF_LV=Vmem_LV_1466;
    dfs_con.vcore_LF_HV=Vcore_HV_1066;
    dfs_con.vcore_LF_NV=Vcore_NV_1066;
    dfs_con.vcore_LF_LV=Vcore_LV_1066;
    dfs_con.vm_LF_HV=Vmem_HV_1066;
    dfs_con.vm_LF_NV=Vmem_NV_1066;
    dfs_con.vm_LF_LV=Vmem_LV_1066;
    dfs_con.ddr_HF=DDR_1466;
    dfs_con.ddr_LF=DDR_1066;
    dfs_con.Freq_Status=HIGH_FREQUENCY;
    dfs_con.V_Status=NV;
  }
  #if 0
  else if (dfs_status==2){
    dfs_con.vcore_HF_HV=Vcore_HV_1466;
    dfs_con.vcore_HF_NV=Vcore_NV_1466;
    dfs_con.vcore_HF_LV=Vcore_LV_1466;
    dfs_con.vm_HF_HV=Vmem_HV_1466;
    dfs_con.vm_HF_NV=Vmem_NV_1466;
    dfs_con.vm_HF_LV=Vmem_LV_1466;
    dfs_con.vcore_LF_HV=Vcore_HV_1200;
    dfs_con.vcore_LF_NV=Vcore_NV_1200;
    dfs_con.vcore_LF_LV=Vcore_LV_1200;
    dfs_con.vm_LF_HV=Vmem_HV_1200;
    dfs_con.vm_LF_NV=Vmem_NV_1200;
    dfs_con.vm_LF_LV=Vmem_LV_1200;
    dfs_con.ddr_HF=DDR_1466;
    dfs_con.ddr_LF=DDR_1200;
    dfs_con.Freq_Status=HIGH_FREQUENCY;
    dfs_con.V_Status=NV;

  }
  #endif
  else if(dfs_status==1){
    printk("HQA Do 1333 configure");
    dfs_con.vcore_HF_HV=Vcore_HV_1333;
    dfs_con.vcore_HF_NV=Vcore_NV_1333;
    dfs_con.vcore_HF_LV=Vcore_LV_1333;
    dfs_con.vm_HF_HV=Vmem_HV_1333;
    dfs_con.vm_HF_NV=Vmem_NV_1333;
    dfs_con.vm_HF_LV=Vmem_LV_1333;
    dfs_con.vcore_LF_HV=Vcore_HV_1066;
    dfs_con.vcore_LF_NV=Vcore_NV_1066;
    dfs_con.vcore_LF_LV=Vcore_LV_1066;
    dfs_con.vm_LF_HV=Vmem_HV_1066;
    dfs_con.vm_LF_NV=Vmem_NV_1066;
    dfs_con.vm_LF_LV=Vmem_LV_1066;
    dfs_con.ddr_HF=DDR_1333;
    dfs_con.ddr_LF=DDR_1066;
    dfs_con.Freq_Status=HIGH_FREQUENCY;
    dfs_con.V_Status=NV;
  }
  else{
    dfs_con.vcore_HF_HV=Vcore_HV_1066;
    dfs_con.vcore_HF_NV=Vcore_NV_1066;
    dfs_con.vcore_HF_LV=Vcore_LV_1066;
    dfs_con.vm_HF_HV=Vmem_HV_1066;
    dfs_con.vm_HF_NV=Vmem_NV_1066;
    dfs_con.vm_HF_LV=Vmem_LV_1066;
    dfs_con.vcore_LF_HV=Vcore_HV_1066;
    dfs_con.vcore_LF_NV=Vcore_NV_1066;
    dfs_con.vcore_LF_LV=Vcore_LV_1066;
    dfs_con.vm_LF_HV=Vmem_HV_1066;
    dfs_con.vm_LF_NV=Vmem_NV_1066;
    dfs_con.vm_LF_LV=Vmem_LV_1066;
    dfs_con.ddr_HF=DDR_1066;
    dfs_con.ddr_LF=DDR_1066;
    dfs_con.Freq_Status=HIGH_FREQUENCY;
    dfs_con.V_Status=NV;

  }

}



void print_DEBUG_DMA(void)
{
   int i;
   printk("DEBUG FOR 3 PHASE VOLTAGE CHANGE\n");
   printk("Channel 0x%x\n",channel);
    for (i=0;i<16;i++)
   {
     printk("[APDMA]addr:0x%x, value:%x\n",(0xF1000080+i*4),*((volatile int *)(0xF1000080+i*4)));
   }
     printk("[APDMA]addr:0x%x, value:%x\n",(0xF10000D0),*((volatile int *)(0xF10000D0)));
     printk("[APDMA] addr:0x%x, value:%x\n",(0xF0001050),*((volatile int *)(0xF0001050)));
     printk("[APDMA] addr:0x%x, value:%x\n",(0xF0000040),*((volatile int *)(0xF0000040)));
     printk("[APDMA] addr:0x%x, value:%x\n",(0xF000305c),*((volatile int *)(0xF000305c)));
    for(i=0;i<32;i++)
   {
    printk("[APDMA]addr:0x%x, value:%x\n",(0xF1000000+i*4),*((volatile int *)(0xF1000000+i*4)));
   }  

   printk("[APDMA] APDMA clock is on ? %d\n",clock_is_on(MT_CG_PERI_AP_DMA));
}


int DFS_APDMA_early_init(void)
{
   #ifdef APDMA_TEST
   src_array_p=DRAM_BASE+get_max_DRAM_size()/2-IOREMAP_ALIGMENT;
   dst_array_p=DRAM_BASE+get_max_DRAM_size()/2;
   src_array_v = ioremap(src_array_p,0x1000)+IOREMAP_ALIGMENT-BUFF_LEN/2;
   dst_array_v = ioremap(dst_array_p,0x1000)+BUFF_LEN/2;
   #else
   src_array_p=DRAM_BASE+get_max_DRAM_size()/2-BUFF_LEN/2;
   dst_array_p=DRAM_BASE+get_max_DRAM_size()/2+BUFF_LEN/2;
   src_array_v = ioremap(rounddown(src_array_p,IOREMAP_ALIGMENT),IOREMAP_ALIGMENT*2)+IOREMAP_ALIGMENT-BUFF_LEN/2;
   dst_array_v = src_array_v+BUFF_LEN;
   memset(src_array_v,0x6a6a6a6a,BUFF_LEN);
   #endif
   channel = DFS_APDMA_CHANNEL;
   enable_clock(MT_CG_PERI_AP_DMA,"APDMA_MODULE");
   mt_reset_gdma_conf(channel);
   disable_clock(MT_CG_PERI_AP_DMA,"APDMA_MODULE");
   return 1;
}


int DFS_APDMA_Init(void)
{
    writel(((~DMA_GSEC_EN_BIT)&readl(DMA_GLOBAL_GSEC_EN)), DMA_GLOBAL_GSEC_EN);
    return 1;
}
int DFS_APDMA_Enable(void)
{

    while(readl(DMA_START(DMA_BASE_CH(channel)))& 0x1);
    writel(src_array_p, DMA_SRC(DMA_BASE_CH(channel)));
    writel(dst_array_p, DMA_DST(DMA_BASE_CH(channel)));
    writel(BUFF_LEN , DMA_LEN1(DMA_BASE_CH(channel)));
    writel(DMA_CON_BURST_8BEAT, DMA_CON(DMA_BASE_CH(channel)));    
#ifdef APDMAREG_DUMP
   int i;
   printk("Channel 0x%x\n",channel);
    for (i=0;i<16;i++)
   {
     printk("[Before]addr:0x%x, value:%x\n",(0xF1000080+i*4),*((volatile int *)(0xF1000080+i*4)));
   }
     printk("[Before]addr:0x%x, value:%x\n",(0xF10000D0),*((volatile int *)(0xF10000D0)));
     printk("[Infra] addr:0x%x, value:%x\n",(0xF0001050),*((volatile int *)(0xF0001050)));
     printk("[Infra] addr:0x%x, value:%x\n",(0xF0000040),*((volatile int *)(0xF0000040)));
     printk("[Infra] addr:0x%x, value:%x\n",(0xF000305c),*((volatile int *)(0xF000305c)));
    for(i=0;i<32;i++)
   {
    printk("[Before]addr:0x%x, value:%x\n",(0xF1000000+i*4),*((volatile int *)(0xF1000000+i*4)));
   }                          
     
#ifdef APDMA_TEST
    for(i = 0; i < BUFF_LEN/sizeof(unsigned int); i++) {
                dst_array_v[i] = 0;
                src_array_v[i] = i;
        }

#endif
#endif

 mt_reg_sync_writel(0x1,DMA_START(DMA_BASE_CH(channel)));

#ifdef APDMAREG_DUMP
    for (i=0;i<15;i++)
   {
     printk("[AFTER]addr:0x%x, value:%x\n",(0xF1000080+i*4),*((volatile int *)(0xF1000080+i*4)));
   }
   
     printk("[AFTER]addr:0x%x, value:%x\n",(0xF10000D0),*((volatile int *)(0xF10000D0)));
    for(i=0;i<31;i++)
   {
    printk("[AFTER]addr:0x%x, value:%x\n",(0xF1000000+i*4),*((volatile int *)(0xF1000000+i*4)));
   }

#ifdef APDMA_TEST
        for(i = 0; i < BUFF_LEN/sizeof(unsigned int); i++){
                if(dst_array_v[i] != src_array_v[i]){
                        printk("DMA ERROR at Address %x\n", (unsigned int)&dst_array_v[i]);
                        ASSERT(0);
                }
        }
        printk("Channe0 DFS DMA TEST PASS\n");
#endif
#endif
        return 1;     
}

int DFS_APDMA_Init_2(void)
{
#ifdef TEMP_SENSOR
   unsigned val1;
    val1 = (DRAMC_READ_REG(0x1E8) & (0xff00ffff));      // Clear REFRCNT
    DRAMC_WRITE_REG(val1,0x1E8);
    dsb();
#endif

   return 1;
}


int DFS_APDMA_END(void)
{
#ifdef TEMP_SENSOR
   unsigned val1;
#endif

     while(readl(DMA_START(DMA_BASE_CH(channel))));

#ifdef TEMP_SENSOR
    val1 = (DRAMC_READ_REG(0x1E8) | (0x00010000));      // Enable temp sensor.
    DRAMC_WRITE_REG(val1,0x1E8);
    dsb();
#endif

  return 1 ;
}
int DFS_get_dram_data(int dfs_enable, int dram_dpeed,int dram_type,int dramc_vcore)
{
  dram_info.dram_dfs_enable=dfs_enable;
  dram_info.dram_speed=dram_dpeed;
  dram_info.dram_type=dram_type;
  dram_info.dram_vcore=dramc_vcore;
  printk("DRAM_INFO: DFS_ENABLE = %x, SPEED=%x, Type=%x, vcore=%x\n",dram_info.dram_dfs_enable,
    dram_info.dram_speed,dram_info.dram_type,dram_info.dram_vcore);
  OTA_Not_used=0x1; 
 return 1;
}
int DFS_APDMA_free(void)
{
  mt_free_gdma(channel);
  return 1 ;
}

/*
 * XXX: Reserved memory in low memory must be 1MB aligned.
 *     This is because the Linux kernel always use 1MB section to map low memory.
 *
 *    We Reserved the memory regien which could cross rank for APDMA to do dummy read.
 *    
 */

void DFS_Reserved_Memory(int DFS_status)
{
  unsigned long high_memory_phys;
  high_memory_phys=virt_to_phys(high_memory);
  unsigned long DFS_dummy_read_center_address;
  DFS_dummy_read_center_address=DRAM_BASE+get_max_DRAM_size()/2;
  /*For DFS Purpose, we remove this memory block for Dummy read/write by APDMA.*/
  printk("[DFS Check]DRAM SIZE:0x%x\n",get_max_DRAM_size());
  printk("[DFS Check]DRAM Dummy read  from:0x%x to 0x%x\n",(DFS_dummy_read_center_address-BUFF_LEN/2),(DFS_dummy_read_center_address+BUFF_LEN/2));
  printk("[DFS Check]DRAM Dummy read center address:0x%x\n",DFS_dummy_read_center_address);
  printk("[DFS Check]High Memory start address 0x%x\n",high_memory_phys);
  if((DFS_dummy_read_center_address - SZ_4K) >= high_memory_phys){
    printk("[DFS Check]DFS Dummy read reserved 0x%x to 0x%x\n",DFS_dummy_read_center_address-SZ_4K,DFS_dummy_read_center_address+SZ_4K);
    memblock_reserve(DFS_dummy_read_center_address-SZ_4K,SZ_4K*2);
    memblock_free(DFS_dummy_read_center_address-SZ_4K, SZ_4K*2);
    memblock_remove(DFS_dummy_read_center_address-SZ_4K,SZ_4K*2);
  }
  else{
    printk("[DFS Check]DFS Dummy read reserved 0x%x to 0x%x\n",DFS_dummy_read_center_address-SZ_1M,DFS_dummy_read_center_address+SZ_1M);
    memblock_reserve(DFS_dummy_read_center_address-SZ_1M,SZ_1M*2);
    memblock_free(DFS_dummy_read_center_address-SZ_1M,SZ_1M*2);
    memblock_remove(DFS_dummy_read_center_address-SZ_1M,SZ_1M*2);
  }
  return;
}
int get_DRAM_default_voltage(void)
{
  unsigned int DRAM_Type;
  if(OTA_Not_used==1)
  {
    printk("[OTA DRAM] update DDR-1466 PATCH With critical patch\n");
    DRAM_Type=get_ddr_type();
    return dram_info.dram_vcore;
  }
  else
  {
    DRAM_Type=get_ddr_type();
    if(DRAM_Type==LPDDR3)
    {
      if((*((volatile unsigned int *)(0xF0206174))& 0x1)==1)
      {
        printk("[OTA DRAM] DRAM Default Voltage Voltage_1_075 \n");
        return Voltage_1_075;
      }
      else
      {
        printk("[OTA DRAM] DRAM Default Voltage Voltage_1_125 \n");
        return Voltage_1_125;
      }

    }
    else if(DRAM_Type==LPDDR2)
    {
      printk("[OTA DRAM] DRAM Default Voltage Voltage_1_0 \n");
      return  Voltage_1_0;
    }
    else if (DRAM_Type==DDR3_16 || DRAM_Type==DDR3_32)
    {
      printk("[OTA DRAM] DRAM Default Voltage Voltage_1_125 \n");
      return Voltage_1_125;      
    }
    else
    {
      printk("[OTA DRAM] DRAM Type Error\n");
      BUG_ON(1);
    }

  }

}
int get_DRAM_default_freq(void)
{
   unsigned int mempll9;
   unsigned int speed_value;
   if(OTA_Not_used==1)
   {
     return dram_info.dram_speed;
   }
   else
   {
     mempll9=DRAMC_READ_REG(0x624);
     speed_value=(mempll9 & 0xff000000)>> 24;
     if(speed_value==0xb1)
     {
       printk("[OTA DRAM] Default Speed 1466 \n");
       return speed_367;
     }
     else if(speed_value==0xa1)
     {
       printk("[OTA DRAM] Default Speed 1333 \n");
       return speed_333;
     }
     else if((speed_value==0xa0)||((speed_value>>4)==0x8))
     {
       printk("[OTA DRAM] Default Speed 1066 \n");
       return speed_266;
     }
     else
     {
       printk("[OTA DRAM] DRAM Default Speed error\n ");
       BUG_ON(1);
     }
   }   
}


int DFS_Detection(void)
{
   unsigned int DRAM_Type;
   if(get_ddr_type){
      DRAM_Type=get_ddr_type();
      printk("dummy regs 0x%x\n",*((unsigned int *)(DRAMC0_BASE + 0xf4)));
      printk("check 0x%x\n",(0x1 <<15));
      if(DRAM_Type == LPDDR3)
      {
         if(*((unsigned int *)(DRAMC0_BASE + 0xf4))&(0x1 <<15))
         {
           printk("[LPDDR3] DFS could be enable 1066 to 1333  \n");
           return 1;
         }
         else if (*((unsigned int *)(DRAMC0_BASE + 0xf4))&(0x1 <<7))
         {
           printk("[LPDDR3] DFS could be enable 1066 to 1466  \n");
           return 3;           
         }  
         else
         {
           printk("[LPDDR3] DFS could not be enable\n");
           return -1;
         }
      }
      else
      {
         printk("[LPDDR2] do not support DFS\n");
         return -1;
      }
   }
   else
   {
       printk("do not support get_ddr_type\n");
       ASSERT(0);
       return -1;
   }
  return -1;
}

int get_rank_num(void)
{
  volatile unsigned int emi_cona;
  emi_cona=readl(EMI_CONA);
  return (emi_cona & 0x20000) ? 2 : 1;
}
#define MRW_PASR_VALID	0xF0000000
extern void spm_set_pasr(u32 value);
int configure_mrw_pasr(unsigned char  segment_rank0,unsigned char segment_rank1)
{
   unsigned int ddr_type=0;
   if(get_rank_num()!=2)
   {
     spm_set_pasr(0);
     printk("do not support Single Rank PASR\n");
     return -1;
   }
    
   if(get_ddr_type){
      ddr_type=get_ddr_type(); 
      if(ddr_type == LPDDR3 || ddr_type == LPDDR2)
      {
	 spm_set_pasr(MRW_PASR_VALID | segment_rank0 | (segment_rank1 << 16));
      }
      else if(ddr_type==DDR3_16||ddr_type==DDR3_32)
      {
	 spm_set_pasr(0);
         printk("do not support DDR3 PASR\n");
         /*ASSERT(0); No need to do ASSERT & return -1
         return -1;*/
      }
   }
   else
   {
       spm_set_pasr(0);
       printk("do not support get_ddr_type\n");
       /*ASSERT(0); No need to do ASSERT & return -1
       return -1;*/
   }
   return 0;
}

/* We could use an array to construct the relationship between DDR-type and PASR. */
bool pasr_is_valid(void)
{
	unsigned int ddr_type=0;

	if(get_ddr_type) {
		ddr_type=get_ddr_type();
		/* Following DDR types can support PASR */
      		if(ddr_type == LPDDR3 || ddr_type == LPDDR2) {
			return true;
		}
	}

	return false;
}
void dump_DMA_Reserved_AREA(void)
{
  int i;
  int j;
  int k;
  unsigned int *debug_p_src;
  unsigned int *debug_p_dst;

  debug_p_src=src_array_v;
  debug_p_dst=dst_array_v;

  printk("=====DMA DUMP Reserved SRC AREA======\n");
  /*dump memory info to make sure 0x6a6a6a6a already write into memory*/
  for(i=0;i<8;i++)
  {
   for(j=0;j<8;j++)
     printk("&0x%x=0x%x ",((debug_p_src+i*8)+j),*((debug_p_src+i*8)+j));
   printk("\n");
  }
  printk("=====DMA DUMP Reserved SRC AREA END======\n\n");
  printk("=====DMA DUMP Reserved DST AREA======\n");
  for(i=0;i<8;i++)
  {
   for(j=0;j<8;j++)
     printk("&0x%x=0x%x ",((debug_p_dst+i*8)+j),*((debug_p_dst+i*8)+j));
   printk("\n");
  }
  printk("=====DMA DUMP Reserved DST AREA END======\n\n");
  enable_clock(MT_CG_PERI_AP_DMA, "DFS_GDMA_MODULE");
    printk("=====DMA DUMP REGS======\n");
    for (k=0;k<15;k++)
   {
     printk("[AFTER]addr:0x%x, value:%x\n",(0xF1000080+k*4),*((volatile int *)(0xF1000080+k*4)));
   }

     printk("[AFTER]addr:0x%x, value:%x\n",(0xF10000D0),*((volatile int *)(0xF10000D0)));
    for(k=0;k<31;k++)
   {
    printk("[AFTER]addr:0x%x, value:%x\n",(0xF1000000+k*4),*((volatile int *)(0xF1000000+k*4)));
   }

   disable_clock(MT_CG_PERI_AP_DMA, "DFS_GDMA_MODULE");

   printk("====END=====\n");

}
//#define VOLTAGE_DMA_DEBUG
void DFS_Phase_delay(void)
{
  unsigned long long delay_start;
  unsigned long  long delay_end;
  unsigned long  long one_hundread_us;
  delay_start=0;
  delay_end=0;
  delay_start=sched_clock();
  delay_end=delay_start+0x19000;
#ifdef VOLTAGE_DMA_DEBUG
  printk("[Time Limit]delay_start:%llu, delay_end:%llu, delay(ns):%llu\n",delay_start,delay_end,delay_end-delay_start);
#endif
  Voltage_DMA_counter=0;
  while(delay_start < delay_end)
  {
    DFS_APDMA_Enable();
    DFS_APDMA_END();
    Voltage_DMA_counter++;
      delay_start=sched_clock();
  }
#ifdef VOLTAGE_DMA_DEBUG
  printk("[Timer accuracy]delay_start:%llu, delay_end:%llu, delay:%llu\n",delay_start,delay_end,delay_start-delay_end);
  printk("Voltage DMA Counter:0x%x\n",Voltage_DMA_counter);
#endif
  return;
}

int get_ddr_type(void)
{
  unsigned int value;

  value = DRAMC_READ_REG(DRAMC_LPDDR2);
  if((value>>28) & 0x1) //check LPDDR2_EN
  {
    return LPDDR2;
  }

  value = DRAMC_READ_REG(DRAMC_PADCTL4);
  if((value>>7) & 0x1) //check DDR3_EN
  {
    if (DRAMC_READ_REG(DRAMC_CONF1) & 0x1)
    {
      return DDR3_32;
    }
    else
    {
      return DDR3_16;
    }
  }

  value = DRAMC_READ_REG(DRAMC_ACTIM1);
  if((value>>28) & 0x1) //check LPDDR3_EN
  {
    return LPDDR3;
  }

  return mDDR;
}



arch_initcall(dram_overclock_init);
