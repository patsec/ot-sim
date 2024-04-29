#ifndef LPHD_H
#define LPHD_H

#include <stdlib.h>
#include <libiec61850/iec61850_model.h>

extern LogicalNode   iedModel_WTG_LPHD1;
extern DataObject    iedModel_WTG_LPHD1_NamPlt;
extern DataAttribute iedModel_WTG_LPHD1_NamPlt_vendor;
extern DataAttribute iedModel_WTG_LPHD1_NamPlt_swRev;
extern DataAttribute iedModel_WTG_LPHD1_NamPlt_configRev;
extern DataObject    iedModel_WTG_LPHD1_PhyNam;
extern DataAttribute iedModel_WTG_LPHD1_PhyNam_vendor;
extern DataObject    iedModel_WTG_LPHD1_PhyHealth;
extern DataAttribute iedModel_WTG_LPHD1_PhyHealth_stVal;
extern DataAttribute iedModel_WTG_LPHD1_PhyHealth_q;
extern DataAttribute iedModel_WTG_LPHD1_PhyHealth_t;
extern DataObject    iedModel_WTG_LPHD1_Proxy;
extern DataAttribute iedModel_WTG_LPHD1_Proxy_stVal;
extern DataAttribute iedModel_WTG_LPHD1_Proxy_q;
extern DataAttribute iedModel_WTG_LPHD1_Proxy_t;

#endif // LPHD_H