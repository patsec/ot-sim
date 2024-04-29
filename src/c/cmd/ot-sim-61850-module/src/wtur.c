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
  (ModelNode*) &iedModel_WTG_WTUR1_TotWh_actVal,
  0
};

DataAttribute iedModel_WTG_WTUR1_TotWh_actVal = {
  DataAttributeModelType,
  "actVal",
  (ModelNode*) &iedModel_WTG_WTUR1_TotWh,
  (ModelNode*) &iedModel_WTG_WTUR1_TotWh_pulsQty,
  NULL,
  0,
  IEC61850_FC_ST,
  IEC61850_INT64,
  0 + TRG_OPT_DATA_CHANGED,
  NULL,
  0
};

DataAttribute iedModel_WTG_WTUR1_TotWh_pulsQty = {
  DataAttributeModelType,
  "pulsQty",
  (ModelNode*) &iedModel_WTG_WTUR1_TotWh,
  (ModelNode*) &iedModel_WTG_WTUR1_TotWh_q,
  NULL,
  0,
  IEC61850_FC_CF,
  IEC61850_FLOAT32,
  0 + TRG_OPT_DATA_CHANGED,
  NULL,
  0
};

DataAttribute iedModel_WTG_WTUR1_TotWh_q = {
  DataAttributeModelType,
  "q",
  (ModelNode*) &iedModel_WTG_WTUR1_TotWh,
  (ModelNode*) &iedModel_WTG_WTUR1_TotWh_t,
  NULL,
  0,
  IEC61850_FC_ST,
  IEC61850_QUALITY,
  0 + TRG_OPT_QUALITY_CHANGED,
  NULL,
  0
};

DataAttribute iedModel_WTG_WTUR1_TotWh_t = {
  DataAttributeModelType,
  "t",
  (ModelNode*) &iedModel_WTG_WTUR1_TotWh,
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
  (ModelNode*) &iedModel_WTG_WTUR1_TurSt_stVal,
  0
};

DataAttribute iedModel_WTG_WTUR1_TurSt_stVal = {
  DataAttributeModelType,
  "stVal",
  (ModelNode*) &iedModel_WTG_WTUR1_TurSt,
  (ModelNode*) &iedModel_WTG_WTUR1_TurSt_q,
  NULL,
  0,
  IEC61850_FC_ST,
  IEC61850_INT32,
  0 + TRG_OPT_DATA_CHANGED,
  NULL,
  0
};

DataAttribute iedModel_WTG_WTUR1_TurSt_q = {
  DataAttributeModelType,
  "q",
  (ModelNode*) &iedModel_WTG_WTUR1_TurSt,
  (ModelNode*) &iedModel_WTG_WTUR1_TurSt_t,
  NULL,
  0,
  IEC61850_FC_ST,
  IEC61850_QUALITY,
  0 + TRG_OPT_QUALITY_CHANGED,
  NULL,
  0
};

DataAttribute iedModel_WTG_WTUR1_TurSt_t = {
  DataAttributeModelType,
  "t",
  (ModelNode*) &iedModel_WTG_WTUR1_TurSt,
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
  (ModelNode*) &iedModel_WTG_WTUR1_TurOp_stVal,
  0
};

DataAttribute iedModel_WTG_WTUR1_TurOp_stVal = {
  DataAttributeModelType,
  "stVal",
  (ModelNode*) &iedModel_WTG_WTUR1_TurOp,
  (ModelNode*) &iedModel_WTG_WTUR1_TurOp_q,
  NULL,
  0,
  IEC61850_FC_ST,
  IEC61850_INT32,
  0 + TRG_OPT_DATA_CHANGED,
  NULL,
  0
};

DataAttribute iedModel_WTG_WTUR1_TurOp_q = {
  DataAttributeModelType,
  "q",
  (ModelNode*) &iedModel_WTG_WTUR1_TurOp,
  (ModelNode*) &iedModel_WTG_WTUR1_TurOp_t,
  NULL,
  0,
  IEC61850_FC_ST,
  IEC61850_QUALITY,
  0 + TRG_OPT_QUALITY_CHANGED,
  NULL,
  0
};

DataAttribute iedModel_WTG_WTUR1_TurOp_t = {
  DataAttributeModelType,
  "t",
  (ModelNode*) &iedModel_WTG_WTUR1_TurOp,
  (ModelNode*) &iedModel_WTG_WTUR1_TurOp_Oper,
  NULL,
  0,
  IEC61850_FC_ST,
  IEC61850_TIMESTAMP,
  0,
  NULL,
  0
};

DataAttribute iedModel_WTG_WTUR1_TurOp_Oper = {
  DataAttributeModelType,
  "Oper",
  (ModelNode*) &iedModel_WTG_WTUR1_TurOp,
  NULL,
  (ModelNode*) &iedModel_WTG_WTUR1_TurOp_Oper_ctlVal,
  0,
  IEC61850_FC_CO,
  IEC61850_CONSTRUCTED,
  0,
  NULL,
  0
};

DataAttribute iedModel_WTG_WTUR1_TurOp_Oper_ctlVal = {
  DataAttributeModelType,
  "ctlVal",
  (ModelNode*) &iedModel_WTG_WTUR1_TurOp_Oper,
  (ModelNode*) &iedModel_WTG_WTUR1_TurOp_Oper_origin,
  NULL,
  0,
  IEC61850_FC_CO,
  IEC61850_BOOLEAN,
  0,
  NULL,
  0
};

DataAttribute iedModel_WTG_WTUR1_TurOp_Oper_origin = {
  DataAttributeModelType,
  "origin",
  (ModelNode*) &iedModel_WTG_WTUR1_TurOp_Oper,
  (ModelNode*) &iedModel_WTG_WTUR1_TurOp_Oper_ctlNum,
  (ModelNode*) &iedModel_WTG_WTUR1_TurOp_Oper_origin_orCat,
  0,
  IEC61850_FC_CO,
  IEC61850_CONSTRUCTED,
  0,
  NULL,
  0
};

DataAttribute iedModel_WTG_WTUR1_TurOp_Oper_origin_orCat = {
  DataAttributeModelType,
  "orCat",
  (ModelNode*) &iedModel_WTG_WTUR1_TurOp_Oper_origin,
  (ModelNode*) &iedModel_WTG_WTUR1_TurOp_Oper_origin_orIdent,
  NULL,
  0,
  IEC61850_FC_CO,
  IEC61850_ENUMERATED,
  0,
  NULL,
  0
};

DataAttribute iedModel_WTG_WTUR1_TurOp_Oper_origin_orIdent = {
  DataAttributeModelType,
  "orIdent",
  (ModelNode*) &iedModel_WTG_WTUR1_TurOp_Oper_origin,
  NULL,
  NULL,
  0,
  IEC61850_FC_CO,
  IEC61850_OCTET_STRING_64,
  0,
  NULL,
  0
};

DataAttribute iedModel_WTG_WTUR1_TurOp_Oper_ctlNum = {
  DataAttributeModelType,
  "ctlNum",
  (ModelNode*) &iedModel_WTG_WTUR1_TurOp_Oper,
  (ModelNode*) &iedModel_WTG_WTUR1_TurOp_Oper_T,
  NULL,
  0,
  IEC61850_FC_CO,
  IEC61850_INT8U,
  0,
  NULL,
  0
};

DataAttribute iedModel_WTG_WTUR1_TurOp_Oper_T = {
  DataAttributeModelType,
  "T",
  (ModelNode*) &iedModel_WTG_WTUR1_TurOp_Oper,
  (ModelNode*) &iedModel_WTG_WTUR1_TurOp_Oper_Test,
  NULL,
  0,
  IEC61850_FC_CO,
  IEC61850_TIMESTAMP,
  0,
  NULL,
  0
};

DataAttribute iedModel_WTG_WTUR1_TurOp_Oper_Test = {
  DataAttributeModelType,
  "Test",
  (ModelNode*) &iedModel_WTG_WTUR1_TurOp_Oper,
  (ModelNode*) &iedModel_WTG_WTUR1_TurOp_Oper_Check,
  NULL,
  0,
  IEC61850_FC_CO,
  IEC61850_BOOLEAN,
  0,
  NULL,
  0
};

DataAttribute iedModel_WTG_WTUR1_TurOp_Oper_Check = {
  DataAttributeModelType,
  "Check",
  (ModelNode*) &iedModel_WTG_WTUR1_TurOp_Oper,
  NULL,
  NULL,
  0,
  IEC61850_FC_CO,
  IEC61850_CHECK,
  0,
  NULL,
  0
};