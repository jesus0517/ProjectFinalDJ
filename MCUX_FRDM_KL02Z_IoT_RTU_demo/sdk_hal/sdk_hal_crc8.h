/*! @file : sdk_hal_crc8.h
 * @author  Ernesto Andres Rincon Cruz
 * @version 1.0.0
 * @date    14/02/2021
 * @brief   Driver para generar CRC 8 bits
 * @details
 *
 */
#ifndef SDK_HAL_CRC8_H_
#define SDK_HAL_CRC8_H_
/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "fsl_common.h"

/*!
 * @addtogroup HAL
 * @{
 */
/*!
 * @addtogroup CRC8
 * @{
 */
/*******************************************************************************
 * Public Definitions
 ******************************************************************************/

/*******************************************************************************
 * External vars
 ******************************************************************************/

/*******************************************************************************
 * Public vars
 ******************************************************************************/

/*******************************************************************************
 * Public Prototypes
 ******************************************************************************/
/*!
 * @brief Calcula polinomio de tipo Cyclic Redundancy Check de 8 bits (CRC8)
 * @param data	apuntador de memoria a datos para calcular polinomio
 * @param len	cantidad de datos requeridos para calcular
 * @return	resultado del calculo polinomio
 * @endcode
 */
uint8_t crc8Calcular(uint8_t *data, size_t len);

/** @} */ // end of CRC8 group
/** @} */ // end of HAL group

#endif /* SDK_HAL_CRC8_H_ */
