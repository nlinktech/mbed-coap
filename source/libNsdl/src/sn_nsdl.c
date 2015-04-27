/**
 * \file sn_nsdl.c
 *
 * \brief Nano service device library
 *
 * Copyright (c) 2011 - 2014 ARM. All rights reserved.
 *
 */

#include <string.h>

#include "ns_types.h"
#include "sn_nsdl.h"
#include "sn_coap_header.h"
#include "sn_coap_protocol.h"
#include "sn_nsdl_lib.h"
#include "sn_grs.h"

/* Defines */
#define RESOURCE_DIR_LEN				2
#define EP_NAME_PARAMETERS_LEN			3
#define ET_PARAMETER_LEN				3
#define LT_PARAMETER_LEN				3
#define DOMAIN_PARAMETER_LEN			2
#define RT_PARAMETER_LEN				3
#define IF_PARAMETER_LEN				3
#define OBS_PARAMETER_LEN				3
#define AOBS_PARAMETER_LEN				8
#define COAP_CON_PARAMETER_LEN			3
#define BS_EP_PARAMETER_LEN				3
#define BS_QUEUE_MODE_PARAMATER_LEN		2

#define SN_NSDL_EP_REGISTER_MESSAGE		1
#define SN_NSDL_EP_UPDATE_MESSAGE		2

#define	SN_NSDL_MSG_REGISTER			1
#define SN_NSDL_MSG_UNREGISTER			2
#define SN_NSDL_MSG_UPDATE				3
#define SN_NSDL_MSG_EVENT				4

/* Constants */
static uint8_t	 	ep_name_parameter_string[] 	= {'e','p','='}; 		/* Endpoint name. A unique name for the registering node in a domain.  */
static uint8_t		resource_path_ptr[]			= {'r','d'};			/* For resource directory */
static uint8_t		resource_type_parameter[]	= {'r','t','='};		/* Resource type. Only once for registration */
static uint8_t		obs_parameter[]				= {'o','b','s'};		/* Observable */
//static uint8_t	aobs_parameter[]			= {'a','o','b','s',';','i','d','='};	/* Auto-observable - TBD */
static uint8_t		if_description_parameter[]	= {'i','f','='};		/* Interface description. Only once */
static uint8_t		ep_lifetime_parameter[]		= {'l','t','='};		/* Lifetime. Number of seconds that this registration will be valid for. Must be updated within this time, or will be removed. */
static uint8_t		ep_domain_parameter[]		= {'d','='};			/* Domain name. If this parameter is missing, a default domain is assumed. */
static uint8_t 		coap_con_type_parameter[]	= {'c','t','='};		/* CoAP content type */
/* * OMA BS parameters * */
static uint8_t bs_uri[] 						= {'b','s'};
static uint8_t bs_ep_name[] 					= {'e','p','='};
static uint8_t et_parameter[] 					= {'e','t','='}; 		/* Endpoint type */
static uint8_t bs_queue_mode[]					= {'b','='};

/* Function prototypes */
static uint16_t 		sn_nsdl_internal_coap_send					(struct nsdl_s *handle, sn_coap_hdr_s *coap_header_ptr, sn_nsdl_addr_s *dst_addr_ptr, uint8_t message_description);
static void				sn_nsdl_resolve_nsp_address					(struct nsdl_s *handle);
int8_t					sn_nsdl_build_registration_body				(struct nsdl_s *handle, sn_coap_hdr_s *message_ptr, uint8_t updating_registeration);
static uint16_t 		sn_nsdl_calculate_registration_body_size	(struct nsdl_s *handle, uint8_t updating_registeration);
static uint8_t 			sn_nsdl_calculate_uri_query_option_len		(sn_nsdl_ep_parameters_s *endpoint_info_ptr, uint8_t msg_type);
static int8_t 			sn_nsdl_fill_uri_query_options				(struct nsdl_s *handle, sn_nsdl_ep_parameters_s *parameter_ptr, sn_coap_hdr_s *source_msg_ptr, uint8_t msg_type);
static int8_t			sn_nsdl_local_rx_function					(struct nsdl_s *handle, sn_coap_hdr_s *coap_packet_ptr, sn_nsdl_addr_s *address_ptr);
static int8_t 			sn_nsdl_resolve_ep_information				(struct nsdl_s *handle, sn_coap_hdr_s *coap_packet_ptr);
static uint8_t 			sn_nsdl_itoa_len							(uint8_t value);
static uint8_t 			*sn_nsdl_itoa								(uint8_t *ptr, uint8_t value);
static uint32_t 		sn_nsdl_atoi								(uint8_t *ptr, uint8_t len);
static uint32_t 		sn_nsdl_ahextoi								(uint8_t *ptr, uint8_t len);
static int8_t			sn_nsdl_resolve_lwm2m_address				(struct nsdl_s *handle, uint8_t *uri, uint16_t uri_len);
static int8_t 			sn_nsdl_process_oma_tlv						(struct nsdl_s *handle, uint8_t *data_ptr, uint16_t data_len);
static void 			sn_nsdl_check_oma_bs_status					(struct nsdl_s *handle);
static int8_t 			sn_nsdl_create_oma_device_object_base		(struct nsdl_s *handle, sn_nsdl_oma_device_t *oma_device_setup_ptr, sn_nsdl_oma_binding_and_mode_t binding_and_mode);


int8_t sn_nsdl_destroy(struct nsdl_s *handle)
{
	if(handle == NULL)
		return SN_NSDL_FAILURE;

	if(handle->ep_information_ptr)
	{
		if(handle->ep_information_ptr->endpoint_name_ptr)
		{
			handle->sn_nsdl_free(handle->ep_information_ptr->endpoint_name_ptr);
			handle->ep_information_ptr->endpoint_name_ptr = 0;
		}
		if(handle->ep_information_ptr->domain_name_ptr)
		{
			handle->sn_nsdl_free(handle->ep_information_ptr->domain_name_ptr);
			handle->ep_information_ptr->domain_name_ptr = 0;
			handle->ep_information_ptr->domain_name_len = 0;
		}
		if(handle->ep_information_ptr->type_ptr)
		{
			handle->sn_nsdl_free(handle->ep_information_ptr->type_ptr);
			handle->ep_information_ptr->type_ptr = 0;
		}

		if(handle->ep_information_ptr->lifetime_ptr)

		{
			handle->sn_nsdl_free(handle->ep_information_ptr->lifetime_ptr);
			handle->ep_information_ptr->lifetime_ptr = 0;
		}

		handle->sn_nsdl_free(handle->ep_information_ptr);
		handle->ep_information_ptr = 0;
	}

	if(handle->nsp_address_ptr)
	{
		if(handle->nsp_address_ptr->omalw_address_ptr)
		{
			if(handle->nsp_address_ptr->omalw_address_ptr->addr_ptr)
			{
				handle->sn_nsdl_free(handle->nsp_address_ptr->omalw_address_ptr->addr_ptr);
				handle->nsp_address_ptr->omalw_address_ptr->addr_ptr = 0;
			}
			handle->sn_nsdl_free(handle->nsp_address_ptr->omalw_address_ptr);
		}

		handle->sn_nsdl_free(handle->nsp_address_ptr);
		handle->nsp_address_ptr = 0;
	}

	if(handle->oma_bs_address_ptr)
	{
		handle->sn_nsdl_free(handle->oma_bs_address_ptr);
	}

	/* Destroy also libCoap and grs part of libNsdl */
	sn_grs_destroy(handle->grs);

	sn_coap_protocol_destroy(handle->grs->coap);

	handle->sn_nsdl_free(handle);

	return SN_NSDL_SUCCESS;
}

struct nsdl_s *sn_nsdl_init	(uint8_t (*sn_nsdl_tx_cb)(sn_nsdl_capab_e , uint8_t *, uint16_t, sn_nsdl_addr_s *),
							uint8_t (*sn_nsdl_rx_cb)(struct nsdl_s *, sn_coap_hdr_s *, sn_nsdl_addr_s *),
							void *(*sn_nsdl_alloc)(uint16_t),void (*sn_nsdl_free)(void *))
{
	/* Check pointers and define function pointers */
	if(!sn_nsdl_alloc || !sn_nsdl_free || !sn_nsdl_tx_cb || !sn_nsdl_rx_cb)
		return NULL;

	struct nsdl_s *handle = NULL;

	handle = sn_nsdl_alloc(sizeof(struct nsdl_s));

	if(handle == NULL)
			return NULL;

	memset(handle, 0, sizeof(struct nsdl_s));

	/* Define function pointers */
	handle->sn_nsdl_alloc = sn_nsdl_alloc;
	handle->sn_nsdl_free = sn_nsdl_free;

	handle->sn_nsdl_tx_callback = sn_nsdl_tx_cb;
	handle->sn_nsdl_rx_callback = sn_nsdl_rx_cb;

	/* Initialize ep parameters struct */
	if(!handle->ep_information_ptr)
	{
		handle->ep_information_ptr = handle->sn_nsdl_alloc(sizeof(sn_nsdl_ep_parameters_s));
		if(!handle->ep_information_ptr)
		{
			return NULL;
		}
		memset(handle->ep_information_ptr, 0, sizeof(sn_nsdl_ep_parameters_s));
	}

	handle->grs = sn_grs_init(sn_nsdl_tx_cb,&sn_nsdl_local_rx_function, sn_nsdl_alloc, sn_nsdl_free);

	/* Initialize GRS */
	if(handle->grs == NULL)
	{
		handle->sn_nsdl_free(handle->ep_information_ptr);
		handle->ep_information_ptr = 0;
		return NULL;
	}

	sn_nsdl_resolve_nsp_address(handle);

	handle->sn_nsdl_endpoint_registered = SN_NSDL_ENDPOINT_NOT_REGISTERED;

	return handle;
}

uint16_t sn_nsdl_register_endpoint(struct nsdl_s *handle, sn_nsdl_ep_parameters_s *endpoint_info_ptr)
{
	/* Local variables */
	sn_coap_hdr_s 	*register_message_ptr;
	uint16_t		message_id	= 0;

	if(endpoint_info_ptr == NULL || handle == NULL)
		return 0;

	/*** Build endpoint register message ***/

	/* Allocate memory for header struct */
	register_message_ptr = handle->sn_nsdl_alloc(sizeof(sn_coap_hdr_s));
	if(register_message_ptr == NULL)
		return 0;

	memset(register_message_ptr, 0, sizeof(sn_coap_hdr_s));

	/* Fill message fields -> confirmable post to specified NSP path */
	register_message_ptr->msg_type 	= 	COAP_MSG_TYPE_CONFIRMABLE;
	register_message_ptr->msg_code 	= 	COAP_MSG_CODE_REQUEST_POST;

	/* Allocate memory for the extended options list */
	register_message_ptr->options_list_ptr = handle->sn_nsdl_alloc(sizeof(sn_coap_options_list_s));
	if(register_message_ptr->options_list_ptr == NULL)
	{
		handle->sn_nsdl_free(register_message_ptr);
		register_message_ptr = 0;
		return 0;
	}

	memset(register_message_ptr->options_list_ptr, 0, sizeof(sn_coap_options_list_s));

	register_message_ptr->uri_path_len = sizeof(resource_path_ptr);
	register_message_ptr->uri_path_ptr = resource_path_ptr;

	/* Fill Uri-query options */
	sn_nsdl_fill_uri_query_options(handle, endpoint_info_ptr, register_message_ptr, SN_NSDL_EP_REGISTER_MESSAGE);
	if(endpoint_info_ptr->ds_register_mode == REGISTER_WITH_RESOURCES)
	{
		/* Built body for message */
		if(sn_nsdl_build_registration_body(handle, register_message_ptr, 0) == SN_NSDL_FAILURE)
		{
			register_message_ptr->uri_path_ptr = NULL;
			register_message_ptr->options_list_ptr->uri_host_ptr = NULL;
			sn_coap_parser_release_allocated_coap_msg_mem(handle->grs->coap, register_message_ptr);
			return 0;
		}
	}
	/* Build and send coap message to NSP */
	message_id = sn_nsdl_internal_coap_send(handle, register_message_ptr, handle->nsp_address_ptr->omalw_address_ptr, SN_NSDL_MSG_REGISTER);

	if(register_message_ptr->payload_ptr)
	{
		handle->sn_nsdl_free(register_message_ptr->payload_ptr);
		register_message_ptr->payload_ptr = NULL;
	}

	register_message_ptr->uri_path_ptr = NULL;
	register_message_ptr->options_list_ptr->uri_host_ptr = NULL;

	sn_coap_parser_release_allocated_coap_msg_mem(handle->grs->coap, register_message_ptr);

	if(handle->ep_information_ptr)
	{
		if(handle->ep_information_ptr->domain_name_ptr)
		{
			handle->sn_nsdl_free(handle->ep_information_ptr->domain_name_ptr);
			handle->ep_information_ptr->domain_name_ptr = 0;
			handle->ep_information_ptr->domain_name_len = 0;
		}

		if(handle->ep_information_ptr->endpoint_name_ptr)
		{
			handle->sn_nsdl_free(handle->ep_information_ptr->endpoint_name_ptr);
			handle->ep_information_ptr->endpoint_name_ptr = 0;
			handle->ep_information_ptr->endpoint_name_len = 0;
		}

		if(endpoint_info_ptr->domain_name_ptr)
		{

			if(!handle->ep_information_ptr->domain_name_ptr)
			{
				handle->ep_information_ptr->domain_name_ptr = handle->sn_nsdl_alloc(endpoint_info_ptr->domain_name_len);
			}
			if(!handle->ep_information_ptr->domain_name_ptr)
			{
				return 0;
			}

			memcpy(handle->ep_information_ptr->domain_name_ptr, endpoint_info_ptr->domain_name_ptr, endpoint_info_ptr->domain_name_len);
			handle->ep_information_ptr->domain_name_len = endpoint_info_ptr->domain_name_len;

		}

		if(endpoint_info_ptr->endpoint_name_ptr)
		{

			if(!handle->ep_information_ptr->endpoint_name_ptr)
			{
				handle->ep_information_ptr->endpoint_name_ptr = handle->sn_nsdl_alloc(endpoint_info_ptr->endpoint_name_len);
			}
			if(!handle->ep_information_ptr->endpoint_name_ptr)
			{
				if(handle->ep_information_ptr->domain_name_ptr)
				{
					handle->sn_nsdl_free(handle->ep_information_ptr->domain_name_ptr);
					handle->ep_information_ptr->domain_name_ptr  = 0;
					handle->ep_information_ptr->domain_name_len = 0;
				}
				return 0;
			}

			memcpy(handle->ep_information_ptr->endpoint_name_ptr, endpoint_info_ptr->endpoint_name_ptr, endpoint_info_ptr->endpoint_name_len);
			handle->ep_information_ptr->endpoint_name_len = endpoint_info_ptr->endpoint_name_len;

		}

		handle->ep_information_ptr->binding_and_mode = endpoint_info_ptr->binding_and_mode;
		handle->ep_information_ptr->ds_register_mode = endpoint_info_ptr->ds_register_mode;
	}

	return message_id;
}

uint16_t sn_nsdl_unregister_endpoint(struct nsdl_s *handle)
{
	/* Local variables */
	sn_coap_hdr_s  	*unregister_message_ptr;
	uint8_t			*temp_ptr = 0;
	uint16_t 		message_id = 0;

	/* Check parameters */
	if(handle == NULL)
		return 0;

	/* Check that EP have been registered */
	if(sn_nsdl_is_ep_registered(handle))
	{

		/* Memory allocation for unregister message */
		unregister_message_ptr = handle->sn_nsdl_alloc(sizeof(sn_coap_hdr_s));
		if(!unregister_message_ptr)
			return 0;

		memset(unregister_message_ptr, 0, sizeof(sn_coap_hdr_s));

		/* Fill unregister message */
		unregister_message_ptr->msg_type = COAP_MSG_TYPE_CONFIRMABLE;
		unregister_message_ptr->msg_code = COAP_MSG_CODE_REQUEST_DELETE;

		unregister_message_ptr->uri_path_len = (RESOURCE_DIR_LEN + 1 + handle->ep_information_ptr->domain_name_len + 1 + handle->ep_information_ptr->endpoint_name_len);
		unregister_message_ptr->uri_path_ptr = handle->sn_nsdl_alloc(unregister_message_ptr->uri_path_len);
		if (!unregister_message_ptr->uri_path_ptr)
		{
			sn_coap_parser_release_allocated_coap_msg_mem(handle->grs->coap, unregister_message_ptr);
			return 0;
		}

		temp_ptr = unregister_message_ptr->uri_path_ptr;

		memcpy(temp_ptr,resource_path_ptr, RESOURCE_DIR_LEN);
		temp_ptr += RESOURCE_DIR_LEN;

		*temp_ptr++ = '/';

		memcpy(temp_ptr ,handle->ep_information_ptr->domain_name_ptr, handle->ep_information_ptr->domain_name_len);
		temp_ptr += handle->ep_information_ptr->domain_name_len;

		*temp_ptr++ = '/';

		memcpy(temp_ptr ,handle->ep_information_ptr->endpoint_name_ptr, handle->ep_information_ptr->endpoint_name_len);

		/* Send message */
		message_id = sn_nsdl_internal_coap_send(handle, unregister_message_ptr, handle->nsp_address_ptr->omalw_address_ptr, SN_NSDL_MSG_UNREGISTER);

		/* Free memory */
		sn_coap_parser_release_allocated_coap_msg_mem(handle->grs->coap, unregister_message_ptr);

	}

	return message_id;
}

uint16_t sn_nsdl_update_registration(struct nsdl_s *handle, uint8_t *lt_ptr, uint8_t lt_len)
{
	/* Local variables */
	sn_coap_hdr_s 	*register_message_ptr;
	uint8_t			*temp_ptr;
	sn_nsdl_ep_parameters_s temp_parameters;
	uint16_t 		message_id = 0;

	/* Check parameters */
	if(handle == NULL)
		return 0;

	memset(&temp_parameters, 0, sizeof(sn_nsdl_ep_parameters_s));

	temp_parameters.lifetime_len = lt_len;
	temp_parameters.lifetime_ptr = lt_ptr;

	/*** Build endpoint register update message ***/

	/* Allocate memory for header struct */
	register_message_ptr = handle->sn_nsdl_alloc(sizeof(sn_coap_hdr_s));
	if(register_message_ptr == NULL)
		return 0;

	memset(register_message_ptr, 0, sizeof(sn_coap_hdr_s));

	/* Fill message fields -> confirmable post to specified NSP path */
	register_message_ptr->msg_type 	= 	COAP_MSG_TYPE_CONFIRMABLE;
	register_message_ptr->msg_code 	= 	COAP_MSG_CODE_REQUEST_PUT;

	register_message_ptr->uri_path_len 	= 	sizeof(resource_path_ptr) + handle->ep_information_ptr->domain_name_len + handle->ep_information_ptr->endpoint_name_len + 2; 	/* = rd/domain/endpoint */

	register_message_ptr->uri_path_ptr 	= 	handle->sn_nsdl_alloc(register_message_ptr->uri_path_len);
	if(!register_message_ptr->uri_path_ptr)
	{
		sn_coap_parser_release_allocated_coap_msg_mem(handle->grs->coap, register_message_ptr);
		return 0;
	}

	temp_ptr = register_message_ptr->uri_path_ptr;

	/* rd/ */
	memcpy(temp_ptr, resource_path_ptr, sizeof(resource_path_ptr));
	temp_ptr += sizeof(resource_path_ptr);
	*temp_ptr++ = '/';

	/* rd/DOMAIN/ */
	memcpy(temp_ptr, handle->ep_information_ptr->domain_name_ptr, handle->ep_information_ptr->domain_name_len);
	temp_ptr += handle->ep_information_ptr->domain_name_len;
	*temp_ptr++ = '/';

	/* rd/domain/ENDPOINT */
	memcpy(temp_ptr, handle->ep_information_ptr->endpoint_name_ptr, handle->ep_information_ptr->endpoint_name_len);


	/* Allocate memory for the extended options list */
	register_message_ptr->options_list_ptr = handle->sn_nsdl_alloc(sizeof(sn_coap_options_list_s));
	if(register_message_ptr->options_list_ptr == NULL)
	{
		sn_coap_parser_release_allocated_coap_msg_mem(handle->grs->coap, register_message_ptr);
		return 0;
	}

	memset(register_message_ptr->options_list_ptr, 0, sizeof(sn_coap_options_list_s));

	/* Fill Uri-query options */
	sn_nsdl_fill_uri_query_options(handle, &temp_parameters, register_message_ptr, SN_NSDL_EP_UPDATE_MESSAGE);

	/* Build payload */
	if(handle->ep_information_ptr->ds_register_mode == REGISTER_WITH_RESOURCES)
	{
		register_message_ptr->payload_len = sn_nsdl_calculate_registration_body_size(handle, 1);

		if(register_message_ptr->payload_len)
		{
			register_message_ptr->payload_ptr = handle->sn_nsdl_alloc(register_message_ptr->payload_len);

			if(!register_message_ptr->payload_ptr)
			{
				sn_coap_parser_release_allocated_coap_msg_mem(handle->grs->coap, register_message_ptr);
				return 0;
			}
			sn_nsdl_build_registration_body(handle, register_message_ptr, 1);
		}
	}

	/* Build and send coap message to NSP */
	message_id = sn_nsdl_internal_coap_send(handle, register_message_ptr, handle->nsp_address_ptr->omalw_address_ptr, SN_NSDL_MSG_UPDATE);

	if(register_message_ptr->payload_ptr)
		handle->sn_nsdl_free(register_message_ptr->payload_ptr);
	sn_coap_parser_release_allocated_coap_msg_mem(handle->grs->coap, register_message_ptr);

	return message_id;
}

void sn_nsdl_nsp_lost(struct nsdl_s *handle)
{
	/* Check parameters */
	if(handle == NULL)
		return;

	handle->sn_nsdl_endpoint_registered = SN_NSDL_ENDPOINT_NOT_REGISTERED;
}

int8_t sn_nsdl_is_ep_registered(struct nsdl_s *handle)
{
	/* Check parameters */
	if(handle == NULL)
		return SN_NSDL_FAILURE;

	return handle->sn_nsdl_endpoint_registered;
}

uint16_t sn_nsdl_send_observation_notification(struct nsdl_s *handle, uint8_t *token_ptr, uint8_t token_len,
													uint8_t *payload_ptr, uint16_t payload_len,
													uint8_t *observe_ptr, uint8_t observe_len,
													sn_coap_msg_type_e message_type, uint8_t content_type)
{
	sn_coap_hdr_s 	*notification_message_ptr;
	uint16_t		return_msg_id = 0;

	/* Check parameters */
	if(handle == NULL)
		return SN_NSDL_FAILURE;

	/* Allocate and initialize memory for header struct */
	notification_message_ptr = handle->sn_nsdl_alloc(sizeof(sn_coap_hdr_s));
	if(notification_message_ptr == NULL)
		return 0;

	memset(notification_message_ptr, 0, sizeof(sn_coap_hdr_s));

	notification_message_ptr->options_list_ptr = handle->sn_nsdl_alloc(sizeof(sn_coap_options_list_s));
	if(notification_message_ptr->options_list_ptr  == NULL)
	{
		handle->sn_nsdl_free(notification_message_ptr);
		return 0;
	}

	memset(notification_message_ptr->options_list_ptr , 0, sizeof(sn_coap_options_list_s));

	/* Fill header */
	notification_message_ptr->msg_type = message_type;
	notification_message_ptr->msg_code = COAP_MSG_CODE_RESPONSE_CONTENT;

	/* Fill token */
	notification_message_ptr->token_len = token_len;
	notification_message_ptr->token_ptr = token_ptr;

	/* Fill payload */
	notification_message_ptr->payload_len = payload_len;
	notification_message_ptr->payload_ptr = payload_ptr;

	/* Fill observe */
	notification_message_ptr->options_list_ptr->observe_len = observe_len;
	notification_message_ptr->options_list_ptr->observe_ptr = observe_ptr;

	/* Fill content type */
	if(content_type)
	{
		notification_message_ptr->content_type_len = 1;
		notification_message_ptr->content_type_ptr = &content_type;
	}

	/* Send message */
	if(sn_nsdl_send_coap_message(handle, handle->nsp_address_ptr->omalw_address_ptr, notification_message_ptr) == SN_NSDL_FAILURE)
		return_msg_id = 0;
	else
		return_msg_id = notification_message_ptr->msg_id;

	/* Free memory */

	notification_message_ptr->payload_ptr = NULL;
	notification_message_ptr->options_list_ptr->observe_ptr = NULL;
	notification_message_ptr->token_ptr = NULL;
	notification_message_ptr->content_type_ptr = NULL;

	sn_coap_parser_release_allocated_coap_msg_mem(handle->grs->coap, notification_message_ptr);

	return return_msg_id;
}

/* * * * * * * * * * */
/* ~ OMA functions ~ */
/* * * * * * * * * * */

uint16_t sn_nsdl_oma_bootstrap(struct nsdl_s *handle, sn_nsdl_addr_s *bootstrap_address_ptr, sn_nsdl_ep_parameters_s *endpoint_info_ptr, sn_nsdl_bs_ep_info_t *bootstrap_endpoint_info_ptr)
{

	/* Local variables */
	sn_coap_hdr_s bootstrap_coap_header;
	uint8_t *uri_query_tmp_ptr;
	uint16_t message_id = 0;

	/* Check parameters */
	if(!bootstrap_address_ptr || !bootstrap_endpoint_info_ptr || !endpoint_info_ptr || !handle)
		return 0;

	/* Create device object */
	if(sn_nsdl_create_oma_device_object_base(handle, bootstrap_endpoint_info_ptr->device_object, endpoint_info_ptr->binding_and_mode) < 0)
		return 0;

	handle->sn_nsdl_oma_bs_done_cb = bootstrap_endpoint_info_ptr->oma_bs_status_cb;

	/* Init CoAP header struct */
	memset(&bootstrap_coap_header, 0, sizeof(sn_coap_hdr_s));

	bootstrap_coap_header.options_list_ptr = handle->sn_nsdl_alloc(sizeof(sn_coap_options_list_s));
	if(!bootstrap_coap_header.options_list_ptr)
		return 0;

	memset(bootstrap_coap_header.options_list_ptr, 0, sizeof(sn_coap_options_list_s));

	/* Build bootstrap start message */
	bootstrap_coap_header.msg_code = COAP_MSG_CODE_REQUEST_POST;
	bootstrap_coap_header.msg_type = COAP_MSG_TYPE_CONFIRMABLE;

	bootstrap_coap_header.uri_path_ptr = bs_uri;
	bootstrap_coap_header.uri_path_len = sizeof(bs_uri);

	uri_query_tmp_ptr = handle->sn_nsdl_alloc(endpoint_info_ptr->endpoint_name_len + BS_EP_PARAMETER_LEN);
	if(!uri_query_tmp_ptr)
	{
		handle->sn_nsdl_free(bootstrap_coap_header.options_list_ptr);
		return 0;
	}

	memcpy(uri_query_tmp_ptr, bs_ep_name, BS_EP_PARAMETER_LEN);
	memcpy((uri_query_tmp_ptr + BS_EP_PARAMETER_LEN), endpoint_info_ptr->endpoint_name_ptr, endpoint_info_ptr->endpoint_name_len);

	bootstrap_coap_header.options_list_ptr->uri_query_len = endpoint_info_ptr->endpoint_name_len + BS_EP_PARAMETER_LEN;
	bootstrap_coap_header.options_list_ptr->uri_query_ptr = uri_query_tmp_ptr;

	/* Save bootstrap server address */
	handle->oma_bs_address_len = bootstrap_address_ptr->addr_len; 		/* Length.. */
	handle->oma_bs_address_ptr = handle->sn_nsdl_alloc(handle->oma_bs_address_len);		/* Address.. */
	if(!handle->oma_bs_address_ptr)
	{
		handle->sn_nsdl_free(bootstrap_coap_header.options_list_ptr);
		handle->sn_nsdl_free(uri_query_tmp_ptr);
		return SN_NSDL_FAILURE;
	}
	memcpy(handle->oma_bs_address_ptr, bootstrap_address_ptr->addr_ptr, handle->oma_bs_address_len);
	handle->oma_bs_port = bootstrap_address_ptr->port;					/* And port */

	/* Send message */
	message_id = sn_nsdl_send_coap_message(handle, bootstrap_address_ptr, &bootstrap_coap_header);

	/* Free allocated memory */
	handle->sn_nsdl_free(uri_query_tmp_ptr);
	handle->sn_nsdl_free(bootstrap_coap_header.options_list_ptr);

	return message_id;
}

omalw_certificate_list_t *sn_nsdl_get_certificates(struct nsdl_s *handle)
{
	sn_nsdl_resource_info_s *resource_ptr = 0;;
	omalw_certificate_list_t *certi_list_ptr = 0;

	/* Check parameters */
	if(handle == NULL)
		return NULL;

	certi_list_ptr = handle->sn_nsdl_alloc(sizeof(omalw_certificate_list_t));

	if(!certi_list_ptr)
		return NULL;

	/* Get private key resource */
	resource_ptr = sn_nsdl_get_resource(handle, 5, (void*)"0/0/5");
	if(!resource_ptr)
	{
		handle->sn_nsdl_free(certi_list_ptr);
		return NULL;
	}
	certi_list_ptr->own_private_key_ptr = resource_ptr->resource;
	certi_list_ptr->own_private_key_len = resource_ptr->resourcelen;

	/* Get client certificate resource */
	resource_ptr = sn_nsdl_get_resource(handle, 5, (void*)"0/0/4");
	if(!resource_ptr)
	{
		handle->sn_nsdl_free(certi_list_ptr);
		return NULL;
	}
	certi_list_ptr->certificate_ptr[0] = resource_ptr->resource;
	certi_list_ptr->certificate_len[0] = resource_ptr->resourcelen;

	/* Get root certificate resource */
	resource_ptr = sn_nsdl_get_resource(handle, 5, (void*)"0/0/3");
	if(!resource_ptr)
	{
		handle->sn_nsdl_free(certi_list_ptr);
		return NULL;
	}
	certi_list_ptr->certificate_ptr[1] = resource_ptr->resource;
	certi_list_ptr->certificate_len[1] = resource_ptr->resourcelen;

	/* return filled list */
	return certi_list_ptr;

}

int8_t sn_nsdl_update_certificates(struct nsdl_s *handle, omalw_certificate_list_t* certificate_ptr, uint8_t certificate_chain)
{
	(void)certificate_chain;

	/* Check pointers */
	if(!certificate_ptr || !handle)
		return SN_NSDL_FAILURE;

	sn_nsdl_resource_info_s *resource_ptr = 0;;

	/* Get private key resource */
	resource_ptr = sn_nsdl_get_resource(handle, 5, (void*)"0/0/5");
	if(!resource_ptr)
	{
		return SN_NSDL_FAILURE;
	}
	handle->sn_nsdl_free(resource_ptr->resource);
	resource_ptr->resource = certificate_ptr->own_private_key_ptr;
	resource_ptr->resourcelen = certificate_ptr->own_private_key_len;

	/* Get client certificate resource */
	resource_ptr = sn_nsdl_get_resource(handle, 5, (void*)"0/0/4");
	if(!resource_ptr)
	{
		return SN_NSDL_FAILURE;
	}
	handle->sn_nsdl_free(resource_ptr->resource);
	resource_ptr->resource = certificate_ptr->certificate_ptr[0];
	resource_ptr->resourcelen = certificate_ptr->certificate_len[0];

	/* Get root certificate resource */
	resource_ptr = sn_nsdl_get_resource(handle, 5, (void*)"0/0/3");
	if(!resource_ptr)
	{
		return SN_NSDL_FAILURE;
	}
	handle->sn_nsdl_free(resource_ptr->resource);
	resource_ptr->resource = certificate_ptr->certificate_ptr[1];
	resource_ptr->resourcelen = certificate_ptr->certificate_len[1];

	return SN_NSDL_SUCCESS;
}

int8_t sn_nsdl_create_oma_device_object(struct nsdl_s *handle, sn_nsdl_oma_device_t *device_object_ptr)
{
	sn_nsdl_resource_info_s *resource_temp = 0;
	uint8_t path[8] = "3/0/11/0";

	if(!device_object_ptr || !handle)
		return SN_NSDL_FAILURE;

	/* * Error code * */

	/* Get first error message */
	resource_temp = sn_grs_search_resource(handle->grs, 8, path, SN_GRS_SEARCH_METHOD);

	while(resource_temp)
	{
		if(resource_temp->resource)
		{
			/* If no error code set */
			if(*resource_temp->resource == 0)
			{
				/* Set error code */
				*resource_temp->resource = (uint8_t)device_object_ptr->error_code;
				resource_temp->resourcelen = 1;

				sn_nsdl_update_resource(handle, resource_temp);
				return SN_NSDL_SUCCESS;
			}
			break;
		}

		if(path[7] == '9')
			return SN_NSDL_FAILURE;

		path[7]++;
		resource_temp = sn_grs_search_resource(handle->grs, 8, path, SN_GRS_SEARCH_METHOD);
	}

	/* Create new resource for this error */
	resource_temp = handle->sn_nsdl_alloc(sizeof(sn_nsdl_resource_info_s));
	if(!resource_temp)
		return SN_NSDL_FAILURE;

	memset(resource_temp, 0, sizeof(sn_nsdl_resource_info_s));

	resource_temp->access = SN_GRS_GET_ALLOWED;
	resource_temp->mode = SN_GRS_DYNAMIC;

	resource_temp->path = path;
	resource_temp->pathlen = 8;

	resource_temp->resource = handle->sn_nsdl_alloc(1);
	if(!resource_temp->resource)
	{
		handle->sn_nsdl_free(resource_temp);
		return SN_NSDL_FAILURE;
	}

	*resource_temp->resource = (uint8_t)device_object_ptr->error_code;
	resource_temp->resourcelen = 1;

	resource_temp->resource_parameters_ptr = handle->sn_nsdl_alloc(sizeof(sn_nsdl_resource_parameters_s));

	memset(resource_temp->resource_parameters_ptr, 0, sizeof(sn_nsdl_resource_parameters_s));

	if(!resource_temp->resource_parameters_ptr)
	{
		handle->sn_nsdl_free(resource_temp->resource);
		handle->sn_nsdl_free(resource_temp);

		return SN_NSDL_FAILURE;
	}

	sn_nsdl_create_resource(handle, resource_temp);

	handle->sn_nsdl_free(resource_temp->resource);
	handle->sn_nsdl_free(resource_temp->resource_parameters_ptr);
	handle->sn_nsdl_free(resource_temp);

	return SN_NSDL_SUCCESS;
}

char *sn_nsdl_get_version(void)
{
#if defined(YOTTA_COMPONENT_VERSION)
	return YOTTA_COMPONENT_VERSION;
#elif defined(VERSION)
	return VERSION;
#else
	return "0.0.0";
#endif
}


int8_t sn_nsdl_process_coap(struct nsdl_s *handle, uint8_t *packet_ptr, uint16_t packet_len, sn_nsdl_addr_s *src_ptr)
{
	sn_coap_hdr_s 			*coap_packet_ptr 	= NULL;
	sn_coap_hdr_s			*coap_response_ptr  = NULL;

	/* Check parameters */
	if(handle == NULL)
		return SN_NSDL_FAILURE;

	/* Parse CoAP packet */
	coap_packet_ptr = sn_coap_protocol_parse(handle->grs->coap, src_ptr, packet_len, packet_ptr);

	/* Check if parsing was successfull */
	if(coap_packet_ptr == (sn_coap_hdr_s *)NULL)
		return SN_NSDL_FAILURE;

	/* Check, if coap itself sends response, or block receiving is ongoing... */
	if(coap_packet_ptr->coap_status != COAP_STATUS_OK && coap_packet_ptr->coap_status != COAP_STATUS_PARSER_BLOCKWISE_MSG_RECEIVED)
	{
		sn_coap_parser_release_allocated_coap_msg_mem(handle->grs->coap, coap_packet_ptr);
		return SN_NSDL_SUCCESS;
	}

	/* If proxy options added, return not supported */
	if (coap_packet_ptr->options_list_ptr)
	{
		if(coap_packet_ptr->options_list_ptr->proxy_uri_len)
		{
			coap_response_ptr = sn_coap_build_response(handle->grs->coap, coap_packet_ptr, COAP_MSG_CODE_RESPONSE_PROXYING_NOT_SUPPORTED);
			if(coap_response_ptr)
			{
				sn_nsdl_send_coap_message(handle, src_ptr, coap_response_ptr);
				sn_coap_parser_release_allocated_coap_msg_mem(handle->grs->coap, coap_response_ptr);
				sn_coap_parser_release_allocated_coap_msg_mem(handle->grs->coap, coap_packet_ptr);
				return SN_NSDL_SUCCESS;
			}
			else
			{
				sn_coap_parser_release_allocated_coap_msg_mem(handle->grs->coap, coap_packet_ptr);
				return SN_NSDL_FAILURE;
			}
		}
	}

	/* * * * * * * * * * * * * * * * * * * * * * * * * * */
	/* If message is response message, call RX callback  */
	/* * * * * * * * * * * * * * * * * * * * * * * * * * */

	if((coap_packet_ptr->msg_code > COAP_MSG_CODE_REQUEST_DELETE) || (coap_packet_ptr->msg_type == COAP_MSG_TYPE_ACKNOWLEDGEMENT))
	{
		int8_t retval = sn_nsdl_local_rx_function(handle, coap_packet_ptr, src_ptr);
		if(coap_packet_ptr->coap_status == COAP_STATUS_PARSER_BLOCKWISE_MSG_RECEIVED && coap_packet_ptr->payload_ptr)
		{
			handle->sn_nsdl_free(coap_packet_ptr->payload_ptr);
			coap_packet_ptr->payload_ptr = 0;
		}
		sn_coap_parser_release_allocated_coap_msg_mem(handle->grs->coap, coap_packet_ptr);
		return retval;
	}

	/* * If OMA bootstrap message... * */
	if((handle->oma_bs_address_len == src_ptr->addr_len) && (handle->oma_bs_port == src_ptr->port) && !memcmp(handle->oma_bs_address_ptr, src_ptr->addr_ptr, handle->oma_bs_address_len))
	{
		/* TLV message. Parse message and check status of the OMA bootstrap  */
		/* process.	If ok, call cb function and return. Otherwise send error */
		/* and return failure.												 */

		if(coap_packet_ptr->content_type_len == 1) //todo check message type
		{
			if(*coap_packet_ptr->content_type_ptr == 99)
			{
				/* TLV parsing failed. Send response to get non-tlv messages */
				if(sn_nsdl_process_oma_tlv(handle, coap_packet_ptr->payload_ptr, coap_packet_ptr->payload_len) == SN_NSDL_FAILURE)
				{
					coap_response_ptr = sn_coap_build_response(handle->grs->coap, coap_packet_ptr, COAP_MSG_CODE_RESPONSE_NOT_ACCEPTABLE);
					if(coap_response_ptr)
					{
						sn_nsdl_send_coap_message(handle, src_ptr, coap_response_ptr);
						sn_coap_parser_release_allocated_coap_msg_mem(handle->grs->coap, coap_response_ptr);
					}
					else
					{
						sn_coap_parser_release_allocated_coap_msg_mem(handle->grs->coap, coap_packet_ptr);
						return SN_NSDL_FAILURE;
					}
				}
				/* Success TLV parsing */
				else
				{
					coap_response_ptr = sn_coap_build_response(handle->grs->coap, coap_packet_ptr, COAP_MSG_CODE_RESPONSE_CREATED);
					if(coap_response_ptr)
					{
						sn_nsdl_send_coap_message(handle, src_ptr, coap_response_ptr);
						sn_coap_parser_release_allocated_coap_msg_mem(handle->grs->coap, coap_response_ptr);

					}
					else
					{
						sn_coap_parser_release_allocated_coap_msg_mem(handle->grs->coap, coap_packet_ptr);
						return SN_NSDL_FAILURE;
					}
					sn_nsdl_check_oma_bs_status(handle);
				}

				sn_coap_parser_release_allocated_coap_msg_mem(handle->grs->coap, coap_packet_ptr);
				return SN_NSDL_SUCCESS;
			}

			/* Non - TLV message */
			else if(*coap_packet_ptr->content_type_ptr == 97)
			{
				sn_grs_process_coap(handle, coap_packet_ptr, src_ptr);

				/* Todo: move this copying to sn_nsdl_check_oma_bs_status(), also from TLV parser */
				/* Security mode */
				if(*(coap_packet_ptr->uri_path_ptr + (coap_packet_ptr->uri_path_len - 1)) == '2')
				{
					handle->nsp_address_ptr->omalw_server_security = (omalw_server_security_t)sn_nsdl_atoi(coap_packet_ptr->payload_ptr, coap_packet_ptr->payload_len);
					sn_coap_parser_release_allocated_coap_msg_mem(handle->grs->coap, coap_packet_ptr);
				}

				/* NSP address */
				else if (*(coap_packet_ptr->uri_path_ptr + (coap_packet_ptr->uri_path_len - 1)) == '0')
				{
					sn_nsdl_resolve_lwm2m_address(handle, coap_packet_ptr->payload_ptr, coap_packet_ptr->payload_len);
					sn_coap_parser_release_allocated_coap_msg_mem(handle->grs->coap, coap_packet_ptr);
				}

				sn_nsdl_check_oma_bs_status(handle);
			}
			else
			{
				sn_coap_parser_release_allocated_coap_msg_mem(handle->grs->coap, coap_packet_ptr);
				return SN_NSDL_FAILURE;
			}
		}
		else
		{
			sn_coap_parser_release_allocated_coap_msg_mem(handle->grs->coap, coap_packet_ptr);
			return SN_NSDL_FAILURE;
		}


		return SN_NSDL_SUCCESS;
	}


	/* * * * * * * * * * * * * * * */
	/* Other messages are for GRS  */
	/* * * * * * * * * * * * * * * */

	return sn_grs_process_coap(handle, coap_packet_ptr, src_ptr);
}

int8_t sn_nsdl_exec(uint32_t time)
{
	/* Call CoAP execution function */
	return sn_coap_protocol_exec(time);
}

sn_nsdl_resource_info_s *sn_nsdl_get_resource(struct nsdl_s *handle, uint16_t pathlen, uint8_t *path_ptr)
{
	/* Check parameters */
	if(handle == NULL)
		return NULL;

	return sn_grs_search_resource(handle->grs, pathlen, path_ptr, SN_GRS_SEARCH_METHOD);
}


/**
 * \fn static uint16_t sn_nsdl_internal_coap_send(struct nsdl_s *handle, sn_coap_hdr_s *coap_header_ptr, sn_nsdl_addr_s *dst_addr_ptr, uint8_t message_description)
 *
 *
 * \brief To send NSDL messages. Stores message id?s and message description to catch response from NSP server
 * \param 	*handle				Pointer to nsdl-library handle
 * \param	*coap_header_ptr	Pointer to the CoAP message header to be sent
 * \param	*dst_addr_ptr		Pointer to the address structure that contains destination address information
 * \param	message_description Message description to be stored to list for waiting response
 *
 * \return	message id, 0 if failed
 */
static uint16_t sn_nsdl_internal_coap_send(struct nsdl_s *handle, sn_coap_hdr_s *coap_header_ptr, sn_nsdl_addr_s *dst_addr_ptr, uint8_t message_description)
{
	uint8_t 	*coap_message_ptr 	= NULL;
	uint16_t	coap_message_len 	= 0;

	coap_message_len = sn_coap_builder_calc_needed_packet_data_size(coap_header_ptr);

	if(coap_message_len == 0)
		return 0;

	coap_message_ptr = handle->sn_nsdl_alloc(coap_message_len);
	if(!coap_message_ptr)
		return 0;

	/* Build message */
	if(sn_coap_protocol_build(handle->grs->coap, dst_addr_ptr,coap_message_ptr, coap_header_ptr) < 0)
	{
		handle->sn_nsdl_free(coap_message_ptr);
		return 0;
	}

	/* If mesage type is confirmable, save it to list to wait for reply */
	if(coap_header_ptr->msg_type == COAP_MSG_TYPE_CONFIRMABLE)
	{
		if(message_description == SN_NSDL_MSG_REGISTER)
		{
			handle->register_msg_id = coap_header_ptr->msg_id;
		}
		if(message_description == SN_NSDL_MSG_UNREGISTER)
		{
			handle->unregister_msg_id = coap_header_ptr->msg_id;
		}

	}

	handle->sn_nsdl_tx_callback(SN_NSDL_PROTOCOL_COAP, coap_message_ptr, coap_message_len, dst_addr_ptr);
	handle->sn_nsdl_free(coap_message_ptr);

	return coap_header_ptr->msg_id;
}

/**
 * \fn static void sn_nsdl_resolve_nsp_address(struct nsdl_s *handle)
 *
 * \brief Resolves NSP server address.
 *
 * \param *handle Pointer to nsdl-library handle
 * \note Application must set NSP address with set_nsp_address
 */
static void sn_nsdl_resolve_nsp_address(struct nsdl_s *handle)
{
	/* Local variables */
	if(!handle->nsp_address_ptr)
	{
		//allocate only if previously not allocated
		handle->nsp_address_ptr = handle->sn_nsdl_alloc(sizeof(sn_nsdl_oma_server_info_t));
	}

	if(handle->nsp_address_ptr)
	{
		handle->nsp_address_ptr->omalw_server_security = SEC_NOT_SET;
		handle->nsp_address_ptr->omalw_address_ptr = handle->sn_nsdl_alloc(sizeof(sn_nsdl_addr_s));
		if(handle->nsp_address_ptr->omalw_address_ptr)
		{
			memset(handle->nsp_address_ptr->omalw_address_ptr, 0, sizeof(sn_nsdl_addr_s));
			handle->nsp_address_ptr->omalw_address_ptr->type = SN_NSDL_ADDRESS_TYPE_NONE;
		}
	}
}

static int8_t sn_nsdl_create_oma_device_object_base(struct nsdl_s *handle, sn_nsdl_oma_device_t *oma_device_setup_ptr, sn_nsdl_oma_binding_and_mode_t binding_and_mode)
{
	sn_nsdl_resource_info_s new_resource;
	uint8_t object_path[8] = "3/0/11/0";
	uint8_t resource_temp[3];
	uint8_t x = 0;

	if(!oma_device_setup_ptr)
		return SN_NSDL_FAILURE;

	/* * Create resources. * */

	/* These resources can be created multiple times. */
	memset(&new_resource, 0, sizeof(sn_nsdl_resource_info_s));
	new_resource.resource_parameters_ptr = handle->sn_nsdl_alloc(sizeof(sn_nsdl_resource_parameters_s));
	if(!new_resource.resource_parameters_ptr)
		return SN_NSDL_FAILURE;

	memset(new_resource.resource_parameters_ptr, 0, sizeof(sn_nsdl_resource_parameters_s));

	/* Create error - resource */
	new_resource.mode = SN_GRS_STATIC;
	new_resource.access = SN_GRS_GET_ALLOWED;

	new_resource.path = object_path;
	new_resource.pathlen = 8;

	sn_nsdl_itoa(resource_temp, (uint8_t)oma_device_setup_ptr->error_code);

	new_resource.resource = resource_temp;
	new_resource.resourcelen = 1;

	if(sn_nsdl_create_resource(handle, &new_resource) != SN_NSDL_SUCCESS)
	{
		handle->sn_nsdl_free(new_resource.resource_parameters_ptr);
		return SN_NSDL_FAILURE;
	}

	/* These resources can be only once, during OMA bootstrap.. */
	/* Create supported binding and modes */
	object_path[5] = '6';
	new_resource.path = object_path;
	new_resource.pathlen = 6;

	if(binding_and_mode & 0x01)
	{
		resource_temp[x] = 'U';
		x++;
		if(binding_and_mode & 0x02)
		{
			resource_temp[x] = 'Q';
			x++;
		}
	}
	if(binding_and_mode & 0x04)
	{
		resource_temp[x] = 'S';
		x++;
		if((binding_and_mode & 0x02) && !(binding_and_mode & 0x01))
		{
			resource_temp[x] = 'Q';
			x++;
		}
	}

	new_resource.resourcelen = x;

	if(new_resource.resourcelen)
		new_resource.resource = resource_temp;
	else
		new_resource.resource = 0;


	if(sn_nsdl_create_resource(handle, &new_resource) != SN_NSDL_SUCCESS)
	{
		handle->sn_nsdl_free(new_resource.resource_parameters_ptr);
		return SN_NSDL_FAILURE;
	}


	/* Create dynamic reboot object */
	new_resource.mode = SN_GRS_DYNAMIC;

	new_resource.access = SN_GRS_POST_ALLOWED;

	object_path[4] = '4';

	new_resource.path = object_path;
	new_resource.pathlen = 5;

	new_resource.resourcelen = 0;
	new_resource.resource = 0;

	new_resource.sn_grs_dyn_res_callback = oma_device_setup_ptr->sn_oma_device_boot_callback;

	if(sn_nsdl_create_resource(handle, &new_resource) != SN_NSDL_SUCCESS)
	{
		handle->sn_nsdl_free(new_resource.resource_parameters_ptr);
		return SN_NSDL_FAILURE;
	}

	handle->sn_nsdl_free(new_resource.resource_parameters_ptr);
	return SN_NSDL_SUCCESS;
}

/**
 * \fn int8_t sn_nsdl_build_registration_body(struct nsdl_s *handle, sn_coap_hdr_s *message_ptr, uint8_t updating_registeration)
 *
 * \brief 	To build GRS resources to registration message payload
 * \param *handle Pointer to nsdl-library handle
 * \param	*message_ptr Pointer to CoAP message header
 *
 * \return	SN_NSDL_SUCCESS = 0, Failed = -1
 */
int8_t sn_nsdl_build_registration_body(struct nsdl_s *handle, sn_coap_hdr_s *message_ptr, uint8_t updating_registeration)
{
	/* Local variables */
	uint8_t					*temp_ptr;
	const sn_nsdl_resource_info_s 	*resource_temp_ptr;


	/* Calculate needed memory and allocate */
	message_ptr->payload_len = sn_nsdl_calculate_registration_body_size(handle, updating_registeration);

	/* If no resources to be registered, return SN_NSDL_SUCCESS */
	if(!message_ptr->payload_len)
	{
		return SN_NSDL_SUCCESS;
	}

	message_ptr->payload_ptr = handle->sn_nsdl_alloc(message_ptr->payload_len);
	if(!message_ptr->payload_ptr)
	{
		return SN_NSDL_FAILURE;
	}

	/* Build message */
	temp_ptr = message_ptr->payload_ptr;

	resource_temp_ptr = sn_grs_get_first_resource(handle->grs);

	/* Loop trough all resources */
	while(resource_temp_ptr)
	{
		/* if resource needs to be registered */
		if(resource_temp_ptr->resource_parameters_ptr)
		{
			if(updating_registeration && resource_temp_ptr->resource_parameters_ptr->registered == SN_NDSL_RESOURCE_REGISTERED)
			{
				resource_temp_ptr = sn_grs_get_next_resource(handle->grs, resource_temp_ptr);
				continue;
			}
			else
			{
				resource_temp_ptr->resource_parameters_ptr->registered = SN_NDSL_RESOURCE_REGISTERED;
			}

			/* If not first resource, add '.' to separator */
			if(temp_ptr != message_ptr->payload_ptr)
				*temp_ptr++ = ',';

			*temp_ptr++ = '<';
			*temp_ptr++ = '/';
			memcpy(temp_ptr, resource_temp_ptr->path, resource_temp_ptr->pathlen);
			temp_ptr += resource_temp_ptr->pathlen;
			*temp_ptr++ = '>';

			/* Resource attributes */
			if(resource_temp_ptr->resource_parameters_ptr->resource_type_len)
			{
				*temp_ptr++ = ';';
				memcpy(temp_ptr, resource_type_parameter, RT_PARAMETER_LEN);
				temp_ptr += RT_PARAMETER_LEN;
				*temp_ptr++ = '"';
				memcpy(temp_ptr, resource_temp_ptr->resource_parameters_ptr->resource_type_ptr, resource_temp_ptr->resource_parameters_ptr->resource_type_len);
				temp_ptr += resource_temp_ptr->resource_parameters_ptr->resource_type_len;
				*temp_ptr++ = '"';
			}

			if(resource_temp_ptr->resource_parameters_ptr->interface_description_len)
			{
				*temp_ptr++ = ';';
				memcpy(temp_ptr, if_description_parameter, IF_PARAMETER_LEN);
				temp_ptr += IF_PARAMETER_LEN;
				*temp_ptr++ = '"';
				memcpy(temp_ptr, resource_temp_ptr->resource_parameters_ptr->interface_description_ptr, resource_temp_ptr->resource_parameters_ptr->interface_description_len);
				temp_ptr += resource_temp_ptr->resource_parameters_ptr->interface_description_len;
				*temp_ptr++ = '"';
			}

			if(resource_temp_ptr->resource_parameters_ptr->coap_content_type != 0)
			{
				*temp_ptr++ = ';';
				memcpy(temp_ptr, coap_con_type_parameter, COAP_CON_PARAMETER_LEN);
				temp_ptr += COAP_CON_PARAMETER_LEN;
				*temp_ptr++ = '"';
				temp_ptr = sn_nsdl_itoa(temp_ptr, resource_temp_ptr->resource_parameters_ptr->coap_content_type);
				*temp_ptr++ = '"';
			}

			/* ;obs */
			if(resource_temp_ptr->resource_parameters_ptr->observable)
			{
				*temp_ptr++ = ';';
				memcpy(temp_ptr, obs_parameter, OBS_PARAMETER_LEN);
				temp_ptr += OBS_PARAMETER_LEN;
			}

			/* ;aobs;id= */
			/* todo: aosb not supported ATM */
			/*
			if((resource_temp_ptr->resource_parameters_ptr->auto_obs_len > 0 && resource_temp_ptr->resource_parameters_ptr->auto_obs_len <= 8) &&
					resource_temp_ptr->resource_parameters_ptr->auto_obs_ptr)
			{
				uint8_t i = 0;

				*temp_ptr++ = ';';
				memcpy(temp_ptr, aobs_parameter, AOBS_PARAMETER_LEN);
				temp_ptr += AOBS_PARAMETER_LEN;

				while(i < resource_temp_ptr->resource_parameters_ptr->auto_obs_len)
				{
					temp_ptr = sn_nsdl_itoa(temp_ptr, *(resource_temp_ptr->resource_parameters_ptr->auto_obs_ptr + i));
					i++;
				}
			}
			*/

		}

		resource_temp_ptr = sn_grs_get_next_resource(handle->grs, resource_temp_ptr);
	}

	return SN_NSDL_SUCCESS;
}

/**
 * \fn static uint16_t sn_nsdl_calculate_registration_body_size(struct nsdl_s *handle, uint8_t updating_registeration)
 *
 *
 * \brief	Calculates registration message payload size
 * \param 	*handle					Pointer to nsdl-library handle
 * \param	*grs_resources_list_ptr Pointer to list of GRS resources
 *
 * \return	Needed payload size
 */
static uint16_t sn_nsdl_calculate_registration_body_size(struct nsdl_s *handle, uint8_t updating_registeration)
{
	/* Local variables */
	uint16_t return_value = 0;
	const sn_nsdl_resource_info_s *resource_temp_ptr;

	/* check pointer */

	resource_temp_ptr = sn_grs_get_first_resource(handle->grs);

	while(resource_temp_ptr)
	{
		if(resource_temp_ptr->resource_parameters_ptr)
		{
			if(updating_registeration && resource_temp_ptr->resource_parameters_ptr->registered == SN_NDSL_RESOURCE_REGISTERED)
			{
				resource_temp_ptr = sn_grs_get_next_resource(handle->grs, resource_temp_ptr);
				continue;
			}

			/* If not first resource, then '.' will be added */
			if(return_value)
				return_value++;

			/* Count length for the resource path </path> */
			return_value +=	(3 + resource_temp_ptr->pathlen);

			/* Count lengths of the attributes */

			/* Resource type parameter */
			if(resource_temp_ptr->resource_parameters_ptr->resource_type_len)
			{
				/* ;rt="restype" */
				return_value += (6 + resource_temp_ptr->resource_parameters_ptr->resource_type_len);
			}

			/* Interface description parameter */
			if(resource_temp_ptr->resource_parameters_ptr->interface_description_len)
			{
				/* ;if="iftype" */
				return_value += (6 + resource_temp_ptr->resource_parameters_ptr->interface_description_len);
			}

			if(resource_temp_ptr->resource_parameters_ptr->coap_content_type != 0)
			{
				/* ;if="content" */
				return_value += 6; // all but not content
				return_value += sn_nsdl_itoa_len(resource_temp_ptr->resource_parameters_ptr->coap_content_type);
			}

			if(resource_temp_ptr->resource_parameters_ptr->observable)
			{
				/* ;obs */
				return_value += 4;
			}
			/*todo: aobs not supported ATM */
			/*
			if((resource_temp_ptr->resource_parameters_ptr->auto_obs_len > 0 && resource_temp_ptr->resource_parameters_ptr->auto_obs_len <= 8) &&
					resource_temp_ptr->resource_parameters_ptr->auto_obs_ptr)
			{
				uint8_t i = resource_temp_ptr->resource_parameters_ptr->auto_obs_len;
				// ;aobs;id=
				return_value += 9;
				while(i--)
				{
					return_value += sn_nsdl_itoa_len(*(resource_temp_ptr->resource_parameters_ptr->auto_obs_ptr + i));
				}
			}
			*/

		}

		resource_temp_ptr = sn_grs_get_next_resource(handle->grs, resource_temp_ptr);

	}

	return return_value;

}

/**
 * \fn static uint8_t sn_nsdl_calculate_uri_query_option_len(sn_nsdl_ep_parameters_s *endpoint_info_ptr, uint8_t msg_type)
 *
 *
 * \brief Calculates needed uri query option length
 *
 * \param *endpoint_info_ptr 	Pointer to endpoint info structure
 * \param msg_type				Message type
 *
 * \return	SN_NSDL_SUCCESS = 0, Failed = -1
 */
static uint8_t sn_nsdl_calculate_uri_query_option_len(sn_nsdl_ep_parameters_s *endpoint_info_ptr, uint8_t msg_type)
{
	uint8_t return_value = 0;
	uint8_t number_of_parameters = 0;


	if((endpoint_info_ptr->endpoint_name_len != 0) && (msg_type == SN_NSDL_EP_REGISTER_MESSAGE) && endpoint_info_ptr->endpoint_name_ptr != 0)
	{
		return_value += endpoint_info_ptr->endpoint_name_len;
		return_value += EP_NAME_PARAMETERS_LEN;	//ep=
		number_of_parameters++;
	}

	if((endpoint_info_ptr->type_len != 0) && (msg_type == SN_NSDL_EP_REGISTER_MESSAGE) && (endpoint_info_ptr->type_ptr != 0))
	{
		return_value+=endpoint_info_ptr->type_len;
		return_value += ET_PARAMETER_LEN; 		//et=
		number_of_parameters++;
	}

	if((endpoint_info_ptr->lifetime_len != 0) && (endpoint_info_ptr->lifetime_ptr != 0))
	{
		return_value+=endpoint_info_ptr->lifetime_len;
		return_value += LT_PARAMETER_LEN;		//lt=
		number_of_parameters++;
	}

	if((endpoint_info_ptr->domain_name_len != 0) && (msg_type == SN_NSDL_EP_REGISTER_MESSAGE) && (endpoint_info_ptr->domain_name_ptr != 0))
	{
		return_value+=endpoint_info_ptr->domain_name_len;
		return_value += DOMAIN_PARAMETER_LEN;		//d=
		number_of_parameters++;
	}

	if((endpoint_info_ptr->binding_and_mode != 0) && (msg_type == SN_NSDL_EP_REGISTER_MESSAGE))
	{
		return_value += BS_QUEUE_MODE_PARAMATER_LEN;
		if(endpoint_info_ptr->binding_and_mode & 0x01)
			return_value++;
		if(endpoint_info_ptr->binding_and_mode & 0x04)
			return_value++;
		if((endpoint_info_ptr->binding_and_mode & 0x02) && ((endpoint_info_ptr->binding_and_mode & 0x04)||(endpoint_info_ptr->binding_and_mode & 0x01)))
			return_value++;

		number_of_parameters++;
	}

	if(number_of_parameters != 0)
		return_value += (number_of_parameters - 1);

	return return_value;
}

/**
 * \fn static int8_t sn_nsdl_fill_uri_query_options(struct nsdl_s *handle, sn_nsdl_ep_parameters_s *parameter_ptr, sn_coap_hdr_s *source_msg_ptr, uint8_t msg_type)
 *
 *
 * \brief Fills uri-query options to message header struct
 * \param *handle			Pointer to nsdl-library handle
 * \param *parameter_ptr 	Pointer to endpoint parameters struct
 * \param *source_msg_ptr	Pointer to CoAP header struct
 * \param msg_type			Message type
 *
 * \return	SN_NSDL_SUCCESS = 0, Failed = -1
 */
static int8_t sn_nsdl_fill_uri_query_options(struct nsdl_s *handle, sn_nsdl_ep_parameters_s *parameter_ptr, sn_coap_hdr_s *source_msg_ptr, uint8_t msg_type)
{
	uint8_t *temp_ptr = NULL;
	source_msg_ptr->options_list_ptr->uri_query_len  = sn_nsdl_calculate_uri_query_option_len(parameter_ptr, msg_type);

	if(source_msg_ptr->options_list_ptr->uri_query_len == 0)
		return 0;

	source_msg_ptr->options_list_ptr->uri_query_ptr 	= 	handle->sn_nsdl_alloc(source_msg_ptr->options_list_ptr->uri_query_len);

	if (source_msg_ptr->options_list_ptr->uri_query_ptr == NULL)
			return SN_NSDL_FAILURE;

	temp_ptr = source_msg_ptr->options_list_ptr->uri_query_ptr;

	/******************************************************/
	/* If endpoint name is configured, fill needed fields */
	/******************************************************/

	if((parameter_ptr->endpoint_name_len != 0) && (parameter_ptr->endpoint_name_ptr != 0) && (msg_type == SN_NSDL_EP_REGISTER_MESSAGE))
	{
		/* fill endpoint name, first ?ep=, then endpoint name */
		memcpy(temp_ptr, ep_name_parameter_string, sizeof(ep_name_parameter_string));
		temp_ptr += EP_NAME_PARAMETERS_LEN;
		memcpy(temp_ptr, parameter_ptr->endpoint_name_ptr, parameter_ptr->endpoint_name_len);
		temp_ptr += parameter_ptr->endpoint_name_len;
	}

	/******************************************************/
	/* If endpoint type is configured, fill needed fields */
	/******************************************************/

	if((parameter_ptr->type_len != 0) && (parameter_ptr->type_ptr != 0) && (msg_type == SN_NSDL_EP_REGISTER_MESSAGE))
	{
		if(temp_ptr != source_msg_ptr->options_list_ptr->uri_query_ptr)
			*temp_ptr++ = '&';

		memcpy(temp_ptr, et_parameter, sizeof(et_parameter));
		temp_ptr += ET_PARAMETER_LEN;
		memcpy(temp_ptr, parameter_ptr->type_ptr, parameter_ptr->type_len);
		temp_ptr += parameter_ptr->type_len;
	}


	/******************************************************/
	/* If lifetime is configured, fill needed fields */
	/******************************************************/

	if((parameter_ptr->lifetime_len != 0) && (parameter_ptr->lifetime_ptr != 0))
	{
		if(temp_ptr != source_msg_ptr->options_list_ptr->uri_query_ptr)
			*temp_ptr++ = '&';

		memcpy(temp_ptr, ep_lifetime_parameter, sizeof(ep_lifetime_parameter));
		temp_ptr += LT_PARAMETER_LEN;
		memcpy(temp_ptr, parameter_ptr->lifetime_ptr, parameter_ptr->lifetime_len);
		temp_ptr += parameter_ptr->lifetime_len;
	}

	/******************************************************/
	/* If domain is configured, fill needed fields */
	/******************************************************/

	if((parameter_ptr->domain_name_len != 0) && (parameter_ptr->domain_name_ptr != 0) && (msg_type == SN_NSDL_EP_REGISTER_MESSAGE))
	{
		if(temp_ptr != source_msg_ptr->options_list_ptr->uri_query_ptr)
			*temp_ptr++ = '&';

		memcpy(temp_ptr, ep_domain_parameter, sizeof(ep_domain_parameter));
		temp_ptr += DOMAIN_PARAMETER_LEN;
		memcpy(temp_ptr, parameter_ptr->domain_name_ptr, parameter_ptr->domain_name_len);
		temp_ptr += parameter_ptr->domain_name_len;
	}

	/******************************************************/
	/* If queue-mode is configured, fill needed fields    */
	/******************************************************/

	if((parameter_ptr->binding_and_mode != 0) && (msg_type == SN_NSDL_EP_REGISTER_MESSAGE))
	{
		if(temp_ptr != source_msg_ptr->options_list_ptr->uri_query_ptr)
			*temp_ptr++ = '&';

		memcpy(temp_ptr, bs_queue_mode, sizeof(bs_queue_mode));
		temp_ptr += BS_QUEUE_MODE_PARAMATER_LEN;

		if(parameter_ptr->binding_and_mode & 0x01)
		{
			*temp_ptr++ = 'U';
			if(parameter_ptr->binding_and_mode & 0x02)
				*temp_ptr++ = 'Q';
		}

		if(parameter_ptr->binding_and_mode & 0x04)
		{
			*temp_ptr++ = 'S';
			if((parameter_ptr->binding_and_mode & 0x02) && !(parameter_ptr->binding_and_mode & 0x01))
				*temp_ptr++ = 'Q';
		}
	}

	return SN_NSDL_SUCCESS;
}

/**
 * \fn static int8_t sn_nsdl_local_rx_function(struct nsdl_s *handle, sn_coap_hdr_s *coap_packet_ptr, sn_nsdl_addr_s *address_ptr)
 *
 * \brief If received message is reply for the message that NSDL has been sent, it is processed here. Else, packet will be sent to application.
 * \param *handle			Pointer to nsdl-library handle
 * \param *coap_packet_ptr	Pointer to received CoAP packet
 * \param *address_ptr		Pointer to source address struct
 *
 * \return		SN_NSDL_SUCCESS = 0, Failed = -1
 */
static int8_t sn_nsdl_local_rx_function(struct nsdl_s *handle, sn_coap_hdr_s *coap_packet_ptr, sn_nsdl_addr_s *address_ptr)
{
	if((coap_packet_ptr == 0) || (address_ptr == 0))
		return -1;

	if(coap_packet_ptr->msg_id == handle->register_msg_id)
	{
		if(coap_packet_ptr->msg_code == COAP_MSG_CODE_RESPONSE_CREATED)
		{
			handle->sn_nsdl_endpoint_registered = SN_NSDL_ENDPOINT_IS_REGISTERED;
			sn_grs_mark_resources_as_registered(handle);
			if(sn_nsdl_resolve_ep_information(handle, coap_packet_ptr) != SN_NSDL_SUCCESS)
				return SN_NSDL_FAILURE;

			handle->register_msg_id = 0;
		}
	}

	if(coap_packet_ptr->msg_id == handle->unregister_msg_id)
	{
		if(coap_packet_ptr->msg_code == COAP_MSG_CODE_RESPONSE_DELETED)
		{
			if(handle->ep_information_ptr->endpoint_name_ptr)
			{
				handle->sn_nsdl_free(handle->ep_information_ptr->endpoint_name_ptr);
				handle->ep_information_ptr->endpoint_name_ptr = 0;
				handle->ep_information_ptr->endpoint_name_len = 0;
			}
			if(handle->ep_information_ptr->domain_name_ptr)
			{
				handle->sn_nsdl_free(handle->ep_information_ptr->domain_name_ptr);
				handle->ep_information_ptr->domain_name_ptr = 0;
				handle->ep_information_ptr->domain_name_len = 0;
			}

			handle->unregister_msg_id = 0;
		}
	}

	/* No messages to wait for, or message was not response to our request */
	return handle->sn_nsdl_rx_callback(handle, coap_packet_ptr, address_ptr);
}

/**
 * \fn static int8_t sn_nsdl_resolve_ep_information(struct nsdl_s *handle, sn_coap_hdr_s *coap_packet_ptr)
 *
 *
 * \brief Resolves endpoint information from received CoAP message
 * \param *handle 			Pointer to nsdl-library handle
 * \param *coap_packet_ptr 	Pointer to received CoAP message
 *
 * \return	SN_NSDL_SUCCESS = 0, Failed = -1
 */
static int8_t sn_nsdl_resolve_ep_information(struct nsdl_s *handle, sn_coap_hdr_s *coap_packet_ptr)
{
	uint8_t		*temp_ptr;
	uint8_t		parameter_count 	= 0;
	uint16_t	parameter_len 		= 0;

	if(!coap_packet_ptr)
		return SN_NSDL_FAILURE;
	if(!coap_packet_ptr->options_list_ptr)
		return SN_NSDL_FAILURE;
	if(!coap_packet_ptr->options_list_ptr->location_path_ptr)
		return SN_NSDL_FAILURE;

	temp_ptr = coap_packet_ptr->options_list_ptr->location_path_ptr;

	while(temp_ptr <= (coap_packet_ptr->options_list_ptr->location_path_ptr + coap_packet_ptr->options_list_ptr->location_path_len))
	{

		if((temp_ptr == (coap_packet_ptr->options_list_ptr->location_path_ptr + coap_packet_ptr->options_list_ptr->location_path_len)) || (*temp_ptr == '/'))
		{

			parameter_count++;
			if(parameter_count == 2)
			{
				if(!handle->ep_information_ptr->domain_name_ptr)
				{
					handle->ep_information_ptr->domain_name_len = parameter_len - 1;
					handle->ep_information_ptr->domain_name_ptr = handle->sn_nsdl_alloc(handle->ep_information_ptr->domain_name_len);
					if(!handle->ep_information_ptr->domain_name_ptr)
						return SN_NSDL_FAILURE;
					memcpy(handle->ep_information_ptr->domain_name_ptr, temp_ptr - handle->ep_information_ptr->domain_name_len, handle->ep_information_ptr->domain_name_len);
				}

			}
			if(parameter_count == 3)
			{
				if(!handle->ep_information_ptr->endpoint_name_ptr)
				{
					handle->ep_information_ptr->endpoint_name_len = parameter_len - 1;
					handle->ep_information_ptr->endpoint_name_ptr = handle->sn_nsdl_alloc(handle->ep_information_ptr->endpoint_name_len);
					if(!handle->ep_information_ptr->endpoint_name_ptr)
					{
						if(handle->ep_information_ptr->domain_name_ptr)
						{
							handle->sn_nsdl_free(handle->ep_information_ptr->domain_name_ptr);
							handle->ep_information_ptr->domain_name_ptr = NULL;
							handle->ep_information_ptr->domain_name_len = 0;
						}

						return SN_NSDL_FAILURE;

					}
					memcpy(handle->ep_information_ptr->endpoint_name_ptr, temp_ptr - handle->ep_information_ptr->endpoint_name_len, handle->ep_information_ptr->endpoint_name_len);
				}
			}
			parameter_len = 0;
		}
		parameter_len++;
		temp_ptr++;
	}


	return SN_NSDL_SUCCESS;
}

int8_t set_NSP_address(struct nsdl_s *handle, uint8_t *NSP_address, uint16_t port, sn_nsdl_addr_type_e address_type)
{

	/* Check parameters and source pointers */
	if(!handle->nsp_address_ptr || !NSP_address || !handle)
	{
		return SN_NSDL_FAILURE;
	}

	handle->nsp_address_ptr->omalw_address_ptr->type = address_type;
	handle->nsp_address_ptr->omalw_server_security = SEC_NOT_SET;

	if(address_type == SN_NSDL_ADDRESS_TYPE_IPV4)
	{
		if(handle->nsp_address_ptr->omalw_address_ptr->addr_ptr)
		{
			handle->sn_nsdl_free(handle->nsp_address_ptr->omalw_address_ptr->addr_ptr);
		}

		handle->nsp_address_ptr->omalw_address_ptr->addr_len = 4;

		handle->nsp_address_ptr->omalw_address_ptr->addr_ptr = handle->sn_nsdl_alloc(handle->nsp_address_ptr->omalw_address_ptr->addr_len);
		if(!handle->nsp_address_ptr->omalw_address_ptr->addr_ptr)
			return -1;

		memcpy(handle->nsp_address_ptr->omalw_address_ptr->addr_ptr, NSP_address, handle->nsp_address_ptr->omalw_address_ptr->addr_len);
		handle->nsp_address_ptr->omalw_address_ptr->port = port;
	}

	else if(address_type == SN_NSDL_ADDRESS_TYPE_IPV6)
	{
		if(handle->nsp_address_ptr->omalw_address_ptr->addr_ptr)
		{
			handle->sn_nsdl_free(handle->nsp_address_ptr->omalw_address_ptr->addr_ptr);
		}

		handle->nsp_address_ptr->omalw_address_ptr->addr_len = 16;

		handle->nsp_address_ptr->omalw_address_ptr->addr_ptr = handle->sn_nsdl_alloc(handle->nsp_address_ptr->omalw_address_ptr->addr_len);
		if(!handle->nsp_address_ptr->omalw_address_ptr->addr_ptr)
			return -1;

		memcpy(handle->nsp_address_ptr->omalw_address_ptr->addr_ptr, NSP_address, handle->nsp_address_ptr->omalw_address_ptr->addr_len);
		handle->nsp_address_ptr->omalw_address_ptr->port = port;
	}
	return 0;
}


static uint8_t sn_nsdl_itoa_len(uint8_t value)
{
	uint8_t i = 0;

	do
	{
		i++;
	}while((value /= 10) > 0);

	return i;
}

static uint8_t *sn_nsdl_itoa(uint8_t *ptr, uint8_t value)
{

	uint8_t start = 0;
	uint8_t end = 0;
	uint8_t i;

	i = 0;

	/* ITOA */
	do
	{
		ptr[i++] = (value % 10) + '0';
	}while((value /= 10) > 0);

	end = i - 1;

	/* reverse (part of ITOA) */
	while(start < end)
	{
		uint8_t chr;

		chr = ptr[start];
		ptr[start] = ptr[end];
		ptr[end] = chr;

		start++;
		end--;

	}
	return (ptr + i);
}

static uint32_t sn_nsdl_atoi(uint8_t *ptr, uint8_t len)
{

	uint32_t result = 0;

	while(len--)
	{

		if(result)
		{
			result *= 10;
		}

		if(*ptr >= '0' && *ptr<= '9')
			result += *ptr - '0';
		else if(*ptr >= 'a' && *ptr <= 'f')
			result += *ptr - 87;
		else if(*ptr >= 'A' && *ptr <= 'F')
			result += *ptr - 55;

		ptr++;

	}
	return result;

}

static uint32_t sn_nsdl_ahextoi(uint8_t *ptr, uint8_t len)
{

	uint32_t result = 0;

	while(len--)
	{

		if(result)
		{
			result *= 16;
		}

		if(*ptr >= '0' && *ptr<= '9')
			result += *ptr - '0';
		else if(*ptr >= 'a' && *ptr <= 'f')
			result += *ptr - 87;
		else if(*ptr >= 'A' && *ptr <= 'F')
			result += *ptr - 55;

		ptr++;

	}
	return result;

}

static int8_t sn_nsdl_resolve_lwm2m_address(struct nsdl_s *handle, uint8_t *uri, uint16_t uri_len)
{
	uint8_t *temp_ptr = uri;
	uint8_t i = 0;
	uint8_t char_cnt = 0;

	/* jump over coap// */
	while((*(temp_ptr - 2) != '/') || (*(temp_ptr - 1) != '/'))
	{
		temp_ptr++;
		if(temp_ptr - uri >= uri_len)
			return SN_NSDL_FAILURE;
	}

	/* Resolve address type */
	/* Count semicolons */

	while(i < (uri_len - (temp_ptr - uri)))
	{
		if(*(temp_ptr + i) == ':')
			char_cnt++;
		i++;
	}

	/* IPv6 */
	if(char_cnt > 2)
	{
		i = 0;

		handle->nsp_address_ptr->omalw_address_ptr->type = SN_NSDL_ADDRESS_TYPE_IPV6;
		handle->nsp_address_ptr->omalw_address_ptr->addr_len = 16;
		handle->nsp_address_ptr->omalw_address_ptr->addr_ptr = handle->sn_nsdl_alloc(16);
		if(!handle->nsp_address_ptr->omalw_address_ptr->addr_ptr)
			return SN_NSDL_FAILURE;

		memset(handle->nsp_address_ptr->omalw_address_ptr->addr_ptr, 0, 16);
		/* If not found, return failure */
		if(*temp_ptr == '[')
			temp_ptr++;

		/* Resolve address */
		while(i < 16 && ((temp_ptr - uri) + char_cnt) < uri_len)
		{
			char_cnt = 0;
			while(*(temp_ptr + char_cnt) != ':' && *(temp_ptr + char_cnt) != ']')
			{
				char_cnt++;
			}

			if(char_cnt <= 2)
				i++;

			while(char_cnt)
			{
				if(char_cnt%2)
				{
					*(handle->nsp_address_ptr->omalw_address_ptr->addr_ptr + i) = (uint8_t)sn_nsdl_ahextoi(temp_ptr, 1);
					temp_ptr++;
					char_cnt --;
				}
				else
				{
					*(handle->nsp_address_ptr->omalw_address_ptr->addr_ptr + i) = (uint8_t)sn_nsdl_ahextoi(temp_ptr, 2);
					temp_ptr += 2;
					char_cnt -= 2;
				}
				i++;
			}
			temp_ptr++;
		}

		temp_ptr++;
		handle->nsp_address_ptr->omalw_address_ptr->port = sn_nsdl_atoi(temp_ptr, uri_len - (temp_ptr - uri));
	}

	/* IPv4 or Hostname */
	else if(char_cnt == 1)
	{
		char_cnt = 0;
		i = 0;

		/* Check address type */
		while(i < (uri_len - (temp_ptr - uri)))
		{
			if(*(temp_ptr + i) == '.')
				char_cnt++;
			i++;
		}

		/* IPv4 */
		if(char_cnt == 3)
		{
			i = 0;
			char_cnt = 0;

			handle->nsp_address_ptr->omalw_address_ptr->type = SN_NSDL_ADDRESS_TYPE_IPV4;
			handle->nsp_address_ptr->omalw_address_ptr->addr_len = 4;
			handle->nsp_address_ptr->omalw_address_ptr->addr_ptr = handle->sn_nsdl_alloc(4);
			if(!handle->nsp_address_ptr->omalw_address_ptr->addr_ptr)
				return SN_NSDL_FAILURE;

			while(((temp_ptr - uri) < uri_len) && *(temp_ptr - 1) != ':')
			{
				i++;

				if(*(temp_ptr + i) == ':' || *(temp_ptr + i) == '.')
				{
					*(handle->nsp_address_ptr->omalw_address_ptr->addr_ptr + char_cnt) = (uint8_t)sn_nsdl_atoi(temp_ptr, i);
					temp_ptr = temp_ptr + i + 1;
					char_cnt++;
					i = 0;
				}
			}

			handle->nsp_address_ptr->omalw_address_ptr->port = sn_nsdl_atoi(temp_ptr, uri_len - (temp_ptr - uri));
		}

		/* Hostname */
		else
		{
			i = 0;

			handle->nsp_address_ptr->omalw_address_ptr->type = SN_NSDL_ADDRESS_TYPE_HOSTNAME;

			/* Resolve address length */
			if(uri_len > 0xff)
				return SN_NSDL_FAILURE;

			while(((temp_ptr - uri ) + i < uri_len) && *(temp_ptr + i) != ':')
				i++;

			handle->nsp_address_ptr->omalw_address_ptr->addr_len = i;

			/* Copy address */
			handle->nsp_address_ptr->omalw_address_ptr->addr_ptr = handle->sn_nsdl_alloc(i);
			if(!handle->nsp_address_ptr->omalw_address_ptr->addr_ptr)
				return SN_NSDL_FAILURE;

			memcpy(handle->nsp_address_ptr->omalw_address_ptr->addr_ptr, temp_ptr, i);

			temp_ptr += i + 1;

			/* Set port */
			handle->nsp_address_ptr->omalw_address_ptr->port = sn_nsdl_atoi(temp_ptr, uri_len - (temp_ptr - uri));
		}
	}
	else
		return SN_NSDL_FAILURE;

	return SN_NSDL_SUCCESS;
}


int8_t sn_nsdl_process_oma_tlv(struct nsdl_s *handle, uint8_t *data_ptr, uint16_t data_len)
{
	uint8_t *temp_ptr = data_ptr;
	uint8_t type = 0;
	uint16_t identifier = 0;
	uint32_t length = 0;
	uint8_t path_temp[5] = "0/0/x";

	sn_nsdl_resource_info_s resource_temp = {
			.resource_parameters_ptr = 0,
			.mode = SN_GRS_STATIC,
			.pathlen = 5,
			.path = path_temp,
			.resourcelen = 0,
			.resource = 0,
			.access = (sn_grs_resource_acl_e) 0x0f, /* All allowed */
			.sn_grs_dyn_res_callback = 0
	};

	while((temp_ptr - data_ptr) < data_len)
	{
		/* Save type for future use */
		type = *temp_ptr++;

		/* * Bit 5: Indicates the Length of the Identifier. * */
		if(type & 0x20)
		{
			/* 1=The Identifier field of this TLV is 16 bits long */
			identifier = (uint8_t)(*temp_ptr++) << 8;
			identifier += (uint8_t)*temp_ptr++;
		}
		else
		{
			/* 0=The Identifier field of this TLV is 8 bits long */
			identifier = (uint8_t)*temp_ptr++;
		}

		/* * Bit 4-3: Indicates the type of Length. * */
		if((type & 0x18) == 0)
		{
			/* 00 = No length field, the value immediately follows the Identifier field in is of the length indicated by Bits 2-0 of this field */
			length = (type & 0x07);
		}
		else if((type & 0x18) == 0x08)
		{
			/* 01 = The Length field is 8-bits and Bits 2-0 MUST be ignored */
			length = *temp_ptr++;
		}
		else if((type & 0x18) == 0x10)
		{
			/* 10 = The Length field is 16-bits and Bits 2-0 MUST be ignored */
			length = (uint8_t)(*temp_ptr++) << 8;
			length += (uint8_t)*temp_ptr++;
		}
		else if((type & 0x18) == 0x18)
		{
			/* 11 = The Length field is 24-bits and Bits 2-0 MUST be ignored */
			length = (uint8_t)(*temp_ptr++);
			length = length << 16;
			length += (uint8_t)(*temp_ptr++) << 8;
			length += (uint8_t)*temp_ptr++;
		}

		/* * Bits 7-6: Indicates the type of Identifier. * */
		if((type & 0xC0) == 0x00)
		{
			/* 00 = Object Instance in which case the Value contains one or more Resource TLVs */
			/* Not implemented, return failure */
		}
		else if((type & 0xC0) == 0xC0)
		{
			/* 11 = Resource with Value */
			switch(identifier)
			{
			case 0:
				/* Resolve LWM2M Server URI */
				sn_nsdl_resolve_lwm2m_address(handle, temp_ptr, length);
				path_temp[4] = '0';
				resource_temp.resource = temp_ptr;
				resource_temp.resourcelen = length;
				if(sn_nsdl_create_resource(handle, &resource_temp) != SN_NSDL_SUCCESS)
					return SN_NSDL_FAILURE;
				break;
			case 2:
				/* Resolve security Mode */
				handle->nsp_address_ptr->omalw_server_security = (omalw_server_security_t)sn_nsdl_atoi(temp_ptr, length);
				path_temp[4] = '2';
				resource_temp.resource = temp_ptr;
				resource_temp.resourcelen = length;
				if(sn_nsdl_create_resource(handle, &resource_temp) != SN_NSDL_SUCCESS)
					return SN_NSDL_FAILURE;

				break;
			case 3:
				/* Public Key or Identity */
				path_temp[4] = '3';
				resource_temp.resource = temp_ptr;
				resource_temp.resourcelen = length;
				if(sn_nsdl_create_resource(handle, &resource_temp) != SN_NSDL_SUCCESS)
					return SN_NSDL_FAILURE;
				break;
			case 4:
				/* Server Public Key or Identity */;
				path_temp[4] = '4';
				resource_temp.resource = temp_ptr;
				resource_temp.resourcelen = length;
				if(sn_nsdl_create_resource(handle, &resource_temp) != SN_NSDL_SUCCESS)
					return SN_NSDL_FAILURE;

				break;
			case 5:
				/* Secret Key */
				path_temp[4] = '5';
				resource_temp.resource = temp_ptr;
				resource_temp.resourcelen = length;
				if(sn_nsdl_create_resource(handle, &resource_temp) != SN_NSDL_SUCCESS)
					return SN_NSDL_FAILURE;
				break;
			default:
				break;
			}

			/* Move pointer to next TLV message */
			temp_ptr += length;
		}
	}

	return SN_NSDL_SUCCESS;
}

static void sn_nsdl_check_oma_bs_status(struct nsdl_s *handle)
{
	/* Check OMA BS status */
	if((handle->nsp_address_ptr->omalw_server_security == PSK) && (handle->nsp_address_ptr->omalw_address_ptr->type != SN_NSDL_ADDRESS_TYPE_NONE))
	{
		/* call cb that oma bootstrap is done */
		handle->sn_nsdl_oma_bs_done_cb(handle->nsp_address_ptr);
	}
	else if((handle->nsp_address_ptr->omalw_server_security == CERTIFICATE) && (handle->nsp_address_ptr->omalw_address_ptr->type != SN_NSDL_ADDRESS_TYPE_NONE)&&
			((sn_nsdl_get_resource(handle, 5, (void*)"0/0/5") != 0) &&
			(sn_nsdl_get_resource(handle, 5, (void*)"0/0/4") != 0) &&
			(sn_nsdl_get_resource(handle, 5, (void*)"0/0/3") != 0)) )
	{
		handle->sn_nsdl_oma_bs_done_cb(handle->nsp_address_ptr);
	}
}

/* Wrapper */
sn_grs_resource_list_s *sn_nsdl_list_resource(struct nsdl_s *handle, uint16_t pathlen, uint8_t *path)
{
	/* Check parameters */
	if(handle == NULL)
		return NULL;

	return sn_grs_list_resource(handle->grs, pathlen, path);
}

void sn_nsdl_free_resource_list(struct nsdl_s *handle, sn_grs_resource_list_s *list)
{	/* Check parameters */
	if(handle == NULL)
		return;

	sn_grs_free_resource_list(handle->grs, list);
}

extern int8_t sn_nsdl_update_resource(struct nsdl_s *handle, sn_nsdl_resource_info_s *res)
{
	/* Check parameters */
	if(handle == NULL)
		return SN_NSDL_FAILURE;

	return sn_grs_update_resource(handle->grs, res);
}

extern int8_t sn_nsdl_send_coap_message(struct nsdl_s *handle, sn_nsdl_addr_s *address_ptr, sn_coap_hdr_s *coap_hdr_ptr)
{	/* Check parameters */
	if(handle == NULL)
		return SN_NSDL_FAILURE;

	return sn_grs_send_coap_message(handle->grs, address_ptr, coap_hdr_ptr);
}

extern int8_t sn_nsdl_create_resource(struct nsdl_s *handle, sn_nsdl_resource_info_s *res)
{
	/* Check parameters */
	if(handle == NULL)
		return SN_NSDL_FAILURE;

	return sn_grs_create_resource(handle->grs, res);
}

extern int8_t sn_nsdl_delete_resource(struct nsdl_s *handle, uint16_t pathlen, uint8_t *path)
{
	/* Check parameters */
	if(handle == NULL)
		return SN_NSDL_FAILURE;

	return sn_grs_delete_resource(handle->grs, pathlen, path);
}
extern const sn_nsdl_resource_info_s *sn_nsdl_get_first_resource(struct nsdl_s *handle)
{
	/* Check parameters */
	if(handle == NULL)
		return NULL;

	return sn_grs_get_first_resource(handle->grs);
}
extern const sn_nsdl_resource_info_s *sn_nsdl_get_next_resource(struct nsdl_s *handle, const sn_nsdl_resource_info_s *resource)
{
	/* Check parameters */
	if(handle == NULL)
		return NULL;

	return sn_grs_get_next_resource(handle->grs, resource);
}

extern sn_coap_hdr_s *sn_nsdl_build_response(struct nsdl_s *handle, sn_coap_hdr_s *coap_packet_ptr, uint8_t msg_code)
{
	return sn_coap_build_response(handle->grs->coap, coap_packet_ptr, msg_code);
}

extern void sn_nsdl_release_allocated_coap_msg_mem(struct nsdl_s *handle, sn_coap_hdr_s *freed_coap_msg_ptr)
{
	sn_coap_parser_release_allocated_coap_msg_mem(handle->grs->coap, freed_coap_msg_ptr);
}
