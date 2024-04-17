#include "ied.h"
#include "lln0.h"
#include "lphd.h"

LogicalNode iedModel_WTG_LLN0 = {
  LogicalNodeModelType,
  "LLN0",
  (ModelNode*) &iedModel_WTG,
  (ModelNode*) &iedModel_WTG_LPHD1,
  (ModelNode*) &iedModel_WTG_LLN0_Mod,
};

DataObject iedModel_WTG_LLN0_Mod = {
  DataObjectModelType,
  "Mod",
  (ModelNode*) &iedModel_WTG_LLN0,
  (ModelNode*) &iedModel_WTG_LLN0_Beh,
  (ModelNode*) &iedModel_WTG_LLN0_Mod_Oper,
  0
};

DataAttribute iedModel_WTG_LLN0_Mod_Oper = {
  DataAttributeModelType,
  "Oper",
  (ModelNode*) &iedModel_WTG_LLN0_Mod,
  (ModelNode*) &iedModel_WTG_LLN0_Mod_stVal,
  (ModelNode*) &iedModel_WTG_LLN0_Mod_Oper_ctlVal,
  0,
  IEC61850_FC_CO,
  IEC61850_CONSTRUCTED,
  0,
  NULL,
  0
};

DataAttribute iedModel_WTG_LLN0_Mod_Oper_ctlVal = {
  DataAttributeModelType,
  "ctlVal",
  (ModelNode*) &iedModel_WTG_LLN0_Mod_Oper,
  (ModelNode*) &iedModel_WTG_LLN0_Mod_Oper_operTm,
  NULL,
  0,
  IEC61850_FC_CO,
  IEC61850_INT32,
  0,
  NULL,
  0
};

DataAttribute iedModel_WTG_LLN0_Mod_Oper_operTm = {
  DataAttributeModelType,
  "operTm",
  (ModelNode*) &iedModel_WTG_LLN0_Mod_Oper,
  (ModelNode*) &iedModel_WTG_LLN0_Mod_Oper_origin,
  NULL,
  0,
  IEC61850_FC_CO,
  IEC61850_TIMESTAMP,
  0,
  NULL,
  0
};

DataAttribute iedModel_WTG_LLN0_Mod_Oper_origin = {
  DataAttributeModelType,
  "origin",
  (ModelNode*) &iedModel_WTG_LLN0_Mod_Oper,
  (ModelNode*) &iedModel_WTG_LLN0_Mod_Oper_ctlNum,
  (ModelNode*) &iedModel_WTG_LLN0_Mod_Oper_origin_orCat,
  0,
  IEC61850_FC_CO,
  IEC61850_CONSTRUCTED,
  0,
  NULL,
  0
};

DataAttribute iedModel_WTG_LLN0_Mod_Oper_origin_orCat = {
  DataAttributeModelType,
  "orCat",
  (ModelNode*) &iedModel_WTG_LLN0_Mod_Oper_origin,
  (ModelNode*) &iedModel_WTG_LLN0_Mod_Oper_origin_orIdent,
  NULL,
  0,
  IEC61850_FC_CO,
  IEC61850_ENUMERATED,
  0,
  NULL,
  0
};

DataAttribute iedModel_WTG_LLN0_Mod_Oper_origin_orIdent = {
  DataAttributeModelType,
  "orIdent",
  (ModelNode*) &iedModel_WTG_LLN0_Mod_Oper_origin,
  NULL,
  NULL,
  0,
  IEC61850_FC_CO,
  IEC61850_OCTET_STRING_64,
  0,
  NULL,
  0
};

DataAttribute iedModel_WTG_LLN0_Mod_Oper_ctlNum = {
  DataAttributeModelType,
  "ctlNum",
  (ModelNode*) &iedModel_WTG_LLN0_Mod_Oper,
  (ModelNode*) &iedModel_WTG_LLN0_Mod_Oper_T,
  NULL,
  0,
  IEC61850_FC_CO,
  IEC61850_INT8U,
  0,
  NULL,
  0
};

DataAttribute iedModel_WTG_LLN0_Mod_Oper_T = {
  DataAttributeModelType,
  "T",
  (ModelNode*) &iedModel_WTG_LLN0_Mod_Oper,
  (ModelNode*) &iedModel_WTG_LLN0_Mod_Oper_Test,
  NULL,
  0,
  IEC61850_FC_CO,
  IEC61850_TIMESTAMP,
  0,
  NULL,
  0
};

DataAttribute iedModel_WTG_LLN0_Mod_Oper_Test = {
  DataAttributeModelType,
  "Test",
  (ModelNode*) &iedModel_WTG_LLN0_Mod_Oper,
  (ModelNode*) &iedModel_WTG_LLN0_Mod_Oper_Check,
  NULL,
  0,
  IEC61850_FC_CO,
  IEC61850_BOOLEAN,
  0,
  NULL,
  0
};

DataAttribute iedModel_WTG_LLN0_Mod_Oper_Check = {
  DataAttributeModelType,
  "Check",
  (ModelNode*) &iedModel_WTG_LLN0_Mod_Oper,
  NULL,
  NULL,
  0,
  IEC61850_FC_CO,
  IEC61850_CHECK,
  0,
  NULL,
  0
};

DataAttribute iedModel_WTG_LLN0_Mod_stVal = {
  DataAttributeModelType,
  "stVal",
  (ModelNode*) &iedModel_WTG_LLN0_Mod,
  (ModelNode*) &iedModel_WTG_LLN0_Mod_q,
  NULL,
  0,
  IEC61850_FC_ST,
  IEC61850_INT32,
  0 + TRG_OPT_DATA_CHANGED,
  NULL,
  0
};

DataAttribute iedModel_WTG_LLN0_Mod_q = {
  DataAttributeModelType,
  "q",
  (ModelNode*) &iedModel_WTG_LLN0_Mod,
  (ModelNode*) &iedModel_WTG_LLN0_Mod_t,
  NULL,
  0,
  IEC61850_FC_ST,
  IEC61850_QUALITY,
  0 + TRG_OPT_QUALITY_CHANGED,
  NULL,
  0
};

DataAttribute iedModel_WTG_LLN0_Mod_t = {
  DataAttributeModelType,
  "t",
  (ModelNode*) &iedModel_WTG_LLN0_Mod,
  (ModelNode*) &iedModel_WTG_LLN0_Mod_ctlModel,
  NULL,
  0,
  IEC61850_FC_ST,
  IEC61850_TIMESTAMP,
  0,
  NULL,
  0
};

DataAttribute iedModel_WTG_LLN0_Mod_ctlModel = {
  DataAttributeModelType,
  "ctlModel",
  (ModelNode*) &iedModel_WTG_LLN0_Mod,
  NULL,
  NULL,
  0,
  IEC61850_FC_CF,
  IEC61850_ENUMERATED,
  0,
  NULL,
  0
};

DataObject iedModel_WTG_LLN0_Beh = {
  DataObjectModelType,
  "Beh",
  (ModelNode*) &iedModel_WTG_LLN0,
  (ModelNode*) &iedModel_WTG_LLN0_Health,
  (ModelNode*) &iedModel_WTG_LLN0_Beh_stVal,
  0
};

DataAttribute iedModel_WTG_LLN0_Beh_stVal = {
  DataAttributeModelType,
  "stVal",
  (ModelNode*) &iedModel_WTG_LLN0_Beh,
  (ModelNode*) &iedModel_WTG_LLN0_Beh_q,
  NULL,
  0,
  IEC61850_FC_ST,
  IEC61850_INT32,
  0 + TRG_OPT_DATA_CHANGED,
  NULL,
  0
};

DataAttribute iedModel_WTG_LLN0_Beh_q = {
  DataAttributeModelType,
  "q",
  (ModelNode*) &iedModel_WTG_LLN0_Beh,
  (ModelNode*) &iedModel_WTG_LLN0_Beh_t,
  NULL,
  0,
  IEC61850_FC_ST,
  IEC61850_QUALITY,
  0 + TRG_OPT_QUALITY_CHANGED,
  NULL,
  0
};

DataAttribute iedModel_WTG_LLN0_Beh_t = {
  DataAttributeModelType,
  "t",
  (ModelNode*) &iedModel_WTG_LLN0_Beh,
  NULL,
  NULL,
  0,
  IEC61850_FC_ST,
  IEC61850_TIMESTAMP,
  0,
  NULL,
  0
};

DataObject iedModel_WTG_LLN0_Health = {
  DataObjectModelType,
  "Health",
  (ModelNode*) &iedModel_WTG_LLN0,
  (ModelNode*) &iedModel_WTG_LLN0_NamPlt,
  (ModelNode*) &iedModel_WTG_LLN0_Health_stVal,
  0
};

DataAttribute iedModel_WTG_LLN0_Health_stVal = {
  DataAttributeModelType,
  "stVal",
  (ModelNode*) &iedModel_WTG_LLN0_Health,
  (ModelNode*) &iedModel_WTG_LLN0_Health_q,
  NULL,
  0,
  IEC61850_FC_ST,
  IEC61850_INT32,
  0 + TRG_OPT_DATA_CHANGED,
  NULL,
  0
};

DataAttribute iedModel_WTG_LLN0_Health_q = {
  DataAttributeModelType,
  "q",
  (ModelNode*) &iedModel_WTG_LLN0_Health,
  (ModelNode*) &iedModel_WTG_LLN0_Health_t,
  NULL,
  0,
  IEC61850_FC_ST,
  IEC61850_QUALITY,
  0 + TRG_OPT_QUALITY_CHANGED,
  NULL,
  0
};

DataAttribute iedModel_WTG_LLN0_Health_t = {
  DataAttributeModelType,
  "t",
  (ModelNode*) &iedModel_WTG_LLN0_Health,
  NULL,
  NULL,
  0,
  IEC61850_FC_ST,
  IEC61850_TIMESTAMP,
  0,
  NULL,
  0
};

DataObject iedModel_WTG_LLN0_NamPlt = {
  DataObjectModelType,
  "NamPlt",
  (ModelNode*) &iedModel_WTG_LLN0,
  NULL,
  (ModelNode*) &iedModel_WTG_LLN0_NamPlt_vendor,
  0
};

DataAttribute iedModel_WTG_LLN0_NamPlt_vendor = {
  DataAttributeModelType,
  "vendor",
  (ModelNode*) &iedModel_WTG_LLN0_NamPlt,
  (ModelNode*) &iedModel_WTG_LLN0_NamPlt_swRev,
  NULL,
  0,
  IEC61850_FC_DC,
  IEC61850_VISIBLE_STRING_255,
  0,
  NULL,
  0
};

DataAttribute iedModel_WTG_LLN0_NamPlt_swRev = {
  DataAttributeModelType,
  "swRev",
  (ModelNode*) &iedModel_WTG_LLN0_NamPlt,
  (ModelNode*) &iedModel_WTG_LLN0_NamPlt_configRev,
  NULL,
  0,
  IEC61850_FC_DC,
  IEC61850_VISIBLE_STRING_255,
  0,
  NULL,
  0
};

DataAttribute iedModel_WTG_LLN0_NamPlt_configRev = {
  DataAttributeModelType,
  "configRev",
  (ModelNode*) &iedModel_WTG_LLN0_NamPlt,
  NULL,
  NULL,
  0,
  IEC61850_FC_DC,
  IEC61850_VISIBLE_STRING_255,
  0,
  NULL,
  0
};