






#ifndef BMC_NIB_H
#define BMC_NIB_H

#ifdef __cplusplus
extern "C" {
#endif


#define BMC_NIB_DIR "SD:/nib-g64"


int bmc_nib_in_g64(const char *nib_path, const char *g64_path);


int bmc_nib_e_nib(const char *path);



int bmc_nib_prepara(const char *nib_path, char *out, unsigned int out_len);

#ifdef __cplusplus
}
#endif

#endif
