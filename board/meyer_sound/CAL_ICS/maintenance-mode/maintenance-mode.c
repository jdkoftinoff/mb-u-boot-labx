/* Check if the OLED button is pushed, if pushed, go to regular boot up mode;
   if not pushed, go to maintenance mode.
   
   Yi Cao -- Lab X Technologies, LLC (2011)
   yi.cao@labxtechnologies.com
*/

#include "xparameters.h"
#include <linux/types.h>
#include <common.h>
#include <config.h>
#include <command.h>
#include <u-boot/crc.h>
#include "microblaze_fsl.h"


#define REGULAR_FPGA_BASE (0x00800000)
#define GOLDEN_FPGA_BASE (0x00000000)

void ReconfigureFpgaImage(void)
{
  u32 val;
  // ICAP behavior is described (poorly) in Xilinx specification UG380.  Brave
  // souls may look there for detailed guidance on what is being done here.
  
  // It has been empirically determined that ICAP FSL doesn't always work
  // the first time, but if retried enough times it does eventually work.
  // Thus we keep hammering the operation we want and checking for failure
  // until we finally succeed.  Somebody please fix ICAP!! <sigh>

  // Abort anything in progress
	
  printf("Reconfiguring the FPGA\n");
  do {
    putfslx(0x0FFFF, 0, FSL_CONTROL); // Control signal aborts, data doesn't matter
    udelay(1000);
    getfsl(val, 0); // Read the ICAP result
  } while ((val & ICAP_FSL_FAILED) != 0);

  do {
    // Synchronize command bytes
    putfsl(0x0FFFF, 0); // Pad words
    putfsl(0x0FFFF, 0);
    putfsl(0x0AA99, 0); // SYNC
    putfsl(0x05566, 0); // SYNC
        
    getfslx(val, 0, FSL_NONBLOCKING); // Read the ICAP result
    udelay(100000);

    // Set the Mode register so that fallback images will be manipulated
    // correctly.  Use bitstream mode instead of physical mode (required
    // for configuration fallback) and set boot mode for BPI
    getfslx(val, 0, FSL_NONBLOCKING); // Read the ICAP result
    udelay(100000);
        
    putfsl(0x03301, 0); // Write MODE_REG
        
    udelay(100000);
        
    putfsl(0x02100, 0); // Value 0 allows u-boot to use production image
        
    udelay(100000);
        
    // Write the reconfiguration FPGA offset; the base address of the
    // "run-time" FPGA is #defined as a byte address, but the ICAP needs
    // a 16-bit half-word address, so we shift right by one extra bit.
    putfsl(0x03261, 0); // Write GENERAL1
    putfsl(((REGULAR_FPGA_BASE) & 0x0FFFF), 0); // Multiboot start address[15:0]

    putfsl(0x03281, 0); // Write GENERAL2
    putfsl(((REGULAR_FPGA_BASE >> 16) & 0x0FF) | 0x0300, 0); // Opcode 0x3 and address[23:16]

    udelay(100000);

    // Write the fallback FPGA offset (this image)
    putfsl(0x032A1, 0); // Write GENERAL3
    putfsl(((GOLDEN_FPGA_BASE >> 0) & 0x0FFFF), 0);
    putfsl(0x032C1, 0); // Write GENERAL4
    putfsl(((GOLDEN_FPGA_BASE >> 16) & 0x0FF) | 0x300, 0);
    putfsl(0x032E1, 0); // Write GENERAL5
    putfsl(0x00, 0); // Value 0 allows u-boot to use production image

    udelay(100000);
        
    // Write IPROG command
    putfsl(0x030A1, 0); // Write CMD
    putfsl(0x0000E, 0); // IPROG Command
        
    udelay(100000);
        
    // Add some safety noops
    putfsl(0x02000, 0); // Type 1 NOP
    	
    udelay(100000);
      
    getfslx(val, 0, FSL_NONBLOCKING); // Read the ICAP result
    udelay(100000);
        
    putfsl(FINISH_FSL_BIT | 0x02000, 0); // Type 1 NOP, and Trigger the FSL peripheral to drain the FIFO into the ICAP
        
    udelay(100000);
        
    __udelay (1000);
    getfsl(val, 0); // Read the ICAP result
		
    udelay(100000);
        
  } while ((val & ICAP_FSL_FAILED) != 0);

  while(1);
}

void CheckMaintenanceMode()
{
  unsigned long reg;
  
  /* read the GPIO2_TRI register*/
  reg = *((unsigned long *)(XPAR_XPS_GPIO_0_BASEADDR + 0x0C));  
  /* set bit 6 as output */
  reg |= 0x10000;
  *((unsigned long *)(XPAR_XPS_GPIO_0_BASEADDR + 0x0C)) = reg;
  
  /*read the GPIO2_Data register */
  reg = *((unsigned long *)(XPAR_XPS_GPIO_0_BASEADDR + 0x08));
  /* check bit 6 for SMART_SWITCH_CONTACT. If the button is pushed, 
  the register bit value is 0; if not pushed, the register bit is 1
  */
  reg &= 0x10000;
  if (reg == 0) // button pushed, continue with maintenance mode
  {
    printf("\n\n Button pushed, continue with maintenance mode!\n\n");
    
  }
    
  else // button not pushed, go to regular mode
  {
    printf("\n\n Button not pushed, go to regular mode!\n\n");
    ReconfigureFpgaImage();
  }
}
