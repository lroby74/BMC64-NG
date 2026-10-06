


#include <fatfs/ff.h>
#include "bmc_nib.h"

void bmc_nib_cartella(void) {
  f_mkdir(BMC_NIB_DIR);
}
