#include "ied.h"
#include "lphd.h"
#include "wtur.h"

LogicalNode iedModel_WTG_LPHD1 = {
  LogicalNodeModelType,
  "LPHD1",
  (ModelNode*) &iedModel_WTG,
  (ModelNode*) &iedModel_WTG_WTUR1,
  (ModelNode*) &iedModel_WTG_LPHD1_NamPlt,
};

DataObject iedModel_WTG_LPHD1_NamPlt = {
  DataObjectModelType,
  "NamPlt",
  (ModelNode*) &iedModel_WTG_LPHD1,
  (ModelNode*) &iedModel_WTG_LPHD1_PhyNam,
  (ModelNode*) &iedModel_WTG_LPHD1_NamPlt_vendor,
  0
};

DataAttribute iedModel_WTG_LPHD1_NamPlt_vendor = {
  DataAttributeModelType,
  "vendor",
  (ModelNode*) &iedModel_WTG_LPHD1_NamPlt,
  (ModelNode*) &iedModel_WTG_LPHD1_NamPlt_swRev,
  NULL,
  0,
  IEC61850_FC_DC,
  IEC61850_VISIBLE_STRING_255,
  0,
  NULL,
  0
};

DataAttribute iedModel_WTG_LPHD1_NamPlt_swRev = {
  DataAttributeModelType,
  "swRev",
  (ModelNode*) &iedModel_WTG_LPHD1_NamPlt,
  (ModelNode*) &iedModel_WTG_LPHD1_NamPlt_configRev,
  NULL,
  0,
  IEC61850_FC_DC,
  IEC61850_VISIBLE_STRING_255,
  0,
  NULL,
  0
};

DataAttribute iedModel_WTG_LPHD1_NamPlt_configRev = {
  DataAttributeModelType,
  "configRev",
  (ModelNode*) &iedModel_WTG_LPHD1_NamPlt,
  NULL,
  NULL,
  0,
  IEC61850_FC_DC,
  IEC61850_VISIBLE_STRING_255,
  0,
  NULL,
  0
};

DataObject iedModel_WTG_LPHD1_PhyNam = {
  DataObjectModelType,
  "PhyNam",
  (ModelNode*) &iedModel_WTG_LPHD1,
  (ModelNode*) &iedModel_WTG_LPHD1_PhyHealth,
  (ModelNode*) &iedModel_WTG_LPHD1_PhyNam_vendor,
  0
};

DataAttribute iedModel_WTG_LPHD1_PhyNam_vendor = {
  DataAttributeModelType,
  "vendor",
  (ModelNode*) &iedModel_WTG_LPHD1_PhyNam,
  NULL,
  NULL,
  0,
  IEC61850_FC_DC,
  IEC61850_VISIBLE_STRING_255,
  0,
  NULL,
  0
};

DataObject iedModel_WTG_LPHD1_PhyHealth = {
  DataObjectModelType,
  "PhyHealth",
  (ModelNode*) &iedModel_WTG_LPHD1,
  (ModelNode*) &iedModel_WTG_LPHD1_Proxy,
  (ModelNode*) &iedModel_WTG_LPHD1_PhyHealth_stVal,
  0
};

DataAttribute iedModel_WTG_LPHD1_PhyHealth_stVal = {
  DataAttributeModelType,
  "stVal",
  (ModelNode*) &iedModel_WTG_LPHD1_PhyHealth,
  (ModelNode*) &iedModel_WTG_LPHD1_PhyHealth_q,
  NULL,
  0,
  IEC61850_FC_ST,
  IEC61850_INT32,
  0 + TRG_OPT_DATA_CHANGED,
  NULL,
  0
};

DataAttribute iedModel_WTG_LPHD1_PhyHealth_q = {
  DataAttributeModelType,
  "q",
  (ModelNode*) &iedModel_WTG_LPHD1_PhyHealth,
  (ModelNode*) &iedModel_WTG_LPHD1_PhyHealth_t,
  NULL,
  0,
  IEC61850_FC_ST,
  IEC61850_QUALITY,
  0 + TRG_OPT_QUALITY_CHANGED,
  NULL,
  0
};

DataAttribute iedModel_WTG_LPHD1_PhyHealth_t = {
  DataAttributeModelType,
  "t",
  (ModelNode*) &iedModel_WTG_LPHD1_PhyHealth,
  NULL,
  NULL,
  0,
  IEC61850_FC_ST,
  IEC61850_TIMESTAMP,
  0,
  NULL,
  0
};

DataObject iedModel_WTG_LPHD1_Proxy = {
  DataObjectModelType,
  "Proxy",
  (ModelNode*) &iedModel_WTG_LPHD1,
  NULL,
  (ModelNode*) &iedModel_WTG_LPHD1_Proxy_stVal,
  0
};

DataAttribute iedModel_WTG_LPHD1_Proxy_stVal = {
  DataAttributeModelType,
  "stVal",
  (ModelNode*) &iedModel_WTG_LPHD1_Proxy,
  (ModelNode*) &iedModel_WTG_LPHD1_Proxy_q,
  NULL,
  0,
  IEC61850_FC_ST,
  IEC61850_BOOLEAN,
  0 + TRG_OPT_DATA_CHANGED,
  NULL,
  0
};

DataAttribute iedModel_WTG_LPHD1_Proxy_q = {
  DataAttributeModelType,
  "q",
  (ModelNode*) &iedModel_WTG_LPHD1_Proxy,
  (ModelNode*) &iedModel_WTG_LPHD1_Proxy_t,
  NULL,
  0,
  IEC61850_FC_ST,
  IEC61850_QUALITY,
  0 + TRG_OPT_QUALITY_CHANGED,
  NULL,
  0
};

DataAttribute iedModel_WTG_LPHD1_Proxy_t = {
  DataAttributeModelType,
  "t",
  (ModelNode*) &iedModel_WTG_LPHD1_Proxy,
  NULL,
  NULL,
  0,
  IEC61850_FC_ST,
  IEC61850_TIMESTAMP,
  0,
  NULL,
  0
};
