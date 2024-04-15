/*-------------------------------------------------------------------------
 *
 * pg_show_vm.c
 *		Show visibility map
 *
 * Copyright (c) 2008-2024, PostgreSQL Global Development Group
 * Copyright (c) 2024, Hironobu Suzuki @ interdb.jp
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"
#include <unistd.h>
#include <dlfcn.h>

#include "access/visibilitymapdefs.h"
#include "access/visibilitymap.h"
#include "access/relation.h"
#include "access/tupdesc.h"
#include "catalog/pg_authid_d.h"
#include "catalog/pg_class.h"
#include "catalog/pg_inherits.h"
#include "catalog/namespace.h"
#include "utils/acl.h"
#include "utils/syscache.h"
#include "utils/relcache.h"
#include "utils/builtins.h"
#include "storage/lockdefs.h"
#include "storage/bufmgr.h"
#include "funcapi.h"
#include "tcop/utility.h"
#include "pgstat.h"

PG_MODULE_MAGIC;

/* Function declarations */
void		_PG_init(void);
void		_PG_fini(void);

Datum		pg_show_rel_vm(PG_FUNCTION_ARGS);
Datum		pg_show_vm(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(pg_show_rel_vm);
PG_FUNCTION_INFO_V1(pg_show_vm);

static void get_values(const Oid relid, const LOCKMODE mode,
					   BlockNumber *relpages, BlockNumber *all_visible,
					   BlockNumber *all_frozen, List **indexoidlist);
static void set_rel_values(Tuplestorestate *tupstore, TupleDesc tupdesc,
						   const Oid relid, const BlockNumber relpages,
						   const BlockNumber all_visible, const BlockNumber all_frozen,
						   const int type);
static void set_index_values(Tuplestorestate *tupstore, TupleDesc tupdesc, List *indexoidlist,
							 const int type, const LOCKMODE mode);
static void set_data(Tuplestorestate *tupstore, TupleDesc tupdesc,
					 const Oid relid, const LOCKMODE mode, const int type, const bool index);
static Oid	get_relation_oid(const char *relname);
static Datum show_vm(PG_FUNCTION_ARGS, const Oid relid, const bool index, const bool partition);


/* Module callback */
void
_PG_init(void)
{
	if (!process_shared_preload_libraries_in_progress)
		return;

	EmitWarningsOnPlaceholders("pg_show_vm");
}

void
_PG_fini(void)
{
	;
}

static void
get_values(const Oid relid, const LOCKMODE mode,
		   BlockNumber *relpages, BlockNumber *all_visible,
		   BlockNumber *all_frozen, List **indexoidlist)
{
	Relation	rel;

	rel = relation_open(relid, mode);
	*relpages = RelationGetNumberOfBlocks(rel);
	visibilitymap_count(rel, all_visible, all_frozen);
	*indexoidlist = RelationGetIndexList(rel);
	relation_close(rel, mode);
}


#define PG_SHOW_VM_COLS		 5

/*--
 * values[0] : oid
 * values[1] : relpages
 * values[2] : all_visible
 * values[3] : all_frozen
 * values[4] : type
 */

#define type_rel 0
#define type_idx (type_rel + 1)
#define type_partition 2
#define type_partition_idx (type_partition + 1)


static void
set_rel_values(Tuplestorestate *tupstore, TupleDesc tupdesc,
			   const Oid relid, const BlockNumber relpages,
			   const BlockNumber all_visible, const BlockNumber all_frozen,
			   const int type)
{
	int			i;

	Datum		values[PG_SHOW_VM_COLS];
	bool		nulls[PG_SHOW_VM_COLS];

	memset(values, 0, sizeof(values));
	memset(nulls, false, sizeof(nulls));

	i = 0;
	values[i++] = ObjectIdGetDatum(relid);
	values[i++] = ObjectIdGetDatum(relpages);
	values[i++] = ObjectIdGetDatum(all_visible);
	values[i++] = ObjectIdGetDatum(all_frozen);
	values[i++] = ObjectIdGetDatum(type);

	tuplestore_putvalues(tupstore, tupdesc, values, nulls);
}


static void
set_index_values(Tuplestorestate *tupstore, TupleDesc tupdesc, List *indexoidlist,
				 const int type, const LOCKMODE mode)
{
	ListCell   *indexoidscan;

	foreach(indexoidscan, indexoidlist)
	{
		Oid			indexoid;
		Relation	indrel;
		BlockNumber relpages;
		BlockNumber all_visible;
		BlockNumber all_frozen;

		indexoid = lfirst_oid(indexoidscan);

		indrel = index_open(indexoid, mode);
		relpages = RelationGetNumberOfBlocks(indrel);
		visibilitymap_count(indrel, &all_visible, &all_frozen);
		index_close(indrel, mode);

		set_rel_values(tupstore, tupdesc, indexoid, relpages, all_visible, all_frozen, type);
	}
}

static void
set_data(Tuplestorestate *tupstore, TupleDesc tupdesc, const Oid relid,
		 const LOCKMODE mode, const int type, const bool index)
{

	BlockNumber relpages;
	BlockNumber all_visible;
	BlockNumber all_frozen;
	List	   *indexoidlist;

	get_values(relid, mode, &relpages, &all_visible, &all_frozen, &indexoidlist);

	/* relation */
	set_rel_values(tupstore, tupdesc, relid, relpages, all_visible, all_frozen, type);

	/* indexes */
	if ((index == true) && (list_length(indexoidlist) > 0))
		set_index_values(tupstore, tupdesc, indexoidlist, (type + 1), mode);
}

static Oid
get_relation_oid(const char *relname)
{
	return RelnameGetRelid(relname);
}

static Datum
show_vm(PG_FUNCTION_ARGS, const Oid relid, const bool index, const bool partition)
{
	ReturnSetInfo *rsinfo = (ReturnSetInfo *) fcinfo->resultinfo;
	TupleDesc	tupdesc;
	Tuplestorestate *tupstore;
	MemoryContext per_query_ctx;
	MemoryContext oldcontext;
	LOCKMODE	mode;
	HeapTuple	tuple;
	Form_pg_class classForm;
	bool		include_parts;

	if (!is_member_of_role(GetUserId(), ROLE_PG_MONITOR))
		elog(ERROR, "This function requires ROLE_PG_MONITOR privilege.");

	/* check to see if caller supports us returning a tuplestore */
	if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("set-valued function called in context that cannot accept a set")));
	if (!(rsinfo->allowedModes & SFRM_Materialize))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("materialize mode required, but it is not " \
						"allowed in this context")));

	/* Switch into long-lived context to construct returned data structures */
	per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;
	oldcontext = MemoryContextSwitchTo(per_query_ctx);

	/* Build a tuple descriptor for our result type */
	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		elog(ERROR, "return type must be a row type");

	tupstore = tuplestore_begin_heap(true, false, work_mem);
	rsinfo->returnMode = SFRM_Materialize;
	rsinfo->setResult = tupstore;
	rsinfo->setDesc = tupdesc;

	MemoryContextSwitchTo(oldcontext);

	mode = AccessShareLock;

	/* Set relation's data */
	set_data(tupstore, tupdesc, relid, mode, type_rel, index);

	if (partition != true)
		return (Datum) 0;

	/* Set partitions' data */
	tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
	if (!HeapTupleIsValid(tuple))
		elog(ERROR, "cache lookup failed for relation %u", relid);
	classForm = (Form_pg_class) GETSTRUCT(tuple);
	include_parts = (classForm->relkind == RELKIND_PARTITIONED_TABLE);
	ReleaseSysCache(tuple);

	if (include_parts)
	{
		List	   *part_oids = find_all_inheritors(relid, NoLock, NULL);
		ListCell   *part_lc;

		foreach(part_lc, part_oids)
		{
			Oid			part_oid = lfirst_oid(part_lc);

			if (part_oid == relid)
				continue;		/* ignore original table */
			/* Set partition's data */
			set_data(tupstore, tupdesc, part_oid, mode, type_partition, index);
		}
	}

	return (Datum) 0;
}


Datum
pg_show_rel_vm(PG_FUNCTION_ARGS)
{
	char	   *relname;
	Oid			relid;
	bool		index;
	bool		partition;

	relname = text_to_cstring(PG_GETARG_TEXT_PP(0));
	relid = get_relation_oid(relname);
	index = PG_GETARG_BOOL(1);
	partition = PG_GETARG_BOOL(2);
	return show_vm(fcinfo, relid, index, partition);
}

Datum
pg_show_vm(PG_FUNCTION_ARGS)
{
	return show_vm(fcinfo, PG_GETARG_OID(0), true, true);
}
