/*! @file : sdk_pph_mma8460.c
 * @author  Ernesto Andres Rincon Cruz
 * @version 1.0.0
 * @date    17/01/2021
 * @brief   Driver para MMA8451Q
 * @details
 *
*/
/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "sdk_pph_mma8451Q.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/


/*******************************************************************************
 * Private Prototypes
 ******************************************************************************/


/*******************************************************************************
 * External vars
 ******************************************************************************/


/*******************************************************************************
 * Local vars
 ******************************************************************************/


/*******************************************************************************
 * Private Source Code
 ******************************************************************************/


/*******************************************************************************
 * Public Source Code
 ******************************************************************************/
status_t mma8451QWhoAmI(void) {
	status_t status;
	uint8_t i2c_data;

	status = i2c0MasterReadByte(&i2c_data, 1, MMA8451_ADDRESS, REG_WHO_AM_I);
	if (status == kStatus_Success) {
		if (i2c_data == 0x1A)
			return (kStatus_Success);
		else
			return (kStatus_Fail);
	} else {
		return (status);
	}
}

status_t	mma8451QReadAccel(mma8451_data_t *data ){
	status_t status;
	uint8_t i2c_data[7];

	status = i2c0MasterReadByte(&i2c_data[0], 7, MMA8451_ADDRESS, REG_STATUS);
	if (status == kStatus_Success) {
		data->status=i2c_data[REG_STATUS];
		data->x_value=(((uint16_t)(i2c_data[REG_OUT_X_MSB])<<8)|(uint16_t)(i2c_data[REG_OUT_X_LSB]));
		data->y_value=(((uint16_t)(i2c_data[REG_OUT_Y_MSB])<<8)|(uint16_t)(i2c_data[REG_OUT_Y_LSB]));
		data->z_value=(((uint16_t)(i2c_data[REG_OUT_Z_MSB])<<8)|(uint16_t)(i2c_data[REG_OUT_Z_LSB]));
		return (kStatus_Success);
	}else{
		return (status);
	}
}

status_t mma8451QInit(void){
	status_t status;
	uint8_t i2c_data;

    /*  write 0000 0000 = 0x00 to accelerometer control register 1 */
    /*  standby */
    /*  [7-1] = 0000 000 */
    /*  [0]: active=0 */
	i2c_data = 0x00;
	status = i2c0MasterWriteByte(&i2c_data, 1, MMA8451_ADDRESS, REG_CTRL_REG1);

	if(status!=kStatus_Success)
		return(status);

    /*  write 0000 0001= 0x01 to XYZ_DATA_CFG register */
    /*  [7]: reserved */
    /*  [6]: reserved */
    /*  [5]: reserved */
    /*  [4]: hpf_out=0 */
    /*  [3]: reserved */
    /*  [2]: reserved */
    /*  [1-0]: fs=01 for accelerometer range of +/-4g range with 0.488mg/LSB */
    /*  databyte = 0x01; */
	i2c_data = 0x01;
	status = i2c0MasterWriteByte(&i2c_data, 1, MMA8451_ADDRESS, REG_XYZ_DATA_CFG);

	if(status!=kStatus_Success)
		return(status);

    /*  write 0000 1101 = 0x0D to accelerometer control register 1 */
    /*  [7-6]: aslp_rate=00 */
    /*  [5-3]: dr=001 for 200Hz data rate (when in hybrid mode) */
    /*  [2]: lnoise=1 for low noise mode */
    /*  [1]: f_read=0 for normal 16 bit reads */
    /*  [0]: active=1 to take the part out of standby and enable sampling */
    /*   databyte = 0x0D; */
	i2c_data = 0x0D;
	status = i2c0MasterWriteByte(&i2c_data, 1, MMA8451_ADDRESS, REG_CTRL_REG1);

	if(status!=kStatus_Success)
		return(status);

	return(kStatus_Success);
}


