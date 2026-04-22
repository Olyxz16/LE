/******************** (C) COPYRIGHT 2019 STMicroelectronics ********************
 * File Name          : gatt_db.h
 * Author             : SRA
 * Version            : V1.0.0
 * Date               : Oct-2019
 * Description        : Header file for gatt_db.c
 *******************************************************************************
 * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
 * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
 * TIME.
 * AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
 * INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
 * CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
 * INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
 ******************************************************************************/

#ifndef GATT_DB_H
#define GATT_DB_H

/* Includes ------------------------------------------------------------------*/
#include "hci.h"

/* Exported defines ----------------------------------------------------------*/

/**
 * @brief Number of application services
 */
#define NUMBER_OF_APPLICATION_SERVICES (1)

/* Exported function prototypes ----------------------------------------------*/
tBleStatus Add_HWServW2ST_Service(void);
void Read_Request_CB(uint16_t handle);
tBleStatus Environmental_Update(int32_t press, int16_t temp,uint16_t hum);

#endif /* GATT_DB_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/