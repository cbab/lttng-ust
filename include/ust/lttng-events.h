#ifndef _UST_LTTNG_EVENTS_H
#define _UST_LTTNG_EVENTS_H

/*
 * ust/lttng-events.h
 *
 * Copyright 2010 (c) - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * Holds LTTng per-session event registry.
 *
 * Dual LGPL v2.1/GPL v2 license.
 */

#include <urcu/list.h>
#include <uuid/uuid.h>
#include <stdint.h>
#include <ust/lttng-ust-abi.h>

#undef is_signed_type
#define is_signed_type(type)		(((type)(-1)) < 0)

struct ltt_channel;
struct ltt_session;
struct lib_ring_buffer_ctx;

/* Type description */

/* Update the astract_types name table in lttng-types.c along with this enum */
enum abstract_types {
	atype_integer,
	atype_enum,
	atype_array,
	atype_sequence,
	atype_string,
	NR_ABSTRACT_TYPES,
};

/* Update the string_encodings name table in lttng-types.c along with this enum */
enum lttng_string_encodings {
	lttng_encode_none = 0,
	lttng_encode_UTF8 = 1,
	lttng_encode_ASCII = 2,
	NR_STRING_ENCODINGS,
};

struct lttng_enum_entry {
	unsigned long long start, end;	/* start and end are inclusive */
	const char *string;
};

#define __type_integer(_type, _byte_order, _base, _encoding)	\
	{							\
	    .atype = atype_integer,				\
	    .u.basic.integer =					\
		{						\
		  .size = sizeof(_type) * CHAR_BIT,		\
		  .alignment = ltt_alignof(_type) * CHAR_BIT,	\
		  .signedness = is_signed_type(_type),		\
		  .reverse_byte_order = _byte_order != __BYTE_ORDER,	\
		  .base = _base,				\
		  .encoding = lttng_encode_##_encoding,		\
		},						\
	}							\

struct lttng_integer_type {
	unsigned int size;		/* in bits */
	unsigned short alignment;	/* in bits */
	unsigned int signedness:1;
	unsigned int reverse_byte_order:1;
	unsigned int base;		/* 2, 8, 10, 16, for pretty print */
	enum lttng_string_encodings encoding;
};

union _lttng_basic_type {
	struct lttng_integer_type integer;
	struct {
		const char *name;
	} enumeration;
	struct {
		enum lttng_string_encodings encoding;
	} string;
};

struct lttng_basic_type {
	enum abstract_types atype;
	union {
		union _lttng_basic_type basic;
	} u;
};

struct lttng_type {
	enum abstract_types atype;
	union {
		union _lttng_basic_type basic;
		struct {
			struct lttng_basic_type elem_type;
			unsigned int length;		/* num. elems. */
		} array;
		struct {
			struct lttng_basic_type length_type;
			struct lttng_basic_type elem_type;
		} sequence;
	} u;
};

struct lttng_enum {
	const char *name;
	struct lttng_type container_type;
	const struct lttng_enum_entry *entries;
	unsigned int len;
};

/* Event field description */

struct lttng_event_field {
	const char *name;
	struct lttng_type type;
};

struct lttng_ctx_field {
	struct lttng_event_field event_field;
	size_t (*get_size)(size_t offset);
	void (*record)(struct lttng_ctx_field *field,
		       struct lib_ring_buffer_ctx *ctx,
		       struct ltt_channel *chan);
	union {
	} u;
	void (*destroy)(struct lttng_ctx_field *field);
};

struct lttng_ctx {
	struct lttng_ctx_field *fields;
	unsigned int nr_fields;
	unsigned int allocated_fields;
};

struct lttng_event_desc {
	const char *name;
	void *probe_callback;
	const struct lttng_event_ctx *ctx;	/* context */
	const struct lttng_event_field *fields;	/* event payload */
	unsigned int nr_fields;
};

struct lttng_probe_desc {
	const struct lttng_event_desc *event_desc;
	unsigned int nr_events;
	struct cds_list_head head;		/* chain registered probes */
};

/*
 * ltt_event structure is referred to by the tracing fast path. It must be
 * kept small.
 */
struct ltt_event {
	unsigned int id;
	struct ltt_channel *chan;
	int enabled;
	const struct lttng_event_desc *desc;
	void *filter;
	struct lttng_ctx *ctx;
	enum lttng_ust_instrumentation instrumentation;
	union {
	} u;
	struct cds_list_head list;		/* Event list */
	int metadata_dumped:1;
};

struct ltt_channel_ops {
	struct channel *(*channel_create)(const char *name,
				struct ltt_channel *ltt_chan,
				void *buf_addr,
				size_t subbuf_size, size_t num_subbuf,
				unsigned int switch_timer_interval,
				unsigned int read_timer_interval,
				int *shmid);
	void (*channel_destroy)(struct channel *chan);
	struct lib_ring_buffer *(*buffer_read_open)(struct channel *chan);
	void (*buffer_read_close)(struct lib_ring_buffer *buf);
	int (*event_reserve)(struct lib_ring_buffer_ctx *ctx,
			     uint32_t event_id);
	void (*event_commit)(struct lib_ring_buffer_ctx *ctx);
	void (*event_write)(struct lib_ring_buffer_ctx *ctx, const void *src,
			    size_t len);
	/*
	 * packet_avail_size returns the available size in the current
	 * packet. Note that the size returned is only a hint, since it
	 * may change due to concurrent writes.
	 */
	size_t (*packet_avail_size)(struct channel *chan);
	//wait_queue_head_t *(*get_reader_wait_queue)(struct channel *chan);
	//wait_queue_head_t *(*get_hp_wait_queue)(struct channel *chan);
	int (*is_finalized)(struct channel *chan);
	int (*is_disabled)(struct channel *chan);
};

struct ltt_channel {
	unsigned int id;
	struct channel *chan;		/* Channel buffers */
	int enabled;
	struct lttng_ctx *ctx;
	/* Event ID management */
	struct ltt_session *session;
	int objd;			/* Object associated to channel */
	unsigned int free_event_id;	/* Next event ID to allocate */
	struct cds_list_head list;	/* Channel list */
	struct ltt_channel_ops *ops;
	int header_type;		/* 0: unset, 1: compact, 2: large */
	int shmid;			/* shared memory ID */
	int metadata_dumped:1;
};

struct ltt_session {
	int active;			/* Is trace session active ? */
	int been_active;		/* Has trace session been active ? */
	int objd;			/* Object associated to session */
	struct ltt_channel *metadata;	/* Metadata channel */
	struct cds_list_head chan;	/* Channel list head */
	struct cds_list_head events;	/* Event list head */
	struct cds_list_head list;	/* Session list */
	unsigned int free_chan_id;	/* Next chan ID to allocate */
	uuid_t uuid;			/* Trace session unique ID */
	int metadata_dumped:1;
};

struct ltt_transport {
	char *name;
	struct cds_list_head node;
	struct ltt_channel_ops ops;
};

struct ltt_session *ltt_session_create(void);
int ltt_session_enable(struct ltt_session *session);
int ltt_session_disable(struct ltt_session *session);
void ltt_session_destroy(struct ltt_session *session);

struct ltt_channel *ltt_channel_create(struct ltt_session *session,
				       const char *transport_name,
				       void *buf_addr,
				       size_t subbuf_size, size_t num_subbuf,
				       unsigned int switch_timer_interval,
				       unsigned int read_timer_interval);
struct ltt_channel *ltt_global_channel_create(struct ltt_session *session,
				       int overwrite, void *buf_addr,
				       size_t subbuf_size, size_t num_subbuf,
				       unsigned int switch_timer_interval,
				       unsigned int read_timer_interval);

struct ltt_event *ltt_event_create(struct ltt_channel *chan,
				   struct lttng_ust_event *event_param,
				   void *filter);

int ltt_channel_enable(struct ltt_channel *channel);
int ltt_channel_disable(struct ltt_channel *channel);
int ltt_event_enable(struct ltt_event *event);
int ltt_event_disable(struct ltt_event *event);

void ltt_transport_register(struct ltt_transport *transport);
void ltt_transport_unregister(struct ltt_transport *transport);

void synchronize_trace(void);
//int ltt_debugfs_abi_init(void);
//void ltt_debugfs_abi_exit(void);

int ltt_probe_register(struct lttng_probe_desc *desc);
void ltt_probe_unregister(struct lttng_probe_desc *desc);
const struct lttng_event_desc *ltt_event_get(const char *name);
void ltt_event_put(const struct lttng_event_desc *desc);
int ltt_probes_init(void);
void ltt_probes_exit(void);
struct lttng_ctx_field *lttng_append_context(struct lttng_ctx **ctx);
void lttng_remove_context_field(struct lttng_ctx **ctx,
				struct lttng_ctx_field *field);
void lttng_destroy_context(struct lttng_ctx *ctx);
int lttng_add_vtid_to_ctx(struct lttng_ctx **ctx);

//extern const struct file_operations lttng_tracepoint_list_fops;

#endif /* _UST_LTTNG_EVENTS_H */