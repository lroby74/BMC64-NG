//

//






//
#ifndef _circle_bmc_link_nota_h
#define _circle_bmc_link_nota_h

enum
{
	BMC_LINK_NIENTE = 0,
	BMC_LINK_GIU,
	BMC_LINK_SU,
	BMC_LINK_TORNATO
};



static inline int bmc_link_nota (int nLink, int *pDetto, unsigned long long *pGiuDa,
				 unsigned long long nAdesso, unsigned *pGiuSecondi)
{
	if (nLink)
	{
		if (*pDetto == 1)
		{
			return BMC_LINK_NIENTE;
		}
		const int nPrima = *pDetto;
		*pDetto = 1;
		if (nPrima == 0)
		{
			*pGiuSecondi = (unsigned) ((nAdesso - *pGiuDa) / 1000000ULL);
			return BMC_LINK_TORNATO;
		}
		return BMC_LINK_SU;
	}

	if (*pDetto != 1)
	{
		return BMC_LINK_NIENTE;
	}
	*pDetto = 0;
	*pGiuDa = nAdesso;
	return BMC_LINK_GIU;
}

#endif
