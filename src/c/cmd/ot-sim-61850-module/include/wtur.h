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
extern DataAttribute iedModel_WTG_WTUR1_TotWh_actVal;
extern DataAttribute iedModel_WTG_WTUR1_TotWh_pulsQty;
extern DataAttribute iedModel_WTG_WTUR1_TotWh_q;
extern DataAttribute iedModel_WTG_WTUR1_TotWh_t;

extern DataObject    iedModel_WTG_WTUR1_TurSt;
extern DataAttribute iedModel_WTG_WTUR1_TurSt_stVal;
extern DataAttribute iedModel_WTG_WTUR1_TurSt_q;
extern DataAttribute iedModel_WTG_WTUR1_TurSt_t;

extern DataObject    iedModel_WTG_WTUR1_W;
extern DataAttribute iedModel_WTG_WTUR1_W_mag;
extern DataAttribute iedModel_WTG_WTUR1_W_mag_i;
extern DataAttribute iedModel_WTG_WTUR1_W_mag_f;
extern DataAttribute iedModel_WTG_WTUR1_W_q;
extern DataAttribute iedModel_WTG_WTUR1_W_t;

extern DataObject    iedModel_WTG_WTUR1_TurOp;
extern DataAttribute iedModel_WTG_WTUR1_TurOp_stVal;
extern DataAttribute iedModel_WTG_WTUR1_TurOp_q;
extern DataAttribute iedModel_WTG_WTUR1_TurOp_t;
extern DataAttribute iedModel_WTG_WTUR1_TurOp_Oper;
extern DataAttribute iedModel_WTG_WTUR1_TurOp_Oper_ctlVal;
extern DataAttribute iedModel_WTG_WTUR1_TurOp_Oper_origin;
extern DataAttribute iedModel_WTG_WTUR1_TurOp_Oper_origin_orCat;
extern DataAttribute iedModel_WTG_WTUR1_TurOp_Oper_origin_orIdent;
extern DataAttribute iedModel_WTG_WTUR1_TurOp_Oper_ctlNum;
extern DataAttribute iedModel_WTG_WTUR1_TurOp_Oper_T;
extern DataAttribute iedModel_WTG_WTUR1_TurOp_Oper_Test;
extern DataAttribute iedModel_WTG_WTUR1_TurOp_Oper_Check;

#endif // WTUR_H