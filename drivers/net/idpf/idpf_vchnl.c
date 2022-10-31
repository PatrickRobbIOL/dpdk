/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <inttypes.h>
#include <rte_byteorder.h>
#include <rte_common.h>

#include <rte_debug.h>
#include <rte_atomic.h>
#include <rte_eal.h>
#include <rte_ether.h>
#include <ethdev_driver.h>
#include <ethdev_pci.h>
#include <rte_dev.h>

#include "idpf_ethdev.h"
#include "idpf_rxtx.h"

static int
idpf_vc_clean(struct idpf_adapter *adapter)
{
	struct idpf_ctlq_msg *q_msg[IDPF_CTLQ_LEN];
	uint16_t num_q_msg = IDPF_CTLQ_LEN;
	struct idpf_dma_mem *dma_mem;
	int err;
	uint32_t i;

	for (i = 0; i < 10; i++) {
		err = idpf_ctlq_clean_sq(adapter->hw.asq, &num_q_msg, q_msg);
		msleep(20);
		if (num_q_msg > 0)
			break;
	}
	if (err != 0)
		return err;

	/* Empty queue is not an error */
	for (i = 0; i < num_q_msg; i++) {
		dma_mem = q_msg[i]->ctx.indirect.payload;
		if (dma_mem != NULL) {
			idpf_free_dma_mem(&adapter->hw, dma_mem);
			rte_free(dma_mem);
		}
		rte_free(q_msg[i]);
	}

	return 0;
}

static int
idpf_send_vc_msg(struct idpf_adapter *adapter, enum virtchnl_ops op,
		 uint16_t msg_size, uint8_t *msg)
{
	struct idpf_ctlq_msg *ctlq_msg;
	struct idpf_dma_mem *dma_mem;
	int err;

	err = idpf_vc_clean(adapter);
	if (err != 0)
		goto err;

	ctlq_msg = rte_zmalloc(NULL, sizeof(struct idpf_ctlq_msg), 0);
	if (ctlq_msg == NULL) {
		err = -ENOMEM;
		goto err;
	}

	dma_mem = rte_zmalloc(NULL, sizeof(struct idpf_dma_mem), 0);
	if (dma_mem == NULL) {
		err = -ENOMEM;
		goto dma_mem_error;
	}

	dma_mem->size = IDPF_DFLT_MBX_BUF_SIZE;
	idpf_alloc_dma_mem(&adapter->hw, dma_mem, dma_mem->size);
	if (dma_mem->va == NULL) {
		err = -ENOMEM;
		goto dma_alloc_error;
	}

	memcpy(dma_mem->va, msg, msg_size);

	ctlq_msg->opcode = idpf_mbq_opc_send_msg_to_pf;
	ctlq_msg->func_id = 0;
	ctlq_msg->data_len = msg_size;
	ctlq_msg->cookie.mbx.chnl_opcode = op;
	ctlq_msg->cookie.mbx.chnl_retval = VIRTCHNL_STATUS_SUCCESS;
	ctlq_msg->ctx.indirect.payload = dma_mem;

	err = idpf_ctlq_send(&adapter->hw, adapter->hw.asq, 1, ctlq_msg);
	if (err != 0)
		goto send_error;

	return 0;

send_error:
	idpf_free_dma_mem(&adapter->hw, dma_mem);
dma_alloc_error:
	rte_free(dma_mem);
dma_mem_error:
	rte_free(ctlq_msg);
err:
	return err;
}

static enum idpf_vc_result
idpf_read_msg_from_cp(struct idpf_adapter *adapter, uint16_t buf_len,
		      uint8_t *buf)
{
	struct idpf_hw *hw = &adapter->hw;
	struct idpf_ctlq_msg ctlq_msg;
	struct idpf_dma_mem *dma_mem = NULL;
	enum idpf_vc_result result = IDPF_MSG_NON;
	enum virtchnl_ops opcode;
	uint16_t pending = 1;
	int ret;

	ret = idpf_ctlq_recv(hw->arq, &pending, &ctlq_msg);
	if (ret != 0) {
		PMD_DRV_LOG(DEBUG, "Can't read msg from AQ");
		if (ret != -ENOMSG)
			result = IDPF_MSG_ERR;
		return result;
	}

	rte_memcpy(buf, ctlq_msg.ctx.indirect.payload->va, buf_len);

	opcode = (enum virtchnl_ops)rte_le_to_cpu_32(ctlq_msg.cookie.mbx.chnl_opcode);
	adapter->cmd_retval =
		(enum virtchnl_status_code)rte_le_to_cpu_32(ctlq_msg.cookie.mbx.chnl_retval);

	PMD_DRV_LOG(DEBUG, "CQ from CP carries opcode %u, retval %d",
		    opcode, adapter->cmd_retval);

	if (opcode == VIRTCHNL2_OP_EVENT) {
		struct virtchnl2_event *ve =
			(struct virtchnl2_event *)ctlq_msg.ctx.indirect.payload->va;

		result = IDPF_MSG_SYS;
		switch (ve->event) {
		case VIRTCHNL2_EVENT_LINK_CHANGE:
			/* TBD */
			break;
		default:
			PMD_DRV_LOG(ERR, "%s: Unknown event %d from CP",
				    __func__, ve->event);
			break;
		}
	} else {
		/* async reply msg on command issued by pf previously */
		result = IDPF_MSG_CMD;
		if (opcode != adapter->pend_cmd) {
			PMD_DRV_LOG(WARNING, "command mismatch, expect %u, get %u",
				    adapter->pend_cmd, opcode);
			result = IDPF_MSG_ERR;
		}
	}

	if (ctlq_msg.data_len != 0)
		dma_mem = ctlq_msg.ctx.indirect.payload;
	else
		pending = 0;

	ret = idpf_ctlq_post_rx_buffs(hw, hw->arq, &pending, &dma_mem);
	if (ret != 0 && dma_mem != NULL)
		idpf_free_dma_mem(hw, dma_mem);

	return result;
}

#define MAX_TRY_TIMES 200
#define ASQ_DELAY_MS  10

int
idpf_read_one_msg(struct idpf_adapter *adapter, uint32_t ops, uint16_t buf_len,
		  uint8_t *buf)
{
	int err = 0;
	int i = 0;
	int ret;

	do {
		ret = idpf_read_msg_from_cp(adapter, buf_len, buf);
		if (ret == IDPF_MSG_CMD)
			break;
		rte_delay_ms(ASQ_DELAY_MS);
	} while (i++ < MAX_TRY_TIMES);
	if (i >= MAX_TRY_TIMES ||
	    adapter->cmd_retval != VIRTCHNL_STATUS_SUCCESS) {
		err = -EBUSY;
		PMD_DRV_LOG(ERR, "No response or return failure (%d) for cmd %d",
			    adapter->cmd_retval, ops);
	}

	return err;
}

static int
idpf_execute_vc_cmd(struct idpf_adapter *adapter, struct idpf_cmd_info *args)
{
	int err = 0;
	int i = 0;
	int ret;

	if (atomic_set_cmd(adapter, args->ops))
		return -EINVAL;

	ret = idpf_send_vc_msg(adapter, args->ops, args->in_args_size, args->in_args);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "fail to send cmd %d", args->ops);
		clear_cmd(adapter);
		return ret;
	}

	switch (args->ops) {
	case VIRTCHNL_OP_VERSION:
	case VIRTCHNL2_OP_GET_CAPS:
	case VIRTCHNL2_OP_CREATE_VPORT:
	case VIRTCHNL2_OP_DESTROY_VPORT:
	case VIRTCHNL2_OP_CONFIG_RX_QUEUES:
	case VIRTCHNL2_OP_CONFIG_TX_QUEUES:
	case VIRTCHNL2_OP_ENABLE_QUEUES:
	case VIRTCHNL2_OP_DISABLE_QUEUES:
	case VIRTCHNL2_OP_ENABLE_VPORT:
	case VIRTCHNL2_OP_DISABLE_VPORT:
		/* for init virtchnl ops, need to poll the response */
		err = idpf_read_one_msg(adapter, args->ops, args->out_size, args->out_buffer);
		clear_cmd(adapter);
		break;
	default:
		/* For other virtchnl ops in running time,
		 * wait for the cmd done flag.
		 */
		do {
			if (adapter->pend_cmd == VIRTCHNL_OP_UNKNOWN)
				break;
			rte_delay_ms(ASQ_DELAY_MS);
			/* If don't read msg or read sys event, continue */
		} while (i++ < MAX_TRY_TIMES);
		/* If there's no response is received, clear command */
		if (i >= MAX_TRY_TIMES  ||
		    adapter->cmd_retval != VIRTCHNL_STATUS_SUCCESS) {
			err = -EBUSY;
			PMD_DRV_LOG(ERR, "No response or return failure (%d) for cmd %d",
				    adapter->cmd_retval, args->ops);
			clear_cmd(adapter);
		}
		break;
	}

	return err;
}

int
idpf_vc_check_api_version(struct idpf_adapter *adapter)
{
	struct virtchnl2_version_info version, *pver;
	struct idpf_cmd_info args;
	int err;

	memset(&version, 0, sizeof(struct virtchnl_version_info));
	version.major = VIRTCHNL2_VERSION_MAJOR_2;
	version.minor = VIRTCHNL2_VERSION_MINOR_0;

	args.ops = VIRTCHNL_OP_VERSION;
	args.in_args = (uint8_t *)&version;
	args.in_args_size = sizeof(version);
	args.out_buffer = adapter->mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;

	err = idpf_execute_vc_cmd(adapter, &args);
	if (err != 0) {
		PMD_DRV_LOG(ERR,
			    "Failed to execute command of VIRTCHNL_OP_VERSION");
		return err;
	}

	pver = (struct virtchnl2_version_info *)args.out_buffer;
	adapter->virtchnl_version = *pver;

	if (adapter->virtchnl_version.major != VIRTCHNL2_VERSION_MAJOR_2 ||
	    adapter->virtchnl_version.minor != VIRTCHNL2_VERSION_MINOR_0) {
		PMD_INIT_LOG(ERR, "VIRTCHNL API version mismatch:(%u.%u)-(%u.%u)",
			     adapter->virtchnl_version.major,
			     adapter->virtchnl_version.minor,
			     VIRTCHNL2_VERSION_MAJOR_2,
			     VIRTCHNL2_VERSION_MINOR_0);
		return -EINVAL;
	}

	return 0;
}

int
idpf_vc_get_caps(struct idpf_adapter *adapter)
{
	struct virtchnl2_get_capabilities caps_msg;
	struct idpf_cmd_info args;
	int err;

	memset(&caps_msg, 0, sizeof(struct virtchnl2_get_capabilities));

	args.ops = VIRTCHNL2_OP_GET_CAPS;
	args.in_args = (uint8_t *)&caps_msg;
	args.in_args_size = sizeof(caps_msg);
	args.out_buffer = adapter->mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;

	err = idpf_execute_vc_cmd(adapter, &args);
	if (err != 0) {
		PMD_DRV_LOG(ERR,
			    "Failed to execute command of VIRTCHNL2_OP_GET_CAPS");
		return err;
	}

	rte_memcpy(adapter->caps, args.out_buffer, sizeof(caps_msg));

	return 0;
}

int
idpf_vc_create_vport(struct idpf_adapter *adapter)
{
	uint16_t idx = adapter->cur_vport_idx;
	struct virtchnl2_create_vport *vport_req_info =
		(struct virtchnl2_create_vport *)adapter->vport_req_info[idx];
	struct virtchnl2_create_vport vport_msg;
	struct idpf_cmd_info args;
	int err = -1;

	memset(&vport_msg, 0, sizeof(struct virtchnl2_create_vport));
	vport_msg.vport_type = vport_req_info->vport_type;
	vport_msg.txq_model = vport_req_info->txq_model;
	vport_msg.rxq_model = vport_req_info->rxq_model;
	vport_msg.num_tx_q = vport_req_info->num_tx_q;
	vport_msg.num_tx_complq = vport_req_info->num_tx_complq;
	vport_msg.num_rx_q = vport_req_info->num_rx_q;
	vport_msg.num_rx_bufq = vport_req_info->num_rx_bufq;

	memset(&args, 0, sizeof(args));
	args.ops = VIRTCHNL2_OP_CREATE_VPORT;
	args.in_args = (uint8_t *)&vport_msg;
	args.in_args_size = sizeof(vport_msg);
	args.out_buffer = adapter->mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;

	err = idpf_execute_vc_cmd(adapter, &args);
	if (err != 0) {
		PMD_DRV_LOG(ERR,
			    "Failed to execute command of VIRTCHNL2_OP_CREATE_VPORT");
		return err;
	}

	if (adapter->vport_recv_info[idx] == NULL) {
		adapter->vport_recv_info[idx] = rte_zmalloc(NULL,
						    IDPF_DFLT_MBX_BUF_SIZE, 0);
		if (adapter->vport_recv_info[idx] == NULL) {
			PMD_INIT_LOG(ERR, "Failed to alloc vport_recv_info.");
			return -ENOMEM;
		}
	}
	rte_memcpy(adapter->vport_recv_info[idx], args.out_buffer,
		   IDPF_DFLT_MBX_BUF_SIZE);
	return 0;
}

int
idpf_vc_destroy_vport(struct idpf_vport *vport)
{
	struct idpf_adapter *adapter = vport->adapter;
	struct virtchnl2_vport vc_vport;
	struct idpf_cmd_info args;
	int err;

	vc_vport.vport_id = vport->vport_id;

	memset(&args, 0, sizeof(args));
	args.ops = VIRTCHNL2_OP_DESTROY_VPORT;
	args.in_args = (uint8_t *)&vc_vport;
	args.in_args_size = sizeof(vc_vport);
	args.out_buffer = adapter->mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;

	err = idpf_execute_vc_cmd(adapter, &args);
	if (err != 0)
		PMD_DRV_LOG(ERR, "Failed to execute command of VIRTCHNL2_OP_DESTROY_VPORT");

	return err;
}

#define IDPF_RX_BUF_STRIDE		64
int
idpf_vc_config_rxqs(struct idpf_vport *vport)
{
	struct idpf_adapter *adapter = vport->adapter;
	struct idpf_rx_queue **rxq =
		(struct idpf_rx_queue **)vport->dev_data->rx_queues;
	struct virtchnl2_config_rx_queues *vc_rxqs = NULL;
	struct virtchnl2_rxq_info *rxq_info;
	struct idpf_cmd_info args;
	uint16_t total_qs, num_qs;
	int size, i, j;
	int err = 0;
	int k = 0;

	total_qs = vport->num_rx_q + vport->num_rx_bufq;
	while (total_qs) {
		if (total_qs > adapter->max_rxq_per_msg) {
			num_qs = adapter->max_rxq_per_msg;
			total_qs -= adapter->max_rxq_per_msg;
		} else {
			num_qs = total_qs;
			total_qs = 0;
		}

		size = sizeof(*vc_rxqs) + (num_qs - 1) *
			sizeof(struct virtchnl2_rxq_info);
		vc_rxqs = rte_zmalloc("cfg_rxqs", size, 0);
		if (vc_rxqs == NULL) {
			PMD_DRV_LOG(ERR, "Failed to allocate virtchnl2_config_rx_queues");
			err = -ENOMEM;
			break;
		}
		vc_rxqs->vport_id = vport->vport_id;
		vc_rxqs->num_qinfo = num_qs;
		if (vport->rxq_model == VIRTCHNL2_QUEUE_MODEL_SINGLE) {
			for (i = 0; i < num_qs; i++, k++) {
				rxq_info = &vc_rxqs->qinfo[i];
				rxq_info->dma_ring_addr = rxq[k]->rx_ring_phys_addr;
				rxq_info->type = VIRTCHNL2_QUEUE_TYPE_RX;
				rxq_info->queue_id = rxq[k]->queue_id;
				rxq_info->model = VIRTCHNL2_QUEUE_MODEL_SINGLE;
				rxq_info->data_buffer_size = rxq[k]->rx_buf_len;
				rxq_info->max_pkt_size = vport->max_pkt_len;

				rxq_info->desc_ids = VIRTCHNL2_RXDID_2_FLEX_SQ_NIC_M;
				rxq_info->qflags |= VIRTCHNL2_RX_DESC_SIZE_32BYTE;

				rxq_info->ring_len = rxq[k]->nb_rx_desc;
			}
		} else {
			for (i = 0; i < num_qs / 3; i++, k++) {
				/* Rx queue */
				rxq_info = &vc_rxqs->qinfo[i * 3];
				rxq_info->dma_ring_addr =
					rxq[k]->rx_ring_phys_addr;
				rxq_info->type = VIRTCHNL2_QUEUE_TYPE_RX;
				rxq_info->queue_id = rxq[k]->queue_id;
				rxq_info->model = VIRTCHNL2_QUEUE_MODEL_SPLIT;
				rxq_info->data_buffer_size = rxq[k]->rx_buf_len;
				rxq_info->max_pkt_size = vport->max_pkt_len;

				rxq_info->desc_ids = VIRTCHNL2_RXDID_2_FLEX_SPLITQ_M;
				rxq_info->qflags |= VIRTCHNL2_RX_DESC_SIZE_32BYTE;

				rxq_info->ring_len = rxq[k]->nb_rx_desc;
				rxq_info->rx_bufq1_id = rxq[k]->bufq1->queue_id;
				rxq_info->rx_bufq2_id = rxq[k]->bufq2->queue_id;
				rxq_info->rx_buffer_low_watermark = 64;

				/* Buffer queue */
				for (j = 1; j <= IDPF_RX_BUFQ_PER_GRP; j++) {
					struct idpf_rx_queue *bufq = j == 1 ?
						rxq[k]->bufq1 : rxq[k]->bufq2;
					rxq_info = &vc_rxqs->qinfo[i * 3 + j];
					rxq_info->dma_ring_addr =
						bufq->rx_ring_phys_addr;
					rxq_info->type =
						VIRTCHNL2_QUEUE_TYPE_RX_BUFFER;
					rxq_info->queue_id = bufq->queue_id;
					rxq_info->model = VIRTCHNL2_QUEUE_MODEL_SPLIT;
					rxq_info->data_buffer_size = bufq->rx_buf_len;
					rxq_info->desc_ids =
						VIRTCHNL2_RXDID_2_FLEX_SPLITQ_M;
					rxq_info->ring_len = bufq->nb_rx_desc;

					rxq_info->buffer_notif_stride =
						IDPF_RX_BUF_STRIDE;
					rxq_info->rx_buffer_low_watermark = 64;
				}
			}
		}
		memset(&args, 0, sizeof(args));
		args.ops = VIRTCHNL2_OP_CONFIG_RX_QUEUES;
		args.in_args = (uint8_t *)vc_rxqs;
		args.in_args_size = size;
		args.out_buffer = adapter->mbx_resp;
		args.out_size = IDPF_DFLT_MBX_BUF_SIZE;

		err = idpf_execute_vc_cmd(adapter, &args);
		rte_free(vc_rxqs);
		if (err != 0) {
			PMD_DRV_LOG(ERR, "Failed to execute command of VIRTCHNL2_OP_CONFIG_RX_QUEUES");
			break;
		}
	}

	return err;
}

int
idpf_vc_config_rxq(struct idpf_vport *vport, uint16_t rxq_id)
{
	struct idpf_adapter *adapter = vport->adapter;
	struct idpf_rx_queue **rxq =
		(struct idpf_rx_queue **)vport->dev_data->rx_queues;
	struct virtchnl2_config_rx_queues *vc_rxqs = NULL;
	struct virtchnl2_rxq_info *rxq_info;
	struct idpf_cmd_info args;
	uint16_t num_qs;
	int size, err, i;

	if (vport->rxq_model == VIRTCHNL2_QUEUE_MODEL_SINGLE)
		num_qs = IDPF_RXQ_PER_GRP;
	else
		num_qs = IDPF_RXQ_PER_GRP + IDPF_RX_BUFQ_PER_GRP;

	size = sizeof(*vc_rxqs) + (num_qs - 1) *
		sizeof(struct virtchnl2_rxq_info);
	vc_rxqs = rte_zmalloc("cfg_rxqs", size, 0);
	if (vc_rxqs == NULL) {
		PMD_DRV_LOG(ERR, "Failed to allocate virtchnl2_config_rx_queues");
		err = -ENOMEM;
		return err;
	}
	vc_rxqs->vport_id = vport->vport_id;
	vc_rxqs->num_qinfo = num_qs;
	if (vport->rxq_model == VIRTCHNL2_QUEUE_MODEL_SINGLE) {
		rxq_info = &vc_rxqs->qinfo[0];
		rxq_info->dma_ring_addr = rxq[rxq_id]->rx_ring_phys_addr;
		rxq_info->type = VIRTCHNL2_QUEUE_TYPE_RX;
		rxq_info->queue_id = rxq[rxq_id]->queue_id;
		rxq_info->model = VIRTCHNL2_QUEUE_MODEL_SINGLE;
		rxq_info->data_buffer_size = rxq[rxq_id]->rx_buf_len;
		rxq_info->max_pkt_size = vport->max_pkt_len;

		rxq_info->desc_ids = VIRTCHNL2_RXDID_2_FLEX_SQ_NIC_M;
		rxq_info->qflags |= VIRTCHNL2_RX_DESC_SIZE_32BYTE;

		rxq_info->ring_len = rxq[rxq_id]->nb_rx_desc;
	}  else {
		/* Rx queue */
		rxq_info = &vc_rxqs->qinfo[0];
		rxq_info->dma_ring_addr = rxq[rxq_id]->rx_ring_phys_addr;
		rxq_info->type = VIRTCHNL2_QUEUE_TYPE_RX;
		rxq_info->queue_id = rxq[rxq_id]->queue_id;
		rxq_info->model = VIRTCHNL2_QUEUE_MODEL_SPLIT;
		rxq_info->data_buffer_size = rxq[rxq_id]->rx_buf_len;
		rxq_info->max_pkt_size = vport->max_pkt_len;

		rxq_info->desc_ids = VIRTCHNL2_RXDID_2_FLEX_SPLITQ_M;
		rxq_info->qflags |= VIRTCHNL2_RX_DESC_SIZE_32BYTE;

		rxq_info->ring_len = rxq[rxq_id]->nb_rx_desc;
		rxq_info->rx_bufq1_id = rxq[rxq_id]->bufq1->queue_id;
		rxq_info->rx_bufq2_id = rxq[rxq_id]->bufq2->queue_id;
		rxq_info->rx_buffer_low_watermark = 64;

		/* Buffer queue */
		for (i = 1; i <= IDPF_RX_BUFQ_PER_GRP; i++) {
			struct idpf_rx_queue *bufq =
				i == 1 ? rxq[rxq_id]->bufq1 : rxq[rxq_id]->bufq2;
			rxq_info = &vc_rxqs->qinfo[i];
			rxq_info->dma_ring_addr = bufq->rx_ring_phys_addr;
			rxq_info->type = VIRTCHNL2_QUEUE_TYPE_RX_BUFFER;
			rxq_info->queue_id = bufq->queue_id;
			rxq_info->model = VIRTCHNL2_QUEUE_MODEL_SPLIT;
			rxq_info->data_buffer_size = bufq->rx_buf_len;
			rxq_info->desc_ids = VIRTCHNL2_RXDID_2_FLEX_SPLITQ_M;
			rxq_info->ring_len = bufq->nb_rx_desc;

			rxq_info->buffer_notif_stride = IDPF_RX_BUF_STRIDE;
			rxq_info->rx_buffer_low_watermark = 64;
		}
	}

	memset(&args, 0, sizeof(args));
	args.ops = VIRTCHNL2_OP_CONFIG_RX_QUEUES;
	args.in_args = (uint8_t *)vc_rxqs;
	args.in_args_size = size;
	args.out_buffer = adapter->mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;

	err = idpf_execute_vc_cmd(adapter, &args);
	rte_free(vc_rxqs);
	if (err != 0)
		PMD_DRV_LOG(ERR, "Failed to execute command of VIRTCHNL2_OP_CONFIG_RX_QUEUES");

	return err;
}

int
idpf_vc_config_txqs(struct idpf_vport *vport)
{
	struct idpf_adapter *adapter = vport->adapter;
	struct idpf_tx_queue **txq =
		(struct idpf_tx_queue **)vport->dev_data->tx_queues;
	struct virtchnl2_config_tx_queues *vc_txqs = NULL;
	struct virtchnl2_txq_info *txq_info;
	struct idpf_cmd_info args;
	uint16_t total_qs, num_qs;
	int size, i;
	int err = 0;
	int k = 0;

	total_qs = vport->num_tx_q + vport->num_tx_complq;
	while (total_qs) {
		if (total_qs > adapter->max_txq_per_msg) {
			num_qs = adapter->max_txq_per_msg;
			total_qs -= adapter->max_txq_per_msg;
		} else {
			num_qs = total_qs;
			total_qs = 0;
		}
		size = sizeof(*vc_txqs) + (num_qs - 1) *
			sizeof(struct virtchnl2_txq_info);
		vc_txqs = rte_zmalloc("cfg_txqs", size, 0);
		if (vc_txqs == NULL) {
			PMD_DRV_LOG(ERR, "Failed to allocate virtchnl2_config_tx_queues");
			err = -ENOMEM;
			break;
		}
		vc_txqs->vport_id = vport->vport_id;
		vc_txqs->num_qinfo = num_qs;
		if (vport->txq_model == VIRTCHNL2_QUEUE_MODEL_SINGLE) {
			for (i = 0; i < num_qs; i++, k++) {
				txq_info = &vc_txqs->qinfo[i];
				txq_info->dma_ring_addr = txq[k]->tx_ring_phys_addr;
				txq_info->type = VIRTCHNL2_QUEUE_TYPE_TX;
				txq_info->queue_id = txq[k]->queue_id;
				txq_info->model = VIRTCHNL2_QUEUE_MODEL_SINGLE;
				txq_info->sched_mode = VIRTCHNL2_TXQ_SCHED_MODE_QUEUE;
				txq_info->ring_len = txq[k]->nb_tx_desc;
			}
		} else {
			for (i = 0; i < num_qs / 2; i++, k++) {
				/* txq info */
				txq_info = &vc_txqs->qinfo[2 * i];
				txq_info->dma_ring_addr = txq[k]->tx_ring_phys_addr;
				txq_info->type = VIRTCHNL2_QUEUE_TYPE_TX;
				txq_info->queue_id = txq[k]->queue_id;
				txq_info->model = VIRTCHNL2_QUEUE_MODEL_SPLIT;
				txq_info->sched_mode = VIRTCHNL2_TXQ_SCHED_MODE_FLOW;
				txq_info->ring_len = txq[k]->nb_tx_desc;
				txq_info->tx_compl_queue_id =
					txq[k]->complq->queue_id;
				txq_info->relative_queue_id = txq_info->queue_id;

				/* tx completion queue info */
				txq_info = &vc_txqs->qinfo[2 * i + 1];
				txq_info->dma_ring_addr =
					txq[k]->complq->tx_ring_phys_addr;
				txq_info->type = VIRTCHNL2_QUEUE_TYPE_TX_COMPLETION;
				txq_info->queue_id = txq[k]->complq->queue_id;
				txq_info->model = VIRTCHNL2_QUEUE_MODEL_SPLIT;
				txq_info->sched_mode = VIRTCHNL2_TXQ_SCHED_MODE_FLOW;
				txq_info->ring_len = txq[k]->complq->nb_tx_desc;
			}
		}

		memset(&args, 0, sizeof(args));
		args.ops = VIRTCHNL2_OP_CONFIG_TX_QUEUES;
		args.in_args = (uint8_t *)vc_txqs;
		args.in_args_size = size;
		args.out_buffer = adapter->mbx_resp;
		args.out_size = IDPF_DFLT_MBX_BUF_SIZE;

		err = idpf_execute_vc_cmd(adapter, &args);
		rte_free(vc_txqs);
		if (err != 0) {
			PMD_DRV_LOG(ERR, "Failed to execute command of VIRTCHNL2_OP_CONFIG_TX_QUEUES");
			break;
		}
	}

	return err;
}

int
idpf_vc_config_txq(struct idpf_vport *vport, uint16_t txq_id)
{
	struct idpf_adapter *adapter = vport->adapter;
	struct idpf_tx_queue **txq =
		(struct idpf_tx_queue **)vport->dev_data->tx_queues;
	struct virtchnl2_config_tx_queues *vc_txqs = NULL;
	struct virtchnl2_txq_info *txq_info;
	struct idpf_cmd_info args;
	uint16_t num_qs;
	int size, err;

	if (vport->txq_model == VIRTCHNL2_QUEUE_MODEL_SINGLE)
		num_qs = IDPF_TXQ_PER_GRP;
	else
		num_qs = IDPF_TXQ_PER_GRP + IDPF_TX_COMPLQ_PER_GRP;

	size = sizeof(*vc_txqs) + (num_qs - 1) *
		sizeof(struct virtchnl2_txq_info);
	vc_txqs = rte_zmalloc("cfg_txqs", size, 0);
	if (vc_txqs == NULL) {
		PMD_DRV_LOG(ERR, "Failed to allocate virtchnl2_config_tx_queues");
		err = -ENOMEM;
		return err;
	}
	vc_txqs->vport_id = vport->vport_id;
	vc_txqs->num_qinfo = num_qs;

	if (vport->txq_model == VIRTCHNL2_QUEUE_MODEL_SINGLE) {
		txq_info = &vc_txqs->qinfo[0];
		txq_info->dma_ring_addr = txq[txq_id]->tx_ring_phys_addr;
		txq_info->type = VIRTCHNL2_QUEUE_TYPE_TX;
		txq_info->queue_id = txq[txq_id]->queue_id;
		txq_info->model = VIRTCHNL2_QUEUE_MODEL_SINGLE;
		txq_info->sched_mode = VIRTCHNL2_TXQ_SCHED_MODE_QUEUE;
		txq_info->ring_len = txq[txq_id]->nb_tx_desc;
	} else {
		/* txq info */
		txq_info = &vc_txqs->qinfo[0];
		txq_info->dma_ring_addr = txq[txq_id]->tx_ring_phys_addr;
		txq_info->type = VIRTCHNL2_QUEUE_TYPE_TX;
		txq_info->queue_id = txq[txq_id]->queue_id;
		txq_info->model = VIRTCHNL2_QUEUE_MODEL_SPLIT;
		txq_info->sched_mode = VIRTCHNL2_TXQ_SCHED_MODE_FLOW;
		txq_info->ring_len = txq[txq_id]->nb_tx_desc;
		txq_info->tx_compl_queue_id = txq[txq_id]->complq->queue_id;
		txq_info->relative_queue_id = txq_info->queue_id;

		/* tx completion queue info */
		txq_info = &vc_txqs->qinfo[1];
		txq_info->dma_ring_addr = txq[txq_id]->complq->tx_ring_phys_addr;
		txq_info->type = VIRTCHNL2_QUEUE_TYPE_TX_COMPLETION;
		txq_info->queue_id = txq[txq_id]->complq->queue_id;
		txq_info->model = VIRTCHNL2_QUEUE_MODEL_SPLIT;
		txq_info->sched_mode = VIRTCHNL2_TXQ_SCHED_MODE_FLOW;
		txq_info->ring_len = txq[txq_id]->complq->nb_tx_desc;
	}

	memset(&args, 0, sizeof(args));
	args.ops = VIRTCHNL2_OP_CONFIG_TX_QUEUES;
	args.in_args = (uint8_t *)vc_txqs;
	args.in_args_size = size;
	args.out_buffer = adapter->mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;

	err = idpf_execute_vc_cmd(adapter, &args);
	rte_free(vc_txqs);
	if (err != 0)
		PMD_DRV_LOG(ERR, "Failed to execute command of VIRTCHNL2_OP_CONFIG_TX_QUEUES");

	return err;
}

static int
idpf_vc_ena_dis_one_queue(struct idpf_vport *vport, uint16_t qid,
			  uint32_t type, bool on)
{
	struct idpf_adapter *adapter = vport->adapter;
	struct virtchnl2_del_ena_dis_queues *queue_select;
	struct virtchnl2_queue_chunk *queue_chunk;
	struct idpf_cmd_info args;
	int err, len;

	len = sizeof(struct virtchnl2_del_ena_dis_queues);
	queue_select = rte_zmalloc("queue_select", len, 0);
	if (queue_select == NULL)
		return -ENOMEM;

	queue_chunk = queue_select->chunks.chunks;
	queue_select->chunks.num_chunks = 1;
	queue_select->vport_id = vport->vport_id;

	queue_chunk->type = type;
	queue_chunk->start_queue_id = qid;
	queue_chunk->num_queues = 1;

	args.ops = on ? VIRTCHNL2_OP_ENABLE_QUEUES :
		VIRTCHNL2_OP_DISABLE_QUEUES;
	args.in_args = (u8 *)queue_select;
	args.in_args_size = len;
	args.out_buffer = adapter->mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;
	err = idpf_execute_vc_cmd(adapter, &args);
	if (err != 0)
		PMD_DRV_LOG(ERR, "Failed to execute command of VIRTCHNL2_OP_%s_QUEUES",
			    on ? "ENABLE" : "DISABLE");

	rte_free(queue_select);
	return err;
}

int
idpf_switch_queue(struct idpf_vport *vport, uint16_t qid,
		     bool rx, bool on)
{
	uint32_t type;
	int err, queue_id;

	/* switch txq/rxq */
	type = rx ? VIRTCHNL2_QUEUE_TYPE_RX : VIRTCHNL2_QUEUE_TYPE_TX;

	if (type == VIRTCHNL2_QUEUE_TYPE_RX)
		queue_id = vport->chunks_info.rx_start_qid + qid;
	else
		queue_id = vport->chunks_info.tx_start_qid + qid;
	err = idpf_vc_ena_dis_one_queue(vport, queue_id, type, on);
	if (err != 0)
		return err;

	/* switch tx completion queue */
	if (!rx && vport->txq_model == VIRTCHNL2_QUEUE_MODEL_SPLIT) {
		type = VIRTCHNL2_QUEUE_TYPE_TX_COMPLETION;
		queue_id = vport->chunks_info.tx_compl_start_qid + qid;
		err = idpf_vc_ena_dis_one_queue(vport, queue_id, type, on);
		if (err != 0)
			return err;
	}

	/* switch rx buffer queue */
	if (rx && vport->rxq_model == VIRTCHNL2_QUEUE_MODEL_SPLIT) {
		type = VIRTCHNL2_QUEUE_TYPE_RX_BUFFER;
		queue_id = vport->chunks_info.rx_buf_start_qid + 2 * qid;
		err = idpf_vc_ena_dis_one_queue(vport, queue_id, type, on);
		if (err != 0)
			return err;
		queue_id++;
		err = idpf_vc_ena_dis_one_queue(vport, queue_id, type, on);
		if (err != 0)
			return err;
	}

	return err;
}

int
idpf_vc_ena_dis_vport(struct idpf_vport *vport, bool enable)
{
	struct idpf_adapter *adapter = vport->adapter;
	struct virtchnl2_vport vc_vport;
	struct idpf_cmd_info args;
	int err;

	vc_vport.vport_id = vport->vport_id;
	args.ops = enable ? VIRTCHNL2_OP_ENABLE_VPORT :
			    VIRTCHNL2_OP_DISABLE_VPORT;
	args.in_args = (uint8_t *)&vc_vport;
	args.in_args_size = sizeof(vc_vport);
	args.out_buffer = adapter->mbx_resp;
	args.out_size = IDPF_DFLT_MBX_BUF_SIZE;

	err = idpf_execute_vc_cmd(adapter, &args);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Failed to execute command of VIRTCHNL2_OP_%s_VPORT",
			    enable ? "ENABLE" : "DISABLE");
	}

	return err;
}
