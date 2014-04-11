#include "r_macrodriver.h"
#include "r_dali_enumerate.h"
#include "r_user.h"
#include "r_dali.h"
#include "r_dali_analyze.h"
#include "r_dali_timer.h"
#include "r_dali_slave.h"
#define DALI_ENUM_TEST
static uint8_t state;
static uint8_t search_substate;
static uint8_t address;
static uint8_t enumeration_required = 1;
static uint8_t dali_resend_command = 0;
static uint8_t search_h = 0xff;
static uint8_t search_m = 0xff;
static uint8_t search_l = 0xff;
static uint8_t response;
#ifdef DALI_ENUM_TEST
static uint16_t test_withdraw = NO;
#endif
static void DALI_4ms_timeout();
static void DALI_8ms_timeout();
static void DALI_10ms_timeout();
static void DALI_50ms_timeout();
static void DALI_100ms_timeout();

/******************************************************************************
 * Function Name : DALI_Enumerate
 * Description : DALI Enumerate.
 * Argument : none
 * Return Value : none
 ******************************************************************************/
void DALI_Enumerate(uint8_t address)
{
	enumeration_required = 1;
	response = NA;
	DALI_RegisterTimer(MS_4, DALI_4ms_timeout);
	DALI_RegisterTimer(MS_8, DALI_8ms_timeout);
	DALI_RegisterTimer(MS_10, DALI_10ms_timeout);
	DALI_RegisterTimer(MS_50, DALI_50ms_timeout);
	DALI_RegisterTimer(MS_100, DALI_100ms_timeout);
	DALI_StartTimer(MS_4);
}

/******************************************************************************
 * Function Name : DALI_search_address
 * Description : DALI guess search address. Need to find search address. 
 * Argument : none
 * Return Value : none
 ******************************************************************************/
static uint8_t DALI_search_address(uint8_t response, uint8_t *guess )
{
	static uint8_t high;
	static uint8_t last;
	static uint8_t low;
	uint8_t ret = NO;
	if(response == YES){
		if(high - last == 1)
		{
			ret = YES;
			*guess = last;
			high = 0xFF;
			last = 0xFF;
			low = 0x00;
			return ret;
		}
		*guess = (uint8_t)(((uint16_t)last + (uint16_t)low) >> 1);
		high = last;
		last = *guess;
	}
	else if(response == NO){
		*guess = (uint8_t)((uint16_t)high + (uint16_t)last) >> 1;
		low = last;
		last = *guess;
	}
	else if(response == NA){/*first time, send 0xFF*/
		high = 0xFF;
		*guess = last = 0xFF;
		low = 0x0;
	}
	else{
	}
	return ret;
}


/******************************************************************************
 * Function Name : DALI_search_communication
 * Description : DALI guess search address. Need to find search address. 
 * Argument : none
 * Return Value : none
 ******************************************************************************/
static void DALI_search_communication()
{
	uint8_t guess_address;
	if(((search_substate == SUB_SEARCH_H) && (state == SEARCH_H))
			|| ((search_substate == SUB_SEARCH_M) && (state == SEARCH_M))
			|| ((search_substate == SUB_SEARCH_L) && (state == SEARCH_L)))
		if(DALI_search_address(response, &guess_address) == YES){
			//Address guessed
			state++;
			DALI_StartTimer(MS_4);
			return;
		}
	switch( search_substate ){
		case SUB_SEARCH_H:
			if(state == SEARCH_H){
				search_h = guess_address;
			}
			DALI_SendCommand((EXCOMMAND_SEARCHADDRH << 8) & search_h);
			search_substate = SUB_SEARCH_M;
			DALI_StartTimer(MS_4);
			break;
		case SUB_SEARCH_M:
			if(state == SEARCH_M){
				search_m = guess_address;
			}
			DALI_SendCommand((EXCOMMAND_SEARCHADDRM << 8) & search_m);
			search_substate = SUB_SEARCH_L;
			DALI_StartTimer(MS_4);
			break;
		case SUB_SEARCH_L:
			if(state == SEARCH_L){
				search_l = guess_address;
			}
			DALI_SendCommand((EXCOMMAND_SEARCHADDRL << 8) & search_l);
			search_substate = SUB_COMPARE;
			DALI_StartTimer(MS_4);
			break;
		case SUB_COMPARE:
			DALI_SendCommand((EXCOMMAND_COMPARE << 8) & 0x0);
			search_substate = SUB_RESPONSE;
			DALI_StartTimer(MS_8);
			DALI_StartTimer(MS_10);
			break;

		default:
			break;
	}

}
/******************************************************************************
 * Function Name : DALI_4ms_timeout
 * Description : Actions when 4ms timer timeout
 * Argument : none
 * Return Value : none
 ******************************************************************************/
static void DALI_4ms_timeout()
{
	uint8_t guess_address;
	struct dali_slave slave;
	switch(state){
		case INITIALIZE:
			DALI_SendCommand((EXCOMMAND_INITIALIZE << 8) | address);
			dali_resend_command = 1;
			DALI_StartTimer(MS_50);
			DALI_StartTimer(MIN_15);
			break;
		case RANDOMIZE:
			DALI_SendCommand((EXCOMMAND_RANDOMISE << 8) & 0xFF00);
			dali_resend_command = 1;
			DALI_StartTimer(MS_50);
			break;
		case SEARCH_H:
			DALI_search_communication();
			break;
		case SEARCH_M:
			DALI_search_communication();
			break;
		case SEARCH_L:
			DALI_search_communication();
			break;
		case ADDRESS:
			DALI_SendCommand((EXCOMMAND_PROGRAM_SHORT_ADDRESS << 8) | address);
			state = VERIFY;
			DALI_StartTimer(MS_4);
			break;
		case VERIFY:
			DALI_SendCommand((EXCOMMAND_VERIFY_SHORT_ADDRESS << 8) | address);
			DALI_StartTimer(MS_8);
			DALI_StartTimer(MS_10);
			break;
		case WITHDRAW:
			DALI_SendCommand((EXCOMMAND_PROGRAM_SHORT_ADDRESS << 8) | address);
			response = NA;
			address = DALI_get_new_slave_address();
			slave.address = address;
			slave.search_h = search_h;
			slave.search_m = search_m;
			slave.search_l = search_l;
			DALI_set_slave_config(slave);
			state = SEARCH_H;
			search_substate = SUB_SEARCH_H;
			search_h = 0xFF;
			search_m = 0xFF;
			search_l = 0xFF;
#ifdef DALI_ENUM_TEST
			test_withdraw = YES;
#endif
			DALI_StartTimer(MS_4);
			break;
		case TERMINATE:
			DALI_SendCommand((EXCOMMAND_TERMINATE << 8) | address);
			enumeration_required = 0;
			state = RANDOMIZE;
			break;
		default:
			break;
	}
}


/******************************************************************************
 * Function Name : DALI_8ms_timeout
 * Description : Actions when 8ms timer timeout. This is made for testing
 * Argument : none
 * Return Value : none
 ******************************************************************************/
static void DALI_8ms_timeout()
{
#ifdef DALI_ENUM_TEST
	static slave_test_address = 124;
	uint8_t search_address;
	switch( state ){
		case SEARCH_H:
			search_address = search_h;
			break;
		case SEARCH_M:
			search_address = search_m;
			break;
		case SEARCH_L:
			search_address = search_l;
			break;
		case VERIFY:
			break;
		default:
			break;
	}
#endif

	switch( state ){
		case SEARCH_H:
		case SEARCH_M:
		case SEARCH_L:
			switch( search_substate ){

				case SUB_RESPONSE:
#ifdef DALI_ENUM_TEST
					if((search_address >= slave_test_address) && (test_withdraw == NO)){
						DALI_TestReceive( YES );
#endif
						break;
					}
				default:
					break;
			}
			break;
		case VERIFY:
#ifdef DALI_ENUM_TEST
			if(test_withdraw == NO)
				DALI_TestReceive( YES );
#endif
			//			DALI_StartTimer(MS_4);
			break;
		default:
			break;
	}
}
/******************************************************************************
 * Function Name : DALI_10ms_timeout
 * Description : Actions when 10ms timer timeout. By this time all responses
 * from slaves should be received
 * Argument : none
 * Return Value : none
 ******************************************************************************/
static void DALI_10ms_timeout()
{
	switch(state){
		case SEARCH_H:
		case SEARCH_M:
		case SEARCH_L:
			switch(search_substate){
				case SUB_RESPONSE:
					if(response != NA)
						response = NO;//Initialize response
					while(DALI_ReadData(&response)){
						if(DALI_AnalyzeResponse(response, SPECIAL_COMMAND ) == YES)
							response = YES;
					}
					if(response == NA)
						state = TERMINATE;
					search_substate = SUB_SEARCH_H;
					DALI_StartTimer(MS_4);
					break;

				default:
					break;
			}
			break;
		case VERIFY:
			while(DALI_ReadData(&response)){
				if(DALI_AnalyzeResponse(response, SPECIAL_COMMAND ) == YES)
					response = YES;
			}
			if(response == YES)
				state = WITHDRAW;
			else
				state = TERMINATE;
			DALI_StartTimer(MS_4);
		default:
			break;
	}
}

/******************************************************************************
 * Function Name : DALI_50ms_timeout
 * Description : Actions when 50ms timer timeout
 * Argument : none
 * Return Value : none
 ******************************************************************************/
static void DALI_50ms_timeout()
{
	switch( state ){
		case INITIALIZE:
			if(dali_resend_command){
				DALI_SendCommand((EXCOMMAND_INITIALIZE << 8) | address);
				dali_resend_command = 0;
				state = RANDOMIZE;
				DALI_StartTimer(MS_4);
			}
			break;
		case RANDOMIZE:
			if(dali_resend_command){
				DALI_SendCommand((EXCOMMAND_RANDOMISE << 8) & 0xFF00);
				dali_resend_command = 0;
				DALI_StartTimer(MS_100);
			}
			break;
		default:
			break;
	}
}

/******************************************************************************
 * Function Name : DALI_100ms_timeout
 * Description : Actions when 100ms timer timeout
 * Argument : none
 * Return Value : none
 ******************************************************************************/
static void DALI_100ms_timeout()
{
	switch( state ){
		case RANDOMIZE:
			state = SEARCH_H;
			search_substate = SUB_SEARCH_H;
			DALI_StartTimer(MS_4);
			break;
		default:
			break;
	}
}