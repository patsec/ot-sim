#include "ied.h"
#include "lln0.h"

static void initializeValues() {}

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