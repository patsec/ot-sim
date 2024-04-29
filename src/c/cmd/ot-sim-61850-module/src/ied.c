#include "ied.h"
#include "lln0.h"
#include "lphd.h"

static void initializeValues() {
  iedModel_WTG_LLN0_Mod_stVal.mmsValue = MmsValue_newIntegerFromInt32(1);
  iedModel_WTG_LLN0_Mod_ctlModel.mmsValue = MmsValue_newIntegerFromInt32(0);
  iedModel_WTG_LLN0_Beh_stVal.mmsValue = MmsValue_newIntegerFromInt32(1);
  iedModel_WTG_LLN0_Health_stVal.mmsValue = MmsValue_newIntegerFromInt32(1);
  iedModel_WTG_LLN0_NamPlt_vendor.mmsValue = MmsValue_newVisibleString("OT-sim");
  iedModel_WTG_LLN0_NamPlt_swRev.mmsValue = MmsValue_newVisibleString("0.1.0");
  iedModel_WTG_LPHD1_NamPlt_vendor.mmsValue = MmsValue_newVisibleString("OT-sim");
  iedModel_WTG_LPHD1_NamPlt_swRev.mmsValue = MmsValue_newVisibleString("0.1.0");
  iedModel_WTG_LPHD1_NamPlt_configRev.mmsValue = MmsValue_newVisibleString("0.1.0");
  iedModel_WTG_LPHD1_PhyNam_vendor.mmsValue = MmsValue_newVisibleString("OT-sim");
  iedModel_WTG_LPHD1_PhyHealth_stVal.mmsValue = MmsValue_newIntegerFromInt32(1);
  iedModel_WTG_LPHD1_Proxy_stVal.mmsValue = MmsValue_newIntegerFromInt32(0);
}

IedModel iedModel = {
  "WIND",
  &iedModel_WTG,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  initializeValues
};

LogicalDevice iedModel_WTG = {
  LogicalDeviceModelType,
  "WTG",
  (ModelNode*) &iedModel,
  NULL,
  (ModelNode*) &iedModel_WTG_LLN0
};