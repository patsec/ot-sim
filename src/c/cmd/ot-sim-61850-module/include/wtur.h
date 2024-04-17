#ifndef WTUR_H
#define WTUR_H

#include <stdlib.h>
#include <libiec61850/iec61850_model.h>

extern LogicalNode   iedModel_WTG_WTUR1;

extern DataObject    iedModel_WTG_WTUR1_Beh;
extern DataAttribute iedModel_WTG_WTUR1_Beh_stVal;
extern DataAttribute iedModel_WTG_WTUR1_Beh_q;
extern DataAttribute iedModel_WTG_WTUR1_Beh_t;

extern DataObject    iedModel_WTG_WTUR1_TotWh;

extern DataObject    iedModel_WTG_WTUR1_TotWh_cntVal;
extern DataAttribute iedModel_WTG_WTUR1_TotWh_cntVal_actVal;
extern DataAttribute iedModel_WTG_WTUR1_TotWh_cntVal_pulsQty;
extern DataAttribute iedModel_WTG_WTUR1_TotWh_cntVal_q;
extern DataAttribute iedModel_WTG_WTUR1_TotWh_cntVal_t;

extern DataObject    iedModel_WTG_WTUR1_TurSt;

extern DataObject    iedModel_WTG_WTUR1_TurSt_st;
extern DataAttribute iedModel_WTG_WTUR1_TurSt_st_stVal;
extern DataAttribute iedModel_WTG_WTUR1_TurSt_st_q;
extern DataAttribute iedModel_WTG_WTUR1_TurSt_st_t;

extern DataObject    iedModel_WTG_WTUR1_W;

extern DataAttribute iedModel_WTG_WTUR1_W_mag;
extern DataAttribute iedModel_WTG_WTUR1_W_mag_i;
extern DataAttribute iedModel_WTG_WTUR1_W_mag_f;
extern DataAttribute iedModel_WTG_WTUR1_W_q;
extern DataAttribute iedModel_WTG_WTUR1_W_t;

extern DataObject    iedModel_WTG_WTUR1_TurOp;

extern DataObject    iedModel_WTG_WTUR1_TurOp_st;
extern DataAttribute iedModel_WTG_WTUR1_TurOp_st_stVal;
extern DataAttribute iedModel_WTG_WTUR1_TurOp_st_q;
extern DataAttribute iedModel_WTG_WTUR1_TurOp_st_t;

#endif // WTUR_H