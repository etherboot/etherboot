/* Header for pxe_export.c
 */

#ifndef PXE_EXPORT_H
#define PXE_EXPORT_H

#include "pxe.h"

/* export_pxe.c */
extern int pxe_api_call ( int opcode, t_PXENV_ANY *params );

#endif /* PXE_EXPORT_H */
