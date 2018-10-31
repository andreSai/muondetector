#ifndef UBX_MSG_KEY_NAME_MAP_H
#define UBX_MSG_KEY_NAME_MAP_H
#include <muondetector_shared_global.h>
#include <QString>
#include <ublox_messages.h>
#include <QMap>

const QMap<short unsigned int , QString> ubxMsgKeyNameMap({
														 {MSG_ACK,"ACK-ACK"},
														 {MSG_NAK,"ACK-NAK"},
														 // NAV
														 {MSG_NAV_CLOCK,"NAV-CLOCK"},
														 {MSG_NAV_DGPS,"NAV-DGPS"},
														 {MSG_NAV_AOPSTATUS,"NAV-AOPSTATUS"},
														 {MSG_NAV_DOP,"NAV-DOP"},
														 {MSG_NAV_POSECEF,"NAV-POSECEF"},
														 {MSG_NAV_POSLLH,"NAV-POSLLH"},
														 {MSG_NAV_PVT,"NAV-PVT"},
														 {MSG_NAV_SBAS,"NAV-SBAS"},
														 {MSG_NAV_SOL,"NAV-SOL"},
														 {MSG_NAV_STATUS,"NAV-STATUS"},
														 {MSG_NAV_SVINFO,"NAV-SVINFO"},
														 {MSG_NAV_TIMEGPS,"NAV-TIMEGPS"},
														 {MSG_NAV_TIMEUTC,"NAV-TIMEUTC"},
														 {MSG_NAV_VELECEF,"NAV-VELECEF"},
														 {MSG_NAV_VELNED,"NAV-VELNED"},
														 // CFG
														 {MSG_CFG_ANT,"CFG-ANT"},
														 {MSG_CFG_CFG,"CFG-CFG"},
														 {MSG_CFG_DAT,"CFG-DAT"},
														 {MSG_CFG_GNSS,"CFG-GNSS"},
														 {MSG_CFG_INF,"CFG-INF"},
														 {MSG_CFG_ITFM,"CFG-ITFM"},
														 {MSG_CFG_LOGFILTER,"CFG-LOGFILTER"},
														 {MSG_CFG_MSG,"CFG-MSG"},
														 {MSG_CFG_NAV5,"CFG-NAV5"},
														 {MSG_CFG_NAVX5,"CFG-NAV5X"},
														 {MSG_CFG_NMEA,"CFG-NMEA"},
														 {MSG_CFG_PM2,"CFG-PM2"},
														 {MSG_CFG_PRT,"CFG-PRT"},
														 {MSG_CFG_RATE,"CFG-RATE"},
														 {MSG_CFG_RINV,"CFG-RINV"},
														 {MSG_CFG_RST,"CFG-RST"},
														 {MSG_CFG_RXM,"CFG-RXM"},
														 {MSG_CFG_SBAS,"CFG-SBAS"},
														 {MSG_CFG_TP5,"CFG-TP5"},
														 {MSG_CFG_USB,"CFG-USB"},
														 // TIM
														 {MSG_TIM_TP,"TIM-TP"},
														 {MSG_TIM_TM2,"TIM-TM2"},
														 {MSG_TIM_VRFY,"TIM-VRFY"},
														 // MON
														 {MSG_MON_VER,"MON-VER"},
														 {MSG_MON_HW,"MON-HW"},
														 {MSG_MON_HW2,"MON-HW2"},
														 {MSG_MON_IO,"MON-IO"},
														 {MSG_MON_MSGPP,"MON-MSGPP"},
														 {MSG_MON_RXBUF,"MON-RXBUF"},
														 {MSG_MON_RXR,"MON-RXR"},
														 {MSG_MON_TXBUF,"MON-TXBUF"},

														 // the messages only used in Ublox-8
														 {MSG_NAV_EOE,"NAV-EOE"},
														 {MSG_NAV_GEOFENCE,"NAV-GEOFENCE"},
														 {MSG_NAV_ODO,"NAV-ODO"},
														 {MSG_NAV_ORB,"NAV-ORB"},
														 {MSG_NAV_RESETODO,"NAV-RESETODO"},
														 {MSG_NAV_SAT,"NAV-SAT"},
														 {MSG_NAV_TIMEBDS,"NAV-TIMEBDS"},
														 {MSG_NAV_TIMEGAL,"NAV-TIMEGAL"},
														 {MSG_NAV_TIMEGLO,"NAV-TIMEGLO"},
														 {MSG_NAV_TIMELS,"NAV-TIMELS"},

														 // NMEA
														 {MSG_NMEA_DTM,"NMEA-DTM"},
														 {MSG_NMEA_GBQ,"NMEA-GBQ"},
														 {MSG_NMEA_GBS,"NMEA-GBS"},
														 {MSG_NMEA_GGA,"NMEA-GGA"},
														 {MSG_NMEA_GLL,"NMEA-GLL"},
														 {MSG_NMEA_GLQ,"NMEA-GLQ"},
														 {MSG_NMEA_GNQ,"NMEA-GNQ"},
														 {MSG_NMEA_GNS,"NMEA-GNS"},
														 {MSG_NMEA_GPQ,"NMEA-GPQ"},
														 {MSG_NMEA_GRS,"NMEA-GRS"},
														 {MSG_NMEA_GSA,"NMEA-GSA"},
														 {MSG_NMEA_GST,"NMEA-GST"},
														 {MSG_NMEA_GSV,"NMEA-GSV"},
														 {MSG_NMEA_RMC,"NMEA-RMC"},
														 {MSG_NMEA_TXT,"NMEA-TXT"},
														 {MSG_NMEA_VLW,"NMEA-VLW"},
														 {MSG_NMEA_VTG,"NMEA-VTG"},
														 {MSG_NMEA_ZDA,"NMEA-ZDA"},
														 {MSG_NMEA_CONFIG,"NMEA-CONFIG"},
														 {MSG_NMEA_POSITION,"NMEA-POSITION"},
														 {MSG_NMEA_RATE,"NMEA-RATE"},
														 {MSG_NMEA_SVSTATUS,"NMEA-SVSTATUS"},
														 {MSG_NMEA_TIME,"NMEA-TIME"}
	});
#endif // UBX_MSG_KEY_NAME_MAP_H
