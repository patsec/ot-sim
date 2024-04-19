#include "ied.h"
#include "wtur.h"

LogicalNode iedModel_WTG_WTUR1 = {
  LogicalNodeModelType,
  "WTUR1",
  (ModelNode*) &iedModel_WTG,
  NULL,
  (ModelNode*) &iedModel_WTG_WTUR1_Beh,
};

DataObject iedModel_WTG_WTUR1_Beh = {
  DataObjectModelType,
  "NamPlt",
  (ModelNode*) &iedModel_WTG_WTUR1,
  (ModelNode*) &iedModel_WTG_WTUR1_TotWh,
  (ModelNode*) &iedModel_WTG_WTUR1_Beh_stVal,
  0
};

DataAttribute iedModel_WTG_WTUR1_Beh_stVal = {
  DataAttributeModelType,
  "vendor",
  (ModelNode*) &iedModel_WTG_WTUR1_Beh,
  (ModelNode*) &iedModel_WTG_WTUR1_Beh_q,
  NULL,
  0,
  IEC61850_FC_ST,
  IEC61850_INT32,
  0 + TRG_OPT_DATA_CHANGED,
  NULL,
  0
};

DataAttribute iedModel_WTG_WTUR1_Beh_q = {
  DataAttributeModelType,
  "swRev",
  (ModelNode*) &iedModel_WTG_WTUR1_Beh,
  (ModelNode*) &iedModel_WTG_WTUR1_Beh_t,
  NULL,
  0,
  IEC61850_FC_ST,
  IEC61850_QUALITY,
  0 + TRG_OPT_QUALITY_CHANGED,
  NULL,
  0
};

DataAttribute iedModel_WTG_WTUR1_Beh_t = {
  DataAttributeModelType,
  "configRev",
  (ModelNode*) &iedModel_WTG_WTUR1_Beh,
  NULL,
  NULL,
  0,
  IEC61850_FC_ST,
  IEC61850_TIMESTAMP,
  0,
  NULL,
  0
};

DataObject iedModel_WTG_WTUR1_TotWh = {
  DataObjectModelType,
  "TotWh",
  (ModelNode*) &iedModel_WTG_WTUR1,
  (ModelNode*) &iedModel_WTG_WTUR1_TurSt,
  (ModelNode*) &iedModel_WTG_WTUR1_TotWh_cntVal,
  0
};

DataObject iedModel_WTG_WTUR1_TotWh_cntVal = {
  DataObjectModelType,
  "cntVal",
  (ModelNode*) &iedModel_WTG_WTUR1_TotWh,
  NULL,
  (ModelNode*) &iedModel_WTG_WTUR1_TotWh_cntVal_actVal,
  0
};

DataAttribute iedModel_WTG_WTUR1_TotWh_cntVal_actVal = {
  DataAttributeModelType,
  "actVal",
  (ModelNode*) &iedModel_WTG_WTUR1_TotWh_cntVal,
  (ModelNode*) &iedModel_WTG_WTUR1_TotWh_cntVal_pulsQty,
  NULL,
  0,
  IEC61850_FC_ST,
  IEC61850_INT64,
  0 + TRG_OPT_DATA_CHANGED,
  NULL,
  0
};

DataAttribute iedModel_WTG_WTUR1_TotWh_cntVal_pulsQty = {
  DataAttributeModelType,
  "pulsQty",
  (ModelNode*) &iedModel_WTG_WTUR1_TotWh_cntVal,
  (ModelNode*) &iedModel_WTG_WTUR1_TotWh_cntVal_q,
  NULL,
  0,
  IEC61850_FC_CF,
  IEC61850_FLOAT32,
  0 + TRG_OPT_DATA_CHANGED,
  NULL,
  0
};

DataAttribute iedModel_WTG_WTUR1_TotWh_cntVal_q = {
  DataAttributeModelType,
  "q",
  (ModelNode*) &iedModel_WTG_WTUR1_TotWh_cntVal,
  (ModelNode*) &iedModel_WTG_WTUR1_TotWh_cntVal_t,
  NULL,
  0,
  IEC61850_FC_ST,
  IEC61850_QUALITY,
  0 + TRG_OPT_QUALITY_CHANGED,
  NULL,
  0
};

DataAttribute iedModel_WTG_WTUR1_TotWh_cntVal_t = {
  DataAttributeModelType,
  "t",
  (ModelNode*) &iedModel_WTG_WTUR1_TotWh_cntVal,
  NULL,
  NULL,
  0,
  IEC61850_FC_ST,
  IEC61850_TIMESTAMP,
  0,
  NULL,
  0
};

DataObject iedModel_WTG_WTUR1_TurSt = {
  DataObjectModelType,
  "TurSt",
  (ModelNode*) &iedModel_WTG_WTUR1,
  (ModelNode*) &iedModel_WTG_WTUR1_W,
  (ModelNode*) &iedModel_WTG_WTUR1_TurSt_st,
  0
};

DataObject iedModel_WTG_WTUR1_TurSt_st = {
  DataObjectModelType,
  "st",
  (ModelNode*) &iedModel_WTG_WTUR1_TurSt,
  NULL,
  (ModelNode*) &iedModel_WTG_WTUR1_TurSt_st_stVal,
  0
};

DataAttribute iedModel_WTG_WTUR1_TurSt_st_stVal = {
  DataAttributeModelType,
  "stVal",
  (ModelNode*) &iedModel_WTG_WTUR1_TurSt_st,
  (ModelNode*) &iedModel_WTG_WTUR1_TurSt_st_q,
  NULL,
  0,
  IEC61850_FC_ST,
  IEC61850_INT32,
  0 + TRG_OPT_DATA_CHANGED,
  NULL,
  0
};

DataAttribute iedModel_WTG_WTUR1_TurSt_st_q = {
  DataAttributeModelType,
  "q",
  (ModelNode*) &iedModel_WTG_WTUR1_TurSt_st,
  (ModelNode*) &iedModel_WTG_WTUR1_TurSt_st_t,
  NULL,
  0,
  IEC61850_FC_ST,
  IEC61850_QUALITY,
  0 + TRG_OPT_QUALITY_CHANGED,
  NULL,
  0
};

DataAttribute iedModel_WTG_WTUR1_TurSt_st_t = {
  DataAttributeModelType,
  "t",
  (ModelNode*) &iedModel_WTG_WTUR1_TurSt_st,
  NULL,
  NULL,
  0,
  IEC61850_FC_ST,
  IEC61850_TIMESTAMP,
  0,
  NULL,
  0
};

DataObject iedModel_WTG_WTUR1_W = {
  DataObjectModelType,
  "W",
  (ModelNode*) &iedModel_WTG_WTUR1,
  (ModelNode*) &iedModel_WTG_WTUR1_TurOp,
  (ModelNode*) &iedModel_WTG_WTUR1_W_mag,
  0
};

DataAttribute iedModel_WTG_WTUR1_W_mag = {
  DataAttributeModelType,
  "mag",
  (ModelNode*) &iedModel_WTG_WTUR1_W,
  (ModelNode*) &iedModel_WTG_WTUR1_W_q,
  (ModelNode*) &iedModel_WTG_WTUR1_W_mag_i,
  0,
  IEC61850_FC_MX,
  IEC61850_CONSTRUCTED,
  0,
  NULL,
  0
};

DataAttribute iedModel_WTG_WTUR1_W_mag_i = {
  DataAttributeModelType,
  "i",
  (ModelNode*) &iedModel_WTG_WTUR1_W_mag,
  (ModelNode*) &iedModel_WTG_WTUR1_W_mag_f,
  NULL,
  0,
  IEC61850_FC_MX,
  IEC61850_INT32,
  0,
  NULL,
  0
};

DataAttribute iedModel_WTG_WTUR1_W_mag_f = {
  DataAttributeModelType,
  "f",
  (ModelNode*) &iedModel_WTG_WTUR1_W_mag,
  NULL,
  NULL,
  0,
  IEC61850_FC_MX,
  IEC61850_FLOAT32,
  0,
  NULL,
  0
};

DataAttribute iedModel_WTG_WTUR1_W_q = {
  DataAttributeModelType,
  "q",
  (ModelNode*) &iedModel_WTG_WTUR1_W,
  (ModelNode*) &iedModel_WTG_WTUR1_W_t,
  NULL,
  0,
  IEC61850_FC_MX,
  IEC61850_QUALITY,
  0 + TRG_OPT_QUALITY_CHANGED,
  NULL,
  0
};

DataAttribute iedModel_WTG_WTUR1_W_t = {
  DataAttributeModelType,
  "t",
  (ModelNode*) &iedModel_WTG_WTUR1_W,
  NULL,
  NULL,
  0,
  IEC61850_FC_MX,
  IEC61850_TIMESTAMP,
  0,
  NULL,
  0
};

DataObject iedModel_WTG_WTUR1_TurOp = {
  DataObjectModelType,
  "TurOp",
  (ModelNode*) &iedModel_WTG_WTUR1,
  NULL,
  (ModelNode*) &iedModel_WTG_WTUR1_TurOp_st,
  0
};

DataObject iedModel_WTG_WTUR1_TurOp_st = {
  DataObjectModelType,
  "st",
  (ModelNode*) &iedModel_WTG_WTUR1_TurOp,
  NULL,
  (ModelNode*) &iedModel_WTG_WTUR1_TurOp_st_stVal,
  0
};

DataAttribute iedModel_WTG_WTUR1_TurOp_st_stVal = {
  DataAttributeModelType,
  "stVal",
  (ModelNode*) &iedModel_WTG_WTUR1_TurOp_st,
  (ModelNode*) &iedModel_WTG_WTUR1_TurOp_st_q,
  NULL,
  0,
  IEC61850_FC_ST,
  IEC61850_INT32,
  0 + TRG_OPT_DATA_CHANGED,
  NULL,
  0
};

DataAttribute iedModel_WTG_WTUR1_TurOp_st_q = {
  DataAttributeModelType,
  "q",
  (ModelNode*) &iedModel_WTG_WTUR1_TurOp_st,
  (ModelNode*) &iedModel_WTG_WTUR1_TurOp_st_t,
  NULL,
  0,
  IEC61850_FC_ST,
  IEC61850_QUALITY,
  0 + TRG_OPT_QUALITY_CHANGED,
  NULL,
  0
};

DataAttribute iedModel_WTG_WTUR1_TurOp_st_t = {
  DataAttributeModelType,
  "t",
  (ModelNode*) &iedModel_WTG_WTUR1_TurOp_st,
  NULL,
  NULL,
  0,
  IEC61850_FC_ST,
  IEC61850_TIMESTAMP,
  0,
  NULL,
  0
};