/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#include "rscatalog.h"
#include "rsexecute.h"
#include "rsutil.h"

#define MAX_CATALOG_QUERY_LEN (16*1024)
#define MAX_CATALOG_QUERY_FILTER_LEN (4*1024)

#define LIKE_OP "LIKE"
#define EQUAL_OP "="

// Universal (local+external), local , and external schema indicators.
#define NO_SCHEMA_UNIVERSAL_QUERY	0
#define LOCAL_SCHEMA_QUERY			1
#define EXTERNAL_SCHEMA_QUERY		2


#define MAX_TYPES 21

#define SQLTABLES_BASE_QUERY " SELECT CAST(current_database() AS VARCHAR(124)) as TABLE_CAT,n.nspname as TABLE_SCHEM,c.relname as TABLE_NAME," \
                                 " CASE n.nspname ~ '^pg_' OR n.nspname = 'information_schema' " \
                                     " WHEN true THEN CASE " \
                                     " WHEN n.nspname = 'pg_catalog' OR n.nspname = 'information_schema' THEN CASE c.relkind " \
                                     "  WHEN 'r' THEN 'SYSTEM TABLE' " \
                                     "  WHEN 'v' THEN 'SYSTEM VIEW' " \
                                     "  ELSE NULL " \
                                     "  END " \
                                     " WHEN n.nspname = 'pg_toast' THEN CASE c.relkind " \
                                     "  WHEN 'r' THEN 'SYSTEM TOAST TABLE' " \
                                     "  ELSE NULL " \
                                     "  END " \
                                     " ELSE CASE c.relkind " \
                                     "  WHEN 'r' THEN 'TEMPORARY TABLE' " \
                                     "  WHEN 'v' THEN 'TEMPORARY VIEW' " \
                                     "  ELSE NULL " \
                                     "  END " \
                                     " END " \
                                     " WHEN false THEN CASE c.relkind " \
                                     " WHEN 'r' THEN 'TABLE' " \
                                     " WHEN 'v' THEN 'VIEW' " \
									 " WHEN 'f' THEN 'FOREIGN TABLE' " \
									 " WHEN 'm' THEN 'MATERIALIZED VIEW' " \
                                     " ELSE NULL " \
                                     " END " \
                                     " ELSE NULL " \
                                     " END  as TABLE_TYPE, " \
                             " d.description as REMARKS " \
                             " FROM pg_namespace n,pg_class c " \
							 " LEFT JOIN pg_description d ON (c.oid=d.objoid AND d.objsubid=0) " \
							 " LEFT JOIN pg_catalog.pg_class dc ON (d.classoid=dc.oid AND dc.relname='pg_class') " \
							 " LEFT JOIN pg_catalog.pg_namespace dn ON (dn.oid=dc.relnamespace AND dn.nspname='pg_catalog') " \
                             " WHERE n.oid=c.relnamespace " 

#define SQLTABLES_ALL_SCHEMAS_QUERY " SELECT DISTINCT current_database() as TABLE_CAT,n.nspname as TABLE_SCHEM,NULL as TABLE_NAME," \
                                     " NULL  as TABLE_TYPE, NULL as REMARKS " \
                                     " FROM pg_namespace n,pg_class c LEFT JOIN pg_description d ON (c.oid=d.objoid AND d.objsubid=0) " \
                                     " WHERE n.oid=c.relnamespace " \
									 " AND n.nspname <> 'pg_toast' " \
									 " AND (n.nspname !~ '^pg_temp_' " \
									 "		OR n.nspname = (pg_catalog.current_schemas(true))[1]) AND (n.nspname !~ '^pg_toast_temp_' " \
									 "		OR n.nspname = replace((pg_catalog.current_schemas(true))[1], 'pg_temp_', 'pg_toast_temp_')) " \
                                     " AND(has_table_privilege(c.oid, 'SELECT') OR has_table_privilege(c.oid,'INSERT') " \
                                     "        OR has_table_privilege(c.oid,'UPDATE') OR has_table_privilege(c.oid,'DELETE')) " \
                                     " AND has_schema_privilege(n.oid, 'USAGE'::text) " \
                                     " AND c.relkind in ('r','v') " \
                                     " ORDER BY TABLE_SCHEM "

#define SQLTABLES_ALL_SCHEMAS_QUERY_DATASHARE " SELECT DISTINCT CAST(database_name AS varchar(124)) AS TABLE_CAT, CAST(schema_name AS varchar(124)) AS TABLE_SCHEM,NULL as TABLE_NAME," \
                                     " NULL  as TABLE_TYPE, NULL as REMARKS " \
                                     " FROM PG_CATALOG.SVV_ALL_SCHEMAS " \
                                     " ORDER BY TABLE_CAT, TABLE_SCHEM "

#define SQLTABLES_ALL_CATALOG_QUERY " SELECT DISTINCT current_database() as TABLE_CAT,NULL as TABLE_SCHEM,NULL as TABLE_NAME," \
                                     " NULL  as TABLE_TYPE, NULL as REMARKS " \
                                     " ORDER BY TABLE_CAT "

#define SQLTABLES_ALL_CATALOG_QUERY_DATASHARE " SELECT CAST(database_name AS varchar(124)) AS TABLE_CAT, NULL as TABLE_SCHEM,NULL as TABLE_NAME, " \
											  " NULL  as TABLE_TYPE, NULL as REMARKS " \
											  " FROM PG_CATALOG.SVV_REDSHIFT_DATABASES " \
											  " ORDER BY TABLE_CAT "


#define SQLTABLES_ALL_TABLE_TYPES_QUERY  " SELECT DISTINCT NULL as TABLE_CAT, NULL as TABLE_SCHEM, NULL as TABLE_NAME," \
                                     " 'TABLE'  as TABLE_TYPE, " \
                                     " NULL as REMARKS " \
                                     " UNION " \
                                     " SELECT DISTINCT NULL as TABLE_CAT, NULL as TABLE_SCHEM, NULL as TABLE_NAME," \
                                     " 'VIEW'  as TABLE_TYPE, " \
                                     " NULL as REMARKS " \
                                     " UNION " \
                                     " SELECT DISTINCT NULL as TABLE_CAT, NULL as TABLE_SCHEM, NULL as TABLE_NAME," \
                                     " 'SYSTEM TABLE'  as TABLE_TYPE, " \
                                     " NULL as REMARKS " \
                                     " UNION " \
                                     " SELECT DISTINCT NULL as TABLE_CAT, NULL as TABLE_SCHEM, NULL as TABLE_NAME," \
                                     " 'SYSTEM VIEW'  as TABLE_TYPE, " \
                                     " NULL as REMARKS " \
                                     " UNION " \
                                     " SELECT DISTINCT NULL as TABLE_CAT, NULL as TABLE_SCHEM, NULL as TABLE_NAME," \
                                     " 'TEMPORARY TABLE'  as TABLE_TYPE, " \
                                     " NULL as REMARKS " \
                                     " UNION " \
                                     " SELECT DISTINCT NULL as TABLE_CAT, NULL as TABLE_SCHEM, NULL as TABLE_NAME," \
                                     " 'TEMPORARY VIEW'  as TABLE_TYPE, " \
                                     " NULL as REMARKS " \
                                     " UNION " \
                                     " SELECT DISTINCT NULL as TABLE_CAT, NULL as TABLE_SCHEM, NULL as TABLE_NAME," \
                                     " 'SYSTEM TOAST TABLE'  as TABLE_TYPE, " \
                                     " NULL as REMARKS " \
                                     " ORDER BY TABLE_TYPE "

#define SQLPROCEDURES_BASE_QUERY  "SELECT current_database() AS PROCEDURE_CAT, n.nspname AS PROCEDURE_SCHEM, p.proname AS PROCEDURE_NAME, " \
								  " NULL as NUM_INPUT_PARAMS, NULL as NUM_OUTPUT_PARAMS, NULL as NUM_RESULT_SETS, d.description AS REMARKS, " \
								  " CASE  " \
								  " WHEN p.prokind='f' or p.proargmodes is not null THEN 2 " \
								  " WHEN p.prokind='p' THEN 1 " \
								  " ELSE 0 " \
								  " END AS PROCEDURE_TYPE " \
								  " FROM pg_catalog.pg_namespace n, pg_catalog.pg_proc_info p " \
								  " LEFT JOIN pg_catalog.pg_description d ON (p.prooid=d.objoid) " \
								  " LEFT JOIN pg_catalog.pg_class c ON (d.classoid=c.oid AND c.relname='pg_proc') " \
								  " LEFT JOIN pg_catalog.pg_namespace pn ON (c.relnamespace=pn.oid AND pn.nspname='pg_catalog') " \
								  " WHERE p.pronamespace=n.oid "

#define SQLPRIMARY_KEYS_BASE_QUERY     "SELECT " \
										"current_database() AS TABLE_CAT, " \
										"n.nspname AS TABLE_SCHEM,  " \
										"ct.relname AS TABLE_NAME,   " \
										"a.attname AS COLUMN_NAME,   " \
										"a.attnum AS KEY_SEQ,   " \
										"ci.relname AS PK_NAME   " \
										"FROM  " \
										"pg_catalog.pg_namespace n,  " \
										"pg_catalog.pg_class ct,  " \
										"pg_catalog.pg_class ci, " \
										"pg_catalog.pg_attribute a, " \
										"pg_catalog.pg_index i " \
										"WHERE " \
										"ct.oid=i.indrelid AND " \
										"ci.oid=i.indexrelid  AND " \
										"a.attrelid=ci.oid AND " \
										"i.indisprimary  AND " \
										"ct.relnamespace = n.oid "


#define SQLFOREIGN_KEYS_BASE_QUERY        " SELECT distinct current_database() as PKTABLE_CAT, n.nspname as PKTABLE_SCHEM, c.relname as PKTABLE_NAME, " \
                                        " a.attname as PKCOLUMN_NAME, current_database() as FKTABLE_CAT, fn.nspname as FKTABLE_SCHEM, fc.relname as FKTABLE_NAME, " \
                                        " fa.attname as FKCOLUMN_NAME,s.n as KEY_SEQ,NULL as UPDATE_RULE,NULL as DELETE_RULE, " \
                                        " f.conname as FK_NAME,pr.conname as PK_NAME, " \
                                        " (CASE WHEN NOT f.condeferred THEN 7 WHEN f.condeferrable THEN 5 ELSE 6 END) as DEFERRABILITY " \
                                        " FROM pg_catalog.pg_attribute a,pg_catalog.pg_attribute fa,pg_catalog.pg_class c,pg_catalog.pg_class fc, " \
                                        " pg_catalog.pg_namespace n,pg_catalog.pg_namespace fn,pg_catalog.pg_constraint f " \
                                        " left join pg_catalog.pg_constraint pr on(f.conrelid=pr.conrelid and pr.contype='p'), " \
                                        " (SELECT * FROM generate_series(1,current_setting('max_index_keys')::int,1))s(n) " \
                                        " WHERE fn.oid=fc.relnamespace AND n.oid=c.relnamespace AND c.oid=f.confrelid AND fc.oid=f.conrelid " \
                                        " AND a.attrelid=f.confrelid AND fa.attrelid=f.conrelid AND a.attnum=f.confkey[s.n] AND fa.attnum=f.conkey[s.n] " \
                                        " AND f.contype='f' AND f.conkey[s.n]<>0 AND has_schema_privilege(n.oid,'USAGE') "

#define SQLTABLE_PRIVILEGES_BASE_QUERY    " SELECT current_database() as TABLE_CAT, n.nspname as TABLE_SCHEM, c.relname as TABLE_NAME, " \
                                        " CASE WHEN u.usename=o.usename THEN'_SYSTEM'::text ELSE u.usename END as GRANTOR, " \
                                        " o.usename as GRANTEE, p.type as PRIVILEGE, " \
                                        " CASE WHEN aclcontains(c.relacl,makeaclitem(o.usesysid,o.grosysid,u.usesysid,p.type,TRUE))THEN'YES'::text ELSE'NO'::text END " \
                                        " as IS_GRANTABLE " \
                                        " FROM pg_catalog.pg_class c, pg_catalog.pg_namespace n, pg_catalog.pg_user u, " \
                                        " (SELECT z.usename,z.usesysid,x.grosysid FROM pg_catalog.pg_user z LEFT JOIN pg_catalog.pg_group x ON z.usesysid= " \
                                        "   ANY(x.grolist))o, (((((( ( SELECT 'SELECT' UNION ALL SELECT 'INSERT') UNION ALL SELECT 'UPDATE') UNION ALL SELECT 'DELETE') " \
                                        "   UNION ALL SELECT 'REFERENCES') UNION ALL SELECT 'RULE') UNION ALL SELECT 'TRIGGER') ) p(type) " \
                                        "   WHERE c.relnamespace=n.oid AND(c.relkind in('r','v') AND (c.relowner=o.usesysid AND u.usesysid=c.relowner) " \
                                        "      OR (aclcontains(c.relacl,makeaclitem(0,0,u.usesysid,p.type,FALSE)) " \
                                        "      OR aclcontains(c.relacl,makeaclitem(o.usesysid,o.grosysid,u.usesysid,p.type,FALSE)) " \
                                        "       OR aclcontains(c.relacl,makeaclitem(0,o.grosysid,u.usesysid,p.type,FALSE)))) "

#define SQLCOLUMN_PRIVILEGES_BASE_QUERY    " SELECT DISTINCT current_database() TABLE_CAT, n.nspname as TABLE_SCHEM, c.relname as TABLE_NAME, a.attname as COLUMN_NAME, " \
                                        " CASE WHEN u.usename=o.usename THEN'_SYSTEM'::text ELSE u.usename END as GRANTOR, " \
                                        " o.usename as GRANTEE, p.type as PRIVILEGE, " \
                                        " CASE WHEN aclcontains(c.relacl,makeaclitem(o.usesysid,o.grosysid,u.usesysid,p.type,TRUE))THEN'YES'::text ELSE'NO'::text END " \
                                        " as IS_GRANTABLE " \
                                        " FROM current_database()g, pg_catalog.pg_attribute a, pg_catalog.pg_class c, pg_catalog.pg_namespace n, pg_catalog.pg_user u, " \
                                        " (SELECT z.usename,z.usesysid,x.grosysid FROM pg_catalog.pg_user z LEFT JOIN pg_catalog.pg_group x ON z.usesysid= " \
                                        "   ANY(x.grolist)) o, (((((((SELECT 'SELECT' UNION ALL SELECT 'INSERT') UNION ALL SELECT 'UPDATE') UNION ALL SELECT 'DELETE') " \
                                        "   UNION ALL SELECT 'REFERENCES') UNION ALL SELECT 'RULE') UNION ALL SELECT 'TRIGGER')) p(type) " \
                                        "   WHERE a.attnum>0 and NOT a.attisdropped and a.attrelid = c.oid and c.relnamespace=n.oid and " \
                                        "        (c.relkind in('r','v') and (c.relowner=o.usesysid and u.usesysid=c.relowner) " \
                                        "         OR (aclcontains(c.relacl,makeaclitem(0,0,u.usesysid,p.type,FALSE)) " \
                                        "         OR aclcontains(c.relacl,makeaclitem(o.usesysid,o.grosysid,u.usesysid,p.type,FALSE)) " \
                                        "         OR aclcontains(c.relacl,makeaclitem(0,o.grosysid,u.usesysid,p.type,FALSE)))) "

#define SQLSTATISTICS_BASE_QUERY_PART_1    " SELECT NULL as TABLE_CAT,w.nspname as TABLE_SCHEM,w.relname as TABLE_NAME,i.unique as NON_UNIQUE, " \
                                        " w.nspname as INDEX_QUALIFIER,i.conname as INDEX_NAME,i.type as TYPE,s.n as ORDINAL_POSITION,a.attname as COLUMN_NAME, " \
                                        " 'A'::char as ASC_OR_DESC,NULL as CARDINALITY,COALESCE(i.pages,w.relpages) as PAGES,i.consrc as FILTER_CONDITION " \
                                        " FROM (SELECT c.oid,c.relpages, NULL,n.nspname,c.relname FROM pg_catalog.pg_namespace n,pg_catalog.pg_class c " \
                                        "            WHERE c.relnamespace=n.oid " 
// In between put condition like AND c.relname='table_name' AND n.nspname='schema_name'
#define SQLSTATISTICS_BASE_QUERY_PART_2    " )w LEFT JOIN(SELECT 3 AS type,NULL AS pages,1 AS unique,conrelid,conname,string_to_array(array_to_string(conkey,' '),' ') " \
                                        "            AS conkey,consrc FROM pg_catalog.pg_constraint WHERE contype='c' UNION SELECT CASE WHEN i.indisclustered " \
                                        "            THEN 1 WHEN m.amname='hash'THEN 2 ELSE 3 END,c.relpages,CASE WHEN i.indisunique THEN 0 ELSE 1 END,i.indrelid, " \
                                        "            c.relname,string_to_array(array_to_string(ARRAY[i.indkey],' '),' '), NULL FROM pg_catalog.pg_index i " \
                                        "            LEFT JOIN pg_catalog.pg_class c ON c.oid=i.indexrelid,pg_catalog.pg_am m WHERE m.oid=c.relam)i " \
                                        "            ON(i.conrelid=w.oid),(SELECT * FROM generate_series(1,current_setting('max_index_keys')::int,1))s(n), " \
                                        "        pg_catalog.pg_attribute a " \
                                        " WHERE a.attrelid=w.oid AND a.attnum=i.conkey[s.n-1]::int2 " \
                                        " UNION " \
                                        " SELECT NULL, n.nspname,c.relname,null,null,null,0,null,null,null,null,null,null " \
                                        " FROM pg_catalog.pg_namespace n,pg_catalog.pg_class c " \
                                        " WHERE c.relnamespace=n.oid" 


#define SQLSTATISTICS_QUERY_NO_RESULT   " SELECT NULL as TABLE_CAT, NULL as TABLE_SCHEM,NULL as TABLE_NAME,NULL as NON_UNIQUE, " \
                                        " NULL as INDEX_QUALIFIER,NULL as INDEX_NAME,NULL as TYPE,NULL as ORDINAL_POSITION,NULL as COLUMN_NAME, " \
                                        " NULL as ASC_OR_DESC,NULL as CARDINALITY,NULL as PAGES,NULL as FILTER_CONDITION " \
                                        " FROM (SELECT 1 where 1=0) " 

#define SQLSPECIALCOLUMNS_ROWID_BASE_QUERY_PART_1    " select 2 as SCOPE,   COLUMN_NAME, " \
                                                      " (case atttypid " \
                                                      "        when 16  then (-7) " \
                                                      "        when 18  then  1 " \
                                                      "        when 1042 then 1 " \
                                                      "        when     20   then (-5) " \
                                                      "        when    21   then 5 " \
                                                      "        when     23   then 4 " \
                                                      "        when    700  then 7 " \
                                                      "        when    701  then 8 " \
                                                      "        when     19   then 12 " \
                                                      "        when    1043 then 12 " \
                                                      "        when     1082 then 91 " \
                                                      "        when     1114 then 93 " \
                                                      "        when     1083 then 92 " \
                                                      "        when     1266 then 12 " \
                                                      "        when    1700 then 2 " \
                                                      "        else    0 end) as DATA_TYPE, " \
                                                      " (case atttypid " \
                                                      "        when 1042 then 'CHARACTER' " \
                                                      "        when 18 then 'CHARACTER' " \
                                                      "        when 1043 then 'CHARACTER VARYING' " \
                                                      "        when 19 then 'CHARACTER VARYING' " \
                                                      "        when 21 then 'SMALLINT' " \
                                                      "        when 23 then 'INTEGER' " \
                                                      "        when 20 then 'BIGINT' " \
                                                      "        when 700 then 'REAL' " \
                                                      "        when 701 then 'DOUBLE PRECISION' " \
                                                      "        when 16 then 'BOOL' " \
                                                      "        when 1082 then 'DATE' " \
                                                      "        when 1114 then 'TIMESTAMP' " \
                                                      "        when 1083 then 'TIME' " \
                                                      "        when 1266 then 'TIMETZ' " \
                                                      "        when 1700 then 'NUMERIC' " \
                                                      "        else 'UNKNOWN' end) as TYPE_NAME, " \
                                                      " (case when atttypid=1700 " \
                                                      "       then (case when atttypmod=-1 then 1003 " \
                                                      "                     else (case when atttypmod/65536>38 then atttypmod/65536+3 " \
                                                      "                                else atttypmod/65536 " \
                                                      "                                end) " \
                                                      "                      end) " \
                                                      "           when atttypmod>0 and (atttypid=1042 or atttypid=19 or atttypid=18) then atttypmod-4 " \
                                                      "           when atttypid=21 then 5 " \
                                                      "           when atttypid=23 then 10 " \
                                                      "           when atttypid=20 then 19 " \
                                                      "           when atttypid=700 then 15 " \
                                                      "           when atttypid=701 then 53 " \
                                                      "           when atttypid=16 then 1 " \
                                                      "           when atttypid=1082 then 10 " \
                                                      "           when atttypid=1114 then 26 " \
                                                      "           when atttypid=1083 then 15 " \
                                                      "           when atttypid=1266 then 21 " \
                                                      "           else atttypmod  end) as COLUMN_SIZE, " \
                                                      " (case when atttypid=1700 " \
                                                      "          then (case when atttypmod=-1 then 1003 " \
                                                      "                     when atttypmod/65536>38 then atttypmod/65536+3 " \
                                                      "                     when ((atttypmod-4)%65536)>0 then atttypmod/65536+2 " \
                                                      "                     else atttypmod/65536+1 " \
                                                      "                     end) " \
                                                      "          when (atttypid=1042 or atttypid=19 or atttypid=18) then atttypmod-4 " \
                                                      "           when atttypid=1082 then 6 " \
                                                      "           when atttypid=1114 then 16 " \
                                                      "           when atttypid=1083 then 6 " \
                                                      "           when atttypid=1266 then 21 " \
                                                      "        else attlen end) as BUFFER_LENGTH, " \
                                                      " (case when atttypid=1700 and atttypmod>0 then atttypmod%65536-4 " \
                                                      "          when atttypid=1114 then 6 " \
                                                      "          when atttypid=1083 then 6 " \
                                                      "          when atttypid=1266 then 6 " \
                                                      "          when (atttypid=21 or atttypid=23 or atttypid=20) then 0 " \
                                                      "          else NULL  end) as DECIMAL_DIGITS," \
                                                      "     1::int2 as PSEUDO_COLUMN " \
                                        " FROM (SELECT NULL as TABLE_CAT,w.nspname as TABLE_SCHEM,w.relname as TABLE_NAME,i.unique as NON_UNIQUE, " \
                                        " w.nspname as INDEX_QUALIFIER,i.conname as INDEX_NAME,i.type as TYPE,s.n as ORDINAL_POSITION,a.attname as COLUMN_NAME, " \
                                        " 'A'::char as ASC_OR_DESC,NULL as CARDINALITY,COALESCE(i.pages,w.relpages) as PAGES,i.consrc as FILTER_CONDITION, " \
                                        " a.atttypmod as atttypmod, a.atttypid atttypid, a.attlen attlen "\
                                        " FROM (SELECT c.oid,c.relpages, NULL,n.nspname,c.relname FROM pg_catalog.pg_namespace n,pg_catalog.pg_class c " \
                                        "            WHERE c.relnamespace=n.oid " 

// In between put condition like AND c.relname='table_name' AND n.nspname='schema_name'
#define SQLSPECIALCOLUMNS_ROWID_BASE_QUERY_PART_2    " )w LEFT JOIN(SELECT 3 AS type,NULL AS pages,1 AS unique,conrelid,conname,string_to_array(array_to_string(conkey,' '),' ') " \
                                        "            AS conkey,consrc FROM pg_catalog.pg_constraint WHERE contype='c' UNION SELECT CASE WHEN i.indisclustered " \
                                        "            THEN 1 WHEN m.amname='hash'THEN 2 ELSE 3 END,c.relpages,CASE WHEN i.indisunique THEN 0 ELSE 1 END,i.indrelid, " \
                                        "            c.relname,string_to_array(array_to_string(ARRAY[i.indkey],' '),' '), NULL FROM pg_catalog.pg_index i " \
                                        "            LEFT JOIN pg_catalog.pg_class c ON c.oid=i.indexrelid,pg_catalog.pg_am m WHERE m.oid=c.relam)i " \
                                        "            ON(i.conrelid=w.oid),(SELECT * FROM generate_series(1,current_setting('max_index_keys')::int,1))s(n), " \
                                        "        pg_catalog.pg_attribute a " \
                                        " WHERE a.attrelid=w.oid AND a.attnum=i.conkey[s.n-1]::int2 " \

// add  a.attnotnull 
#define SQLSPECIALCOLUMNS_ROWID_BASE_QUERY_PART_3    " UNION " \
                                        " SELECT NULL, n.nspname,c.relname,null,null,null,0,null,null,null,null,null,null,null,null, null " \
                                        " FROM pg_catalog.pg_namespace n,pg_catalog.pg_class c " \
                                        " WHERE c.relnamespace=n.oid" 

#define SQLSPECIALCOLUMNS_ROWID_BASE_QUERY_PART_4    " ) WHERE COLUMN_NAME is not null"

#define SQLSPECIALCOLUMNS_ROWVER_BASE_QUERY_PART_1 " SELECT 0 as SCOPE, 'xmin' as COLUMN_NAME,4::int2 as DATA_TYPE,'integer' as TYPE_NAME,10 as COLUMN_SIZE, " \
                                            " 4 as BUFFER_LENGTH, 0::int2 as DECIMAL_DIGITS,2::int2 as PSEUDO_COLUMN " \
                                            " FROM pg_catalog.pg_class c,pg_catalog.pg_namespace n " \
                                            " WHERE c.relnamespace = n.oid AND has_schema_privilege(n.oid, 'USAGE'::text) " \
                                            
#define SQLSPECIALCOLUMNS_ROWVER_BASE_QUERY_PART_2 " UNION SELECT 0,'oid',4::int2,'oid',10,4,0::int2,2::int2 "\
                                                   " FROM pg_catalog.pg_class c,pg_catalog.pg_namespace n " \
                                                   " WHERE c.relnamespace = n.oid AND c.relhasoids AND has_schema_privilege(n.oid, 'USAGE'::text) "


// Function prototypes
static void addLikeOrEqualFilterCondition(RS_STMT_INFO *pStmt, char *szCatalogQuery, const char *pFilterColumn, const char *pVal, SQLSMALLINT cbLen);
static void addTableTypeFilterCondition(char *filterClause, char *pVal, SQLSMALLINT cbLen, int schemaPatternType);
static void addOrderByClause(char *szCatalogQuery, char *pOrderByCols);
static void addTypeRow(char *szCatalogQuery, RS_TYPE_INFO *pTypeInfo);
static void addEqualFilterCondition(RS_STMT_INFO *pStmt, char *szCatalogQuery, int bufLen, char *pFilterColumn, char *pVal, SQLSMALLINT cbLen);
static int checkForValidCatalogName(RS_STMT_INFO *pStmt, SQLCHAR *pCatalogName); // Deprecated
static bool isSingleDatabaseMetaData(RS_STMT_INFO *pStmt);
static int getExtSchemaPatternMatch(RS_STMT_INFO *pStmt, SQLCHAR *pSchemaName, SQLSMALLINT cbSchemaName);
static void getTableFilterClause(
	char *filterClause,
	RS_STMT_INFO *pStmt,
	SQLCHAR *pCatalogName,
	SQLSMALLINT cbCatalogName,
	SQLCHAR *pSchemaName,
	SQLSMALLINT cbSchemaName,
	SQLCHAR *pTableName,
	SQLSMALLINT cbTableName,
	SQLCHAR *pTableType,
	SQLSMALLINT cbTableType,
	int schemaPatternType,
	bool apiSupportedOnlyForConnectedDatabase,
	char * databaseColName);

static void getCatalogFilterCondition(char *catalogFilter,
										int bufLen,
										RS_STMT_INFO *pStmt,
										char *pCatalogName,
										short cbCatalogName,
										bool apiSupportedOnlyForConnectedDatabase,
										char * databaseColName);
static char *escapedFilterCondition(const char *pName, short cbName);


static void buildLocalSchemaTablesQuery(char *pszCatalogQuery,
	RS_STMT_INFO *pStmt,
	SQLCHAR *pCatalogName,
	SQLSMALLINT cbCatalogName,
	SQLCHAR *pSchemaName,
	SQLSMALLINT cbSchemaName,
	SQLCHAR *pTableName,
	SQLSMALLINT cbTableName,
	SQLCHAR *pTableType,
	SQLSMALLINT cbTableType);

static void buildUniversalSchemaTablesQuery(char *pszCatalogQuery,
	RS_STMT_INFO *pStmt,
	SQLCHAR *pCatalogName,
	SQLSMALLINT cbCatalogName,
	SQLCHAR *pSchemaName,
	SQLSMALLINT cbSchemaName,
	SQLCHAR *pTableName,
	SQLSMALLINT cbTableName,
	SQLCHAR *pTableType,
	SQLSMALLINT cbTableType);

static void buildUniversalAllSchemaTablesQuery(char *pszCatalogQuery,
	RS_STMT_INFO *pStmt,
	SQLCHAR *pCatalogName,
	SQLSMALLINT cbCatalogName,
	SQLCHAR *pSchemaName,
	SQLSMALLINT cbSchemaName,
	SQLCHAR *pTableName,
	SQLSMALLINT cbTableName,
	SQLCHAR *pTableType,
	SQLSMALLINT cbTableType);

static void buildExternalSchemaTablesQuery(char *pszCatalogQuery,
	RS_STMT_INFO *pStmt,
	SQLCHAR *pCatalogName,
	SQLSMALLINT cbCatalogName,
	SQLCHAR *pSchemaName,
	SQLSMALLINT cbSchemaName,
	SQLCHAR *pTableName,
	SQLSMALLINT cbTableName,
	SQLCHAR *pTableType,
	SQLSMALLINT cbTableType);

static void buildLocalSchemaColumnsQuery(char *pszCatalogQuery,
	RS_STMT_INFO *pStmt,
	SQLCHAR *pCatalogName,
	SQLSMALLINT cbCatalogName,
	SQLCHAR *pSchemaName,
	SQLSMALLINT cbSchemaName,
	SQLCHAR *pTableName,
	SQLSMALLINT cbTableName,
	SQLCHAR *pColumnName,
	SQLSMALLINT cbColumnName);

static void buildUniversalSchemaColumnsQuery(char *pszCatalogQuery,
	RS_STMT_INFO *pStmt,
	SQLCHAR *pCatalogName,
	SQLSMALLINT cbCatalogName,
	SQLCHAR *pSchemaName,
	SQLSMALLINT cbSchemaName,
	SQLCHAR *pTableName,
	SQLSMALLINT cbTableName,
	SQLCHAR *pColumnName,
	SQLSMALLINT cbColumnName);

static void buildUniversalAllSchemaColumnsQuery(char *pszCatalogQuery,
	RS_STMT_INFO *pStmt,
	SQLCHAR *pCatalogName,
	SQLSMALLINT cbCatalogName,
	SQLCHAR *pSchemaName,
	SQLSMALLINT cbSchemaName,
	SQLCHAR *pTableName,
	SQLSMALLINT cbTableName,
	SQLCHAR *pColumnName,
	SQLSMALLINT cbColumnName);

static void buildExternalSchemaColumnsQuery(char *pszCatalogQuery,
	RS_STMT_INFO *pStmt,
	SQLCHAR *pCatalogName,
	SQLSMALLINT cbCatalogName,
	SQLCHAR *pSchemaName,
	SQLSMALLINT cbSchemaName,
	SQLCHAR *pTableName,
	SQLSMALLINT cbTableName,
	SQLCHAR *pColumnName,
	SQLSMALLINT cbColumnName);


/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLTables returns the list of table, catalog, or schema names, and table types, 
// stored in a specific data source. The driver returns the information as a result set.
//
SQLRETURN  SQL_API SQLTables(SQLHSTMT phstmt,
                               SQLCHAR *pCatalogName, 
                               SQLSMALLINT cbCatalogName,
                               SQLCHAR *pSchemaName, 
                               SQLSMALLINT cbSchemaName,
                               SQLCHAR *pTableName, 
                               SQLSMALLINT cbTableName,
                               SQLCHAR *pTableType, 
                               SQLSMALLINT cbTableType)
{
    SQLRETURN rc;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLTables(FUNC_CALL, 0, phstmt, pCatalogName, cbCatalogName, pSchemaName, cbSchemaName, pTableName, cbTableName, pTableType, cbTableType);

    rc = RsCatalog::RS_SQLTables(phstmt, pCatalogName, cbCatalogName, pSchemaName, cbSchemaName, pTableName, cbTableName, pTableType, cbTableType);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLTables(FUNC_RETURN, rc, phstmt, pCatalogName, cbCatalogName, pSchemaName, cbSchemaName, pTableName, cbTableName, pTableType, cbTableType);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLTables and SQLTablesW.
//
// SQL_ATTR_METADATA_ID
SQLRETURN  SQL_API RsCatalog::RS_SQLTables(SQLHSTMT phstmt,
                               SQLCHAR *pCatalogName, 
                               SQLSMALLINT cbCatalogName,
                               SQLCHAR *pSchemaName, 
                               SQLSMALLINT cbSchemaName,
                               SQLCHAR *pTableName, 
                               SQLSMALLINT cbTableName,
                               SQLCHAR *pTableType, 
                               SQLSMALLINT cbTableType)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    char szCatalogQuery[MAX_CATALOG_QUERY_LEN];

    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pStmt->pErrorList = clearErrorList(pStmt->pErrorList);

	if ((!pSchemaName || (pSchemaName && *pSchemaName == '\0'))
		&& (!pTableName || (pTableName && *pTableName == '\0'))
		&& (!pTableType || (pTableType && *pTableType == '\0'))
		&& ((pCatalogName != NULL)
			&& ((cbCatalogName == SQL_NTS && _stricmp((char *)pCatalogName, SQL_ALL_CATALOGS) == 0)
				|| (cbCatalogName != SQL_NTS && _strnicmp((char *)pCatalogName, SQL_ALL_CATALOGS, cbCatalogName) == 0)
				) 
			)
		)
	{
		if (isSingleDatabaseMetaData(pStmt))
			rs_strncpy(szCatalogQuery, SQLTABLES_ALL_CATALOG_QUERY, sizeof(szCatalogQuery));
		else
			rs_strncpy(szCatalogQuery, SQLTABLES_ALL_CATALOG_QUERY_DATASHARE, sizeof(szCatalogQuery));
	}
	else 
    if( (!pCatalogName || (pCatalogName && *pCatalogName == '\0'))
         && (!pTableName || (pTableName && *pTableName == '\0'))
		 && (!pTableType || (pTableType && *pTableType == '\0'))
		 && (pSchemaName
                && ((cbSchemaName == SQL_NTS && _stricmp((char *)pSchemaName, SQL_ALL_SCHEMAS) == 0)
                     || (cbSchemaName != SQL_NTS && _strnicmp((char *)pSchemaName, SQL_ALL_SCHEMAS, cbSchemaName) == 0)
                    )
            )
    )
    {
		if (isSingleDatabaseMetaData(pStmt))
			rs_strncpy(szCatalogQuery, SQLTABLES_ALL_SCHEMAS_QUERY, sizeof(szCatalogQuery));
		else
			rs_strncpy(szCatalogQuery, SQLTABLES_ALL_SCHEMAS_QUERY_DATASHARE, sizeof(szCatalogQuery));
	}
    else
    if( (!pCatalogName || (pCatalogName && *pCatalogName == '\0'))
         && (!pSchemaName || (pSchemaName && *pSchemaName == '\0'))
         && (!pTableName || (pTableName && *pTableName == '\0'))
         && (pTableType 
                && ((cbTableType == SQL_NTS && _stricmp((char *)pTableType, SQL_ALL_TABLE_TYPES) == 0)
                     || (cbTableType != SQL_NTS && _strnicmp((char *)pTableType, SQL_ALL_TABLE_TYPES, cbTableType) == 0)
                    )
            )
        )
    {
        rs_strncpy(szCatalogQuery, SQLTABLES_ALL_TABLE_TYPES_QUERY, sizeof(szCatalogQuery));
    }
    else
    {
		int schemaPatternType;

		if(isSingleDatabaseMetaData(pStmt)
			&& !checkForValidCatalogName(pStmt, pCatalogName))
		{
			rc = SQL_ERROR;
			addError(&pStmt->pErrorList,"HYC00", "Optional feature not implemented::RS_SQLTables", 0, NULL);
			goto error;
		} 

		schemaPatternType = getExtSchemaPatternMatch(pStmt, pSchemaName, cbSchemaName);

		if (IS_TRACE_ON())
		{
			RS_LOG_INFO("RSCATALOG", "schemaPatternType=%d", schemaPatternType);
		}

		if (schemaPatternType == LOCAL_SCHEMA_QUERY)
		{
			// Join on pg_catalog
			buildLocalSchemaTablesQuery(szCatalogQuery, pStmt,
				pCatalogName,
				cbCatalogName,
				pSchemaName,
				cbSchemaName,
				pTableName,
				cbTableName,
				pTableType,
				cbTableType);
		}
		else if (schemaPatternType == NO_SCHEMA_UNIVERSAL_QUERY) 
		{
			if (isSingleDatabaseMetaData(pStmt))
			{
				// svv_tables
				buildUniversalSchemaTablesQuery(szCatalogQuery, pStmt,
												pCatalogName,
												cbCatalogName,
												pSchemaName,
												cbSchemaName,
												pTableName,
												cbTableName,
												pTableType,
												cbTableType);
			}
			else {
				// svv_all_tables
				buildUniversalAllSchemaTablesQuery(szCatalogQuery, pStmt,
					pCatalogName,
					cbCatalogName,
					pSchemaName,
					cbSchemaName,
					pTableName,
					cbTableName,
					pTableType,
					cbTableType);
			}
		}
		else if (schemaPatternType == EXTERNAL_SCHEMA_QUERY) {
			// svv_external_tables
			buildExternalSchemaTablesQuery(szCatalogQuery, pStmt,
				pCatalogName,
				cbCatalogName,
				pSchemaName,
				cbSchemaName,
				pTableName,
				cbTableName,
				pTableType,
				cbTableType);
		}

    }

    setCatalogQueryBuf(pStmt, szCatalogQuery);
    rc = RsExecute::RS_SQLExecDirect(phstmt, (SQLCHAR *)szCatalogQuery, SQL_NTS, TRUE, FALSE, FALSE, TRUE);
    resetCatalogQueryFlag(pStmt);

error:

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Unicode version of SQLTables 
//
SQLRETURN SQL_API SQLTablesW(SQLHSTMT          phstmt,
                                SQLWCHAR*      pwCatalogName,
                                SQLSMALLINT    cchCatalogName,
                                SQLWCHAR*      pwSchemaName,
                                SQLSMALLINT    cchSchemaName,
                                SQLWCHAR*      pwTableName,
                                SQLSMALLINT    cchTableName,
                                SQLWCHAR*      pwTableType,
                                SQLSMALLINT    cchTableType)
{
    SQLRETURN rc;
    char szCatalogName[MAX_IDEN_LEN] = {0};
    char szSchemaName[MAX_IDEN_LEN] = {0};
    char szTableName[MAX_IDEN_LEN] = {0}; 
    char szTableType[MAX_TEMP_BUF_LEN] = {0}; 
    
    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLTablesW(FUNC_CALL, 0, phstmt, pwCatalogName, cchCatalogName, pwSchemaName, cchSchemaName, pwTableName, cchTableName, pwTableType, cchTableType);

	wchar16_to_utf8_char(pwCatalogName, cchCatalogName, szCatalogName, MAX_IDEN_LEN);
    wchar16_to_utf8_char(pwSchemaName, cchSchemaName, szSchemaName, MAX_IDEN_LEN);
    wchar16_to_utf8_char(pwTableName, cchTableName, szTableName, MAX_IDEN_LEN);
    wchar16_to_utf8_char(pwTableType, cchTableType, szTableType, MAX_TEMP_BUF_LEN);
	RS_LOG_TRACE("RSCAT", "wchar_to_utf8 strlen(szCatalogName)=%d", strlen(szCatalogName));

    rc = RsCatalog::RS_SQLTables(phstmt, (SQLCHAR *)((pwCatalogName) ? szCatalogName : NULL), SQL_NTS,
                                (SQLCHAR *)((pwSchemaName) ? szSchemaName : NULL), SQL_NTS,
                                (SQLCHAR *)((pwTableName) ? szTableName : NULL), SQL_NTS,
                                (SQLCHAR *)((pwTableType) ? szTableType : NULL), SQL_NTS);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLTablesW(FUNC_RETURN, rc, phstmt, pwCatalogName, cchCatalogName, pwSchemaName, cchSchemaName, pwTableName, cchTableName, pwTableType, cchTableType);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLColumns returns the list of column names in specified tables. 
// The driver returns this information as a result set on the specified StatementHandle.
//
SQLRETURN  SQL_API SQLColumns(SQLHSTMT phstmt,
                               SQLCHAR *pCatalogName, 
                               SQLSMALLINT cbCatalogName,
                               SQLCHAR *pSchemaName, 
                               SQLSMALLINT cbSchemaName,
                               SQLCHAR *pTableName, 
                               SQLSMALLINT cbTableName,
                               SQLCHAR *pColumnName, 
                               SQLSMALLINT cbColumnName)
{
    SQLRETURN rc;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLColumns(FUNC_CALL, 0, phstmt, pCatalogName, cbCatalogName, pSchemaName, cbSchemaName, pTableName, cbTableName, pColumnName, cbColumnName);

    rc = RsCatalog::RS_SQLColumns(phstmt, pCatalogName, cbCatalogName, pSchemaName, cbSchemaName, pTableName, cbTableName, pColumnName, cbColumnName);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLColumns(FUNC_RETURN, rc, phstmt, pCatalogName, cbCatalogName, pSchemaName, cbSchemaName, pTableName, cbTableName, pColumnName, cbColumnName);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLColumns and SQLColumnsW. 
//
SQLRETURN  SQL_API RsCatalog::RS_SQLColumns(SQLHSTMT phstmt,
                               SQLCHAR *pCatalogName, 
                               SQLSMALLINT cbCatalogName,
                               SQLCHAR *pSchemaName, 
                               SQLSMALLINT cbSchemaName,
                               SQLCHAR *pTableName, 
                               SQLSMALLINT cbTableName,
                               SQLCHAR *pColumnName, 
                               SQLSMALLINT cbColumnName)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    char szCatalogQuery[MAX_CATALOG_QUERY_LEN];
	int schemaPatternType;

    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pStmt->pErrorList = clearErrorList(pStmt->pErrorList);

    if(isSingleDatabaseMetaData(pStmt)
		&& !checkForValidCatalogName(pStmt, pCatalogName))
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HYC00", "Optional feature not implemented::RS_SQLColumns", 0, NULL);
        goto error; 
    }

	schemaPatternType = getExtSchemaPatternMatch(pStmt, pSchemaName, cbSchemaName);

	if (IS_TRACE_ON())
	{
		RS_LOG_INFO("RSCATALOG", "schemaPatternType=%d", schemaPatternType);
	}

	if (schemaPatternType == LOCAL_SCHEMA_QUERY)
	{
		// Join on pg_catalog union with pg_late_binding_view
		buildLocalSchemaColumnsQuery(szCatalogQuery, pStmt,
			pCatalogName,
			cbCatalogName,
			pSchemaName,
			cbSchemaName,
			pTableName,
			cbTableName,
			pColumnName,
			cbColumnName);
	}
	else 
	if (schemaPatternType == NO_SCHEMA_UNIVERSAL_QUERY) 
	{
		if (isSingleDatabaseMetaData(pStmt)) 
		{
			// svv_columns
			buildUniversalSchemaColumnsQuery(szCatalogQuery, pStmt,
				pCatalogName,
				cbCatalogName,
				pSchemaName,
				cbSchemaName,
				pTableName,
				cbTableName,
				pColumnName,
				cbColumnName);
		}
		else 
		{
			// svv_all_columns
			buildUniversalAllSchemaColumnsQuery(szCatalogQuery, pStmt,
				pCatalogName,
				cbCatalogName,
				pSchemaName,
				cbSchemaName,
				pTableName,
				cbTableName,
				pColumnName,
				cbColumnName);
		}
	}
	else 
	if (schemaPatternType == EXTERNAL_SCHEMA_QUERY) 
	{
		// svv_external_columns
		buildExternalSchemaColumnsQuery(szCatalogQuery, pStmt,
			pCatalogName,
			cbCatalogName,
			pSchemaName,
			cbSchemaName,
			pTableName,
			cbTableName,
			pColumnName,
			cbColumnName);
	}


    setCatalogQueryBuf(pStmt, szCatalogQuery);
    rc = RsExecute::RS_SQLExecDirect(phstmt, (SQLCHAR *)szCatalogQuery, SQL_NTS, TRUE, FALSE, FALSE, TRUE);
    resetCatalogQueryFlag(pStmt);

error:

    return rc;
}


/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Unicode version of SQLColumns.
//
SQLRETURN SQL_API SQLColumnsW(SQLHSTMT     phstmt,
                                SQLWCHAR*    pwCatalogName,
                                SQLSMALLINT  cchCatalogName,
                                SQLWCHAR*    pwSchemaName,
                                SQLSMALLINT  cchSchemaName,
                                SQLWCHAR*    pwTableName,
                                SQLSMALLINT  cchTableName,
                                SQLWCHAR*    pwColumnName,
                                SQLSMALLINT  cchColumnName)
{
    SQLRETURN rc;
    char szCatalogName[MAX_IDEN_LEN];
    char szSchemaName[MAX_IDEN_LEN];
    char szTableName[MAX_IDEN_LEN]; 
    char szColumnName[MAX_IDEN_LEN]; 
    
    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLColumnsW(FUNC_CALL, 0, phstmt, pwCatalogName, cchCatalogName, pwSchemaName, cchSchemaName, pwTableName, cchTableName, pwColumnName, cchColumnName);

    wchar_to_utf8(pwCatalogName, cchCatalogName, szCatalogName, MAX_IDEN_LEN);
    wchar_to_utf8(pwSchemaName, cchSchemaName, szSchemaName, MAX_IDEN_LEN);
    wchar_to_utf8(pwTableName, cchTableName, szTableName, MAX_IDEN_LEN);
    wchar_to_utf8(pwColumnName, cchColumnName, szColumnName, MAX_IDEN_LEN);


    rc = RsCatalog::RS_SQLColumns(phstmt, (SQLCHAR *)((pwCatalogName) ? szCatalogName : NULL), SQL_NTS,
                              (SQLCHAR *)((pwSchemaName) ? szSchemaName : NULL), SQL_NTS,
                              (SQLCHAR *)((pwTableName) ? szTableName : NULL), SQL_NTS,
                              (SQLCHAR *)((pwColumnName) ? szColumnName : NULL), SQL_NTS);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLColumnsW(FUNC_RETURN, rc, phstmt, pwCatalogName, cchCatalogName, pwSchemaName, cchSchemaName, pwTableName, cchTableName, pwColumnName, cchColumnName);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLStatistics retrieves a list of statistics about a single table 
// and the indexes associated with the table. The driver returns the information as a result set.
//
SQLRETURN  SQL_API SQLStatistics(SQLHSTMT phstmt,
                                   SQLCHAR *pCatalogName, 
                                   SQLSMALLINT cbCatalogName, 
                                   SQLCHAR *pSchemaName, 
                                   SQLSMALLINT cbSchemaName,
                                   SQLCHAR *pTableName, 
                                   SQLSMALLINT cbTableName,
                                   SQLUSMALLINT hUnique, 
                                   SQLUSMALLINT hReserved)
{
    SQLRETURN rc;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLStatistics(FUNC_CALL, 0, phstmt, pCatalogName, cbCatalogName, pSchemaName, cbSchemaName, pTableName, cbTableName, hUnique, hReserved);

    rc = RsCatalog::RS_SQLStatistics(phstmt, pCatalogName, cbCatalogName, pSchemaName, cbSchemaName, pTableName, cbTableName, hUnique, hReserved);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLStatistics(FUNC_RETURN, rc, phstmt, pCatalogName, cbCatalogName, pSchemaName, cbSchemaName, pTableName, cbTableName, hUnique, hReserved);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common fucntion for SQLStatistics and SQLStatisticsW. 
//
// hUnique
SQLRETURN  SQL_API RsCatalog::RS_SQLStatistics(SQLHSTMT phstmt,
                                   SQLCHAR *pCatalogName, 
                                   SQLSMALLINT cbCatalogName, 
                                   SQLCHAR *pSchemaName, 
                                   SQLSMALLINT cbSchemaName,
                                   SQLCHAR *pTableName, 
                                   SQLSMALLINT cbTableName,
                                   SQLUSMALLINT hUnique, 
                                   SQLUSMALLINT hReserved)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    char szCatalogQuery[MAX_CATALOG_QUERY_LEN];

    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pStmt->pErrorList = clearErrorList(pStmt->pErrorList);

    if(!checkForValidCatalogName(pStmt, pCatalogName))
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HYC00", "Optional feature not implemented::RS_SQLStatistics", 0, NULL);
        goto error; 
    }

    if(pTableName == NULL || cbTableName == SQL_NULL_DATA)
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HY009", "Invalid use of null pointer", 0, NULL);
        goto error; 
    }

    rs_strncpy(szCatalogQuery, SQLSTATISTICS_QUERY_NO_RESULT, sizeof(szCatalogQuery));

    setCatalogQueryBuf(pStmt, szCatalogQuery);
    rc = RsExecute::RS_SQLExecDirect(phstmt, (SQLCHAR *)szCatalogQuery, SQL_NTS, TRUE, FALSE, FALSE, TRUE);
    resetCatalogQueryFlag(pStmt);

error:

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Unicode version of SQLStatistics.
//
SQLRETURN SQL_API SQLStatisticsW(SQLHSTMT        phstmt,
                                    SQLWCHAR*    pwCatalogName,
                                    SQLSMALLINT  cchCatalogName,
                                    SQLWCHAR*    pwSchemaName,
                                    SQLSMALLINT  cchSchemaName,
                                    SQLWCHAR*    pwTableName,
                                    SQLSMALLINT  cchTableName,
                                    SQLUSMALLINT hUnique,
                                    SQLUSMALLINT hReserved)
{
    SQLRETURN rc;
    char szCatalogName[MAX_IDEN_LEN];
    char szSchemaName[MAX_IDEN_LEN];
    char szTableName[MAX_IDEN_LEN]; 
    
    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLStatisticsW(FUNC_CALL, 0, phstmt, pwCatalogName, cchCatalogName, pwSchemaName, cchSchemaName, pwTableName, cchTableName, hUnique, hReserved);

    wchar_to_utf8(pwCatalogName, cchCatalogName, szCatalogName, MAX_IDEN_LEN);
    wchar_to_utf8(pwSchemaName, cchSchemaName, szSchemaName, MAX_IDEN_LEN);
    wchar_to_utf8(pwTableName, cchTableName, szTableName, MAX_IDEN_LEN);

    rc = RsCatalog::RS_SQLStatistics(phstmt, (SQLCHAR *)((pwCatalogName) ? szCatalogName : NULL), SQL_NTS,
                                  (SQLCHAR *)((pwSchemaName) ? szSchemaName : NULL), SQL_NTS,
                                  (SQLCHAR *)((pwTableName) ? szTableName : NULL), SQL_NTS,
                                  hUnique,
                                  hReserved);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLStatisticsW(FUNC_RETURN, rc, phstmt, pwCatalogName, cchCatalogName, pwSchemaName, cchSchemaName, pwTableName, cchTableName, hUnique, hReserved);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLSpecialColumns retrieves the following information about columns within a specified table:
//  The optimal set of columns that uniquely identifies a row in the table.
//  Columns that are automatically updated when any value in the row is updated by a transaction.
//
SQLRETURN  SQL_API SQLSpecialColumns(SQLHSTMT phstmt,
                                       SQLUSMALLINT hIdenType, 
                                       SQLCHAR *pCatalogName, 
                                       SQLSMALLINT cbCatalogName,
                                       SQLCHAR *pSchemaName, 
                                       SQLSMALLINT cbSchemaName, 
                                       SQLCHAR *pTableName, 
                                       SQLSMALLINT cbTableName, 
                                       SQLUSMALLINT hScope, 
                                       SQLUSMALLINT hNullable)
{
    SQLRETURN rc;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLSpecialColumns(FUNC_CALL, 0, phstmt, hIdenType, pCatalogName, cbCatalogName, pSchemaName, cbSchemaName, pTableName, cbTableName, hScope, hNullable);

    rc = RsCatalog::RS_SQLSpecialColumns(phstmt, hIdenType, pCatalogName, cbCatalogName, pSchemaName, cbSchemaName, pTableName, cbTableName, hScope, hNullable);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLSpecialColumns(FUNC_RETURN, rc, phstmt, hIdenType, pCatalogName, cbCatalogName, pSchemaName, cbSchemaName, pTableName, cbTableName, hScope, hNullable);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLSpecialColumns and SQLSpecialColumnsW.
//
SQLRETURN  SQL_API RsCatalog::RS_SQLSpecialColumns(SQLHSTMT phstmt,
                                       SQLUSMALLINT hIdenType, 
                                       SQLCHAR *pCatalogName, 
                                       SQLSMALLINT cbCatalogName,
                                       SQLCHAR *pSchemaName, 
                                       SQLSMALLINT cbSchemaName, 
                                       SQLCHAR *pTableName, 
                                       SQLSMALLINT cbTableName, 
                                       SQLUSMALLINT hScope, 
                                       SQLUSMALLINT hNullable)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    char szCatalogQuery[MAX_CATALOG_QUERY_LEN];

    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pStmt->pErrorList = clearErrorList(pStmt->pErrorList);

    if(!checkForValidCatalogName(pStmt, pCatalogName))
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HYC00", "Optional feature not implemented:RS_SQLSpecialColumns", 0, NULL);
        goto error; 
    }

    if(pTableName == NULL || cbTableName == SQL_NULL_DATA)
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HY009", "Invalid use of null pointer:RS_SQLSpecialColumns", 0, NULL);
        goto error; 
    }

    if(hIdenType == SQL_BEST_ROWID)
    {
        rs_strncpy(szCatalogQuery, SQLSPECIALCOLUMNS_ROWID_BASE_QUERY_PART_1, sizeof(szCatalogQuery));
        addEqualFilterCondition(pStmt, szCatalogQuery, MAX_CATALOG_QUERY_LEN, "n.nspname" , (char *)pSchemaName, cbSchemaName); // Condition is in subquery.
        addEqualFilterCondition(pStmt, szCatalogQuery, MAX_CATALOG_QUERY_LEN, "c.relname" , (char *)pTableName, cbTableName);   // Condition is in subquery.
        rs_strncat(szCatalogQuery, SQLSPECIALCOLUMNS_ROWID_BASE_QUERY_PART_2, MAX_CATALOG_QUERY_LEN - strlen(szCatalogQuery));
        if(hNullable == SQL_NO_NULLS)
            addEqualFilterCondition(pStmt, szCatalogQuery, MAX_CATALOG_QUERY_LEN, "a.attnotnull" , "t" , SQL_NTS);   // Condition is in UNION query.
        rs_strncat(szCatalogQuery, SQLSPECIALCOLUMNS_ROWID_BASE_QUERY_PART_3, MAX_CATALOG_QUERY_LEN - strlen(szCatalogQuery));
        addEqualFilterCondition(pStmt, szCatalogQuery, MAX_CATALOG_QUERY_LEN, "n.nspname" , (char *)pSchemaName, cbSchemaName); // Condition is in UNION query.
        addEqualFilterCondition(pStmt, szCatalogQuery, MAX_CATALOG_QUERY_LEN, "c.relname" , (char *)pTableName, cbTableName);   // Condition is in UNION query.
        rs_strncat(szCatalogQuery, SQLSPECIALCOLUMNS_ROWID_BASE_QUERY_PART_4, MAX_CATALOG_QUERY_LEN - strlen(szCatalogQuery));
        addOrderByClause(szCatalogQuery,"SCOPE"); 
    }
    else
    if(hIdenType == SQL_ROWVER)
    {
        rs_strncpy(szCatalogQuery, SQLSPECIALCOLUMNS_ROWVER_BASE_QUERY_PART_1, sizeof(szCatalogQuery));
        addEqualFilterCondition(pStmt, szCatalogQuery, MAX_CATALOG_QUERY_LEN, "n.nspname" , (char *)pSchemaName, cbSchemaName); // Condition is in subquery.
        addEqualFilterCondition(pStmt, szCatalogQuery, MAX_CATALOG_QUERY_LEN, "c.relname" , (char *)pTableName, cbTableName);   // Condition is in subquery.
        rs_strncat(szCatalogQuery, SQLSPECIALCOLUMNS_ROWVER_BASE_QUERY_PART_2, MAX_CATALOG_QUERY_LEN - strlen(szCatalogQuery));
        addEqualFilterCondition(pStmt, szCatalogQuery, MAX_CATALOG_QUERY_LEN, "n.nspname" , (char *)pSchemaName, cbSchemaName); // Condition is in UNION query.
        addEqualFilterCondition(pStmt, szCatalogQuery, MAX_CATALOG_QUERY_LEN, "c.relname" , (char *)pTableName, cbTableName);   // Condition is in UNION query.
        addOrderByClause(szCatalogQuery,"SCOPE"); 
    }
    else
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HY097", "An invalid IdentifierType value was specified", 0, NULL);
        goto error; 
    }

    setCatalogQueryBuf(pStmt, szCatalogQuery);
    rc = RsExecute::RS_SQLExecDirect(phstmt, (SQLCHAR *)szCatalogQuery, SQL_NTS, TRUE, FALSE, FALSE, TRUE);
    resetCatalogQueryFlag(pStmt);

error:

    return rc;

}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Unicode version of  SQLSpecialColumns.
//
SQLRETURN SQL_API SQLSpecialColumnsW(SQLHSTMT       phstmt,
                                        SQLUSMALLINT   hIdenType,
                                        SQLWCHAR*       pwCatalogName,
                                        SQLSMALLINT    cchCatalogName,
                                        SQLWCHAR*      pwSchemaName,
                                        SQLSMALLINT    cchSchemaName,
                                        SQLWCHAR*      pwTableName,
                                        SQLSMALLINT    cchTableName,
                                        SQLUSMALLINT   hScope,
                                        SQLUSMALLINT   hNullable)
{
    SQLRETURN rc;
    char szCatalogName[MAX_IDEN_LEN];
    char szSchemaName[MAX_IDEN_LEN];
    char szTableName[MAX_IDEN_LEN]; 
    
    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLSpecialColumnsW(FUNC_CALL, 0, phstmt, hIdenType, pwCatalogName, cchCatalogName, pwSchemaName, cchSchemaName, pwTableName, cchTableName, hScope, hNullable);

    wchar_to_utf8(pwCatalogName, cchCatalogName, szCatalogName, MAX_IDEN_LEN);
    wchar_to_utf8(pwSchemaName, cchSchemaName, szSchemaName, MAX_IDEN_LEN);
    wchar_to_utf8(pwTableName, cchTableName, szTableName, MAX_IDEN_LEN);


    rc = RsCatalog::RS_SQLSpecialColumns(phstmt, hIdenType,
                                (SQLCHAR *)((pwCatalogName) ? szCatalogName : NULL), SQL_NTS,
                                (SQLCHAR *)((pwSchemaName) ? szSchemaName : NULL), SQL_NTS,
                                (SQLCHAR *)((pwTableName) ? szTableName : NULL), SQL_NTS,
                                hScope,hNullable);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLSpecialColumnsW(FUNC_RETURN, rc, phstmt, hIdenType, pwCatalogName, cchCatalogName, pwSchemaName, cchSchemaName, pwTableName, cchTableName, hScope, hNullable);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLProcedureColumns returns the list of input parameters, as well as the columns that make up the result set 
// for the specified procedures. The driver returns the information as a result set on the specified statement.
//
SQLRETURN SQL_API SQLProcedureColumns(SQLHSTMT           phstmt,
                                        SQLCHAR          *pCatalogName,
                                        SQLSMALLINT      cbCatalogName,
                                        SQLCHAR          *pSchemaName,
                                        SQLSMALLINT      cbSchemaName,
                                        SQLCHAR          *pProcName,
                                        SQLSMALLINT      cbProcName,
                                        SQLCHAR          *pColumnName,
                                        SQLSMALLINT      cbColumnName)
{
    SQLRETURN rc;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLProcedureColumns(FUNC_CALL, 0, phstmt, pCatalogName, cbCatalogName, pSchemaName, cbSchemaName, pProcName, cbProcName, pColumnName, cbColumnName);

    rc = RsCatalog::RS_SQLProcedureColumns(phstmt, pCatalogName, cbCatalogName, pSchemaName, cbSchemaName, pProcName, cbProcName, pColumnName, cbColumnName);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLProcedureColumns(FUNC_RETURN, rc, phstmt, pCatalogName, cbCatalogName, pSchemaName, cbSchemaName, pProcName, cbProcName, pColumnName, cbColumnName);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLProcedureColumns and SQLProcedureColumnsW.
//
SQLRETURN SQL_API RsCatalog::RS_SQLProcedureColumns(SQLHSTMT           phstmt,
                                        SQLCHAR          *pCatalogName,
                                        SQLSMALLINT      cbCatalogName,
                                        SQLCHAR          *pSchemaName,
                                        SQLSMALLINT      cbSchemaName,
                                        SQLCHAR          *pProcName,
                                        SQLSMALLINT      cbProcName,
                                        SQLCHAR          *pColumnName,
                                        SQLSMALLINT      cbColumnName)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    char szCatalogQuery[MAX_CATALOG_QUERY_LEN];
	std::string procedureColQuery = "";
	char catalogFilter[MAX_LARGE_TEMP_BUF_LEN];
	char filterClause[MAX_CATALOG_QUERY_FILTER_LEN];
//	String unknownColumnSize = "2147483647";

    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pStmt->pErrorList = clearErrorList(pStmt->pErrorList);

    if(!checkForValidCatalogName(pStmt, pCatalogName))
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HYC00", "Optional feature not implemented::RS_SQLProcedureColumns", 0, NULL);
        goto error; 
    }

	getCatalogFilterCondition(catalogFilter, MAX_LARGE_TEMP_BUF_LEN, pStmt, (char *)pCatalogName, cbCatalogName,
								TRUE, NULL);

	procedureColQuery.append(
		"SELECT PROCEDURE_CAT , PROCEDURE_SCHEM , PROCEDURE_NAME, COLUMN_NAME, "
		 " COLUMN_TYPE, DATA_TYPE, TYPE_NAME, COLUMN_SIZE , LENGTH AS BUFFER_LENGTH, DECIMAL_DIGITS ,  "
		 " NUM_PREC_RADIX, NULLABLE, REMARKS, COLUMN_DEF, SQL_DATA_TYPE, SQL_DATETIME_SUB, "
		 " CHAR_OCTET_LENGTH, ORDINAL_POSITION, IS_NULLABLE  "
		 " FROM (");

	procedureColQuery.append("SELECT current_database() AS PROCEDURE_CAT, "
		 " n.nspname as PROCEDURE_SCHEM, "
		 " p.proname AS PROCEDURE_NAME, "

		 " CAST(CASE ((array_upper(proargnames, 0) - array_lower(proargnames, 0)) > 0) "
		 " WHEN 't' THEN proargnames[array_upper(proargnames, 1)] "
		 " ELSE '' "
		 " END AS VARCHAR(256)) AS COLUMN_NAME, "

		 " CAST(CASE p.proretset "
		 " WHEN 't' THEN 3 "
		 " ELSE 0 "
		 " END AS SMALLINT) AS COLUMN_TYPE, "
		 " CAST(CASE pg_catalog.format_type(p.prorettype, NULL) "
		 " WHEN 'text' THEN 12 "
		 " WHEN 'bit' THEN  -7 "
		 " WHEN 'bool' THEN  -7 "
		 " WHEN 'boolean' THEN  -7 "
		 " WHEN 'varchar' THEN 12 "
		 " WHEN 'character varying' THEN  12 "
		 " WHEN '\"char\"' THEN 1"
		 " WHEN 'char' THEN  1 "
		 " WHEN 'character' THEN  1 "
		 " WHEN 'nchar' THEN 1 "
		 " WHEN 'bpchar' THEN 1 "
		 " WHEN 'nvarchar' THEN 12 "
		 " WHEN 'date' THEN 91 "
		 " WHEN 'time' THEN 92 "
		 " WHEN 'time without time zone' THEN 92 "
		 " WHEN 'timetz' THEN 2013 "
		 " WHEN 'time with time zone' THEN 2013 "
		 " WHEN 'timestamp' THEN 93 "
		 " WHEN 'timestamp without time zone' THEN 93 "
		 " WHEN 'timestamptz' THEN 2014 "
		 " WHEN 'timestamp with time zone' THEN 2014 "
		 " WHEN 'smallint' THEN 5 "
		 " WHEN 'int2' THEN 5 "
		 " WHEN 'integer' THEN 4 "
		 " WHEN 'int' THEN 4 "
		 " WHEN 'int4' THEN 4 "
		 " WHEN 'bigint' THEN -5 "
		 " WHEN 'int8' THEN -5 "
		 " WHEN 'real' THEN 7 "
		 " WHEN 'float4' THEN 7 "
		 " WHEN 'double precision' THEN 6 "
		 " WHEN 'float8' THEN 6 "
		 " WHEN 'float' THEN 6 "
		 " WHEN 'decimal' THEN 3 "
		 " WHEN 'numeric' THEN 2 "
		 " WHEN '_float4' THEN 2003 "
		 " WHEN '_aclitem' THEN 2003 "
		 " WHEN '_text' THEN 2003 "
		 " WHEN 'bytea' THEN -2 "
		 " WHEN 'oid' THEN -5 "
		 " WHEN 'name' THEN 12 "
		 " WHEN '_int4' THEN 2003 "
		 " WHEN '_int2' THEN 2003 "
		 " WHEN 'ARRAY' THEN 2003 "
		 " WHEN 'geometry' THEN -4 "
		 " WHEN 'super' THEN -1 "
		 " WHEN 'varbyte' THEN -4 "
		 " WHEN 'geography' THEN -4 "
		 " WHEN 'intervaly2m' THEN 107 "
		 " WHEN 'intervald2s' THEN 110 "
		 " ELSE 0 "
		 " END AS SMALLINT) AS DATA_TYPE, "
		 " pg_catalog.format_type(p.prorettype, NULL) AS TYPE_NAME, "
		 " CASE pg_catalog.format_type(p.prorettype, NULL) "
		 " WHEN 'text' THEN NULL "
		 " WHEN 'varchar' THEN NULL "
		 " WHEN 'character varying' THEN NULL "
		 " WHEN '\"char\"' THEN NULL "
		 " WHEN 'character' THEN NULL "
		 " WHEN 'nchar' THEN NULL "
		 " WHEN 'bpchar' THEN NULL "
		 " WHEN 'nvarchar' THEN NULL "
		 " WHEN 'date' THEN 10 "
		 " WHEN 'time' THEN 15 "
		 " WHEN 'time without time zone' THEN 15 "
		 " WHEN 'timetz' THEN 21 "
		 " WHEN 'time with time zone' THEN 21 "
		 " WHEN 'timestamp' THEN 29 "
		 " WHEN 'timestamp without time zone' THEN 29 "
		 " WHEN 'timestamptz' THEN 35 "
		 " WHEN 'timestamp with time zone' THEN 35 "
		 " WHEN 'smallint' THEN 5 "
		 " WHEN 'int2' THEN 5 "
		 " WHEN 'integer' THEN 10 "
		 " WHEN 'int' THEN 10 "
		 " WHEN 'int4' THEN 10 "
		 " WHEN 'bigint' THEN 19 "
		 " WHEN 'int8' THEN 19 "
		 " WHEN 'decimal' THEN 38 "
		 " WHEN 'real' THEN 24 "
		 " WHEN 'float4' THEN 53 "
		 " WHEN 'double precision' THEN 53 "
		 " WHEN 'float8' THEN 53 "
		 " WHEN 'float' THEN 53 "
		 " WHEN 'geometry' THEN NULL "
		 " WHEN 'super' THEN 4194304 "
		 " WHEN 'varbyte' THEN NULL "
		 " WHEN 'geography' THEN NULL "
		 " WHEN 'intervaly2m' THEN 32 "
		 " WHEN 'intervald2s' THEN 64 "
		 " ELSE 2147483647 " 
		 " END AS COLUMN_SIZE, "
		 " CASE pg_catalog.format_type(p.prorettype, NULL) "
		 " WHEN 'text' THEN NULL "
		 " WHEN 'varchar' THEN NULL "
		 " WHEN 'character varying' THEN NULL "
		 " WHEN '\"char\"' THEN NULL "
		 " WHEN 'character' THEN NULL "
		 " WHEN 'nchar' THEN NULL "
		 " WHEN 'bpchar' THEN NULL "
		 " WHEN 'nvarchar' THEN NULL "
		 " WHEN 'date' THEN 6 "
		 " WHEN 'time' THEN 15 "
		 " WHEN 'time without time zone' THEN 15 "
		 " WHEN 'timetz' THEN 21 "
		 " WHEN 'time with time zone' THEN 21 "
		 " WHEN 'timestamp' THEN 6 "
		 " WHEN 'timestamp without time zone' THEN 6 "
		 " WHEN 'timestamptz' THEN 35 "
		 " WHEN 'timestamp with time zone' THEN 35 "
		 " WHEN 'smallint' THEN 2 "
		 " WHEN 'int2' THEN 2 "
		 " WHEN 'integer' THEN 4 "
		 " WHEN 'int' THEN 4 "
		 " WHEN 'int4' THEN 4 "
		 " WHEN 'bigint' THEN 20 "
		 " WHEN 'int8' THEN 20 "
		 " WHEN 'decimal' THEN 8 "
		 " WHEN 'real' THEN 4 "
		 " WHEN 'float4' THEN 8 "
		 " WHEN 'double precision' THEN 8 "
		 " WHEN 'float8' THEN 8 "
		 " WHEN 'float' THEN  8 "
		 " WHEN 'geometry' THEN NULL "
		 " WHEN 'super' THEN 4194304 "
		 " WHEN 'varbyte' THEN NULL "
		 " WHEN 'geography' THEN NULL "
		 " WHEN 'intervaly2m' THEN 32 "
		 " WHEN 'intervald2s' THEN 64 "
		 " END AS LENGTH, "
		 " CAST(CASE pg_catalog.format_type(p.prorettype, NULL) "
		 " WHEN 'smallint' THEN 0 "
		 " WHEN 'int2' THEN 0 "
		 " WHEN 'integer' THEN 0 "
		 " WHEN 'int' THEN 0 "
		 " WHEN 'int4' THEN 0 "
		 " WHEN 'bigint' THEN 0 "
		 " WHEN 'int8' THEN 0 "
		 " WHEN 'decimal' THEN 0 "
		 " WHEN 'real' THEN 8 "
		 " WHEN 'float4' THEN 8 "
		 " WHEN 'double precision' THEN 17 "
		 " WHEN 'float' THEN 17 "
		 " WHEN 'float8' THEN 17 "
		 " WHEN 'time' THEN 6 "
		 " WHEN 'time without time zone' THEN 6 "
		 " WHEN 'timetz' THEN 6 "
		 " WHEN 'time with time zone' THEN 6 "
		 " WHEN 'timestamp' THEN 6 "
		 " WHEN 'timestamp without time zone' THEN 6 "
		 " WHEN 'timestamptz' THEN 6 "
		 " WHEN 'timestamp with time zone' THEN 6 "
		 " WHEN 'intervaly2m' THEN 0 "
		 " WHEN 'intervald2s' THEN 6 "
		 " ELSE NULL END AS SMALLINT) AS DECIMAL_DIGITS, "
		 " 10 AS NUM_PREC_RADIX, "
		 " CAST(2 AS SMALLINT) AS NULLABLE, "
		 " CAST('' AS VARCHAR(256)) AS REMARKS, "
		 " NULL AS COLUMN_DEF, "
		 " CAST(CASE  pg_catalog.format_type(p.prorettype, NULL)"
		 " WHEN 'text' THEN 12 "
		 " WHEN 'bit' THEN  -7 "
		 " WHEN 'bool' THEN  -7 "
		 " WHEN 'boolean' THEN  -7 "
		 " WHEN 'varchar' THEN 12 "
		 " WHEN 'character varying' THEN  12 "
		 " WHEN '\"char\"' THEN 1"
		 " WHEN 'char' THEN  1 "
		 " WHEN 'character' THEN  1 "
		 " WHEN 'nchar' THEN 1 "
		 " WHEN 'bpchar' THEN 1 "
		 " WHEN 'nvarchar' THEN 12 "
		 " WHEN 'date' THEN 91 "
		 " WHEN 'time' THEN 92 "
		 " WHEN 'time without time zone' THEN 92 "
		 " WHEN 'timetz' THEN 2013 "
		 " WHEN 'time with time zone' THEN 2013 "
		 " WHEN 'timestamp' THEN 93 "
		 " WHEN 'timestamp without time zone' THEN 93 "
		 " WHEN 'timestamptz' THEN 2014 "
		 " WHEN 'timestamp with time zone' THEN 2014 "
		 " WHEN 'smallint' THEN 5 "
		 " WHEN 'int2' THEN 5 "
		 " WHEN 'integer' THEN 4 "
		 " WHEN 'int' THEN 4 "
		 " WHEN 'int4' THEN 4 "
		 " WHEN 'bigint' THEN -5 "
		 " WHEN 'int8' THEN -5 "
		 " WHEN 'real' THEN 7 "
		 " WHEN 'float4' THEN 7 "
		 " WHEN 'double precision' THEN 6 "
		 " WHEN 'float8' THEN 6 "
		 " WHEN 'float' THEN 6 "
		 " WHEN 'decimal' THEN 3 "
		 " WHEN 'numeric' THEN 2 "
		 " WHEN 'bytea' THEN -2 "
		 " WHEN 'oid' THEN -5 "
		 " WHEN 'name' THEN 12 "
		 " WHEN 'ARRAY' THEN 2003 "
		 " WHEN 'geometry' THEN -4 "
		 " WHEN 'super' THEN -1 "
		 " WHEN 'varbyte' THEN -4 "
		 " WHEN 'geography' THEN -4 "
		 " WHEN 'intervaly2m' THEN 107 "
		 " WHEN 'intervald2s' THEN 110 "
		 " END AS SMALLINT) AS SQL_DATA_TYPE, "
		 " CAST(NULL AS SMALLINT) AS SQL_DATETIME_SUB, "
		 " CAST(NULL AS SMALLINT) AS CHAR_OCTET_LENGTH, "
		 " CAST(0 AS SMALLINT) AS ORDINAL_POSITION, "
		 " CAST('' AS VARCHAR(256)) AS IS_NULLABLE, "
		 " p.prooid as PROOID, "
		 " -1 AS PROARGINDEX "
		 " FROM pg_catalog.pg_proc_info p LEFT JOIN pg_namespace n ON n.oid = p.pronamespace "
		 " WHERE pg_catalog.format_type(p.prorettype, NULL) != 'void' ");

	procedureColQuery.append(catalogFilter);

	filterClause[0] = '\0';
	addLikeOrEqualFilterCondition(pStmt, filterClause, "n.nspname", (char *)pSchemaName, cbSchemaName);
	addLikeOrEqualFilterCondition(pStmt, filterClause, "proname", (char *)pProcName, cbProcName);
	addLikeOrEqualFilterCondition(pStmt, filterClause, "COLUMN_NAME", (char *)pColumnName, cbColumnName);
	procedureColQuery.append(filterClause);

	procedureColQuery.append(" UNION ALL ");

	procedureColQuery.append(" SELECT DISTINCT current_database() AS PROCEDURE_CAT, "
		 " PROCEDURE_SCHEM, "
		 " PROCEDURE_NAME, "
		 "CAST(CASE (char_length(COLUMN_NAME) > 0) WHEN 't' THEN COLUMN_NAME "
		 "ELSE '' "
		 "END AS VARCHAR(256)) AS COLUMN_NAME, "
		 " CAST( CASE COLUMN_TYPE "
		 " WHEN 105 THEN 1 "
		 " WHEN 98 THEN 2 "
		 " WHEN 111 THEN 4 "
		 " ELSE 0 END AS SMALLINT) AS COLUMN_TYPE, "
		 " CAST(CASE DATA_TYPE "
		 " WHEN 'text' THEN 12 "
		 " WHEN 'bit' THEN  -7 "
		 " WHEN 'bool' THEN  -7 "
		 " WHEN 'boolean' THEN  -7 "
		 " WHEN 'varchar' THEN 12 "
		 " WHEN 'character varying' THEN  12 "
		 " WHEN '\"char\"' THEN  1 "
		 " WHEN 'char' THEN  1 "
		 " WHEN 'character' THEN  1 "
		 " WHEN 'nchar' THEN 1 "
		 " WHEN 'bpchar' THEN 1 "
		 " WHEN 'nvarchar' THEN 12 "
		 " WHEN 'date' THEN 91 "
		 " WHEN 'time' THEN 92 "
		 " WHEN 'time without time zone' THEN 92 "
		 " WHEN 'timetz' THEN 2013 "
		 " WHEN 'time with time zone' THEN 2013 "
		 " WHEN 'timestamp' THEN 93 "
		 " WHEN 'timestamp without time zone' THEN 93 "
		 " WHEN 'timestamptz' THEN 2014 "
		 " WHEN 'timestamp with time zone' THEN 2014 "
		 " WHEN 'smallint' THEN 5 "
		 " WHEN 'int2' THEN 5 "
		 " WHEN 'integer' THEN 4 "
		 " WHEN 'int' THEN 4 "
		 " WHEN 'int4' THEN 4 "
		 " WHEN 'bigint' THEN -5 "
		 " WHEN 'int8' THEN -5 "
		 " WHEN 'real' THEN 7 "
		 " WHEN 'float4' THEN 7 "
		 " WHEN 'double precision' THEN 6 "
		 " WHEN 'float8' THEN 6 "
		 " WHEN 'float' THEN 6 "
		 " WHEN 'decimal' THEN 3 "
		 " WHEN 'numeric' THEN 2 "
		 " WHEN 'bytea' THEN -2 "
		 " WHEN 'oid' THEN -5 "
		 " WHEN 'name' THEN 12 "
		 " WHEN 'ARRAY' THEN 2003 "
		 " WHEN 'geometry' THEN -4 "
		 " WHEN 'super' THEN -1 "
		 " WHEN 'varbyte' THEN -4 "
		 " WHEN 'geography' THEN -4 "
		 " WHEN 'intervaly2m' THEN 107 "
		 " WHEN 'intervald2s' THEN 110 "
		 " ELSE 0 "
		 " END AS SMALLINT) AS DATA_TYPE, "
		 " TYPE_NAME, "
		 " CASE COLUMN_SIZE "
		 " WHEN 'text' THEN COLUMN_BYTES "
		 " WHEN 'varchar' THEN COLUMN_BYTES "
		 " WHEN 'character varying' THEN COLUMN_BYTES "
		 " WHEN '\"char\"' THEN COLUMN_BYTES "
		 " WHEN 'character' THEN COLUMN_BYTES "
		 " WHEN 'nchar' THEN COLUMN_BYTES "
		 " WHEN 'bpchar' THEN COLUMN_BYTES "
		 " WHEN 'nvarchar' THEN COLUMN_BYTES "
		 " WHEN 'date' THEN 10 "
		 " WHEN 'time' THEN 15 "
		 " WHEN 'time without time zone' THEN 15 "
		 " WHEN 'timetz' THEN 21 "
		 " WHEN 'time with time zone' THEN 21 "
		 " WHEN 'timestamp' THEN 6 "
		 " WHEN 'timestamp without time zone' THEN 6 "
		 " WHEN 'timestamptz' THEN 35 "
		 " WHEN 'timestamp with time zone' THEN 35 "
		 " WHEN 'smallint' THEN 5 "
		 " WHEN 'int2' THEN 5 "
		 " WHEN 'integer' THEN 10 "
		 " WHEN 'int' THEN 10 "
		 " WHEN 'int4' THEN 10 "
		 " WHEN 'bigint' THEN 19 "
		 " WHEN 'int8' THEN 19 "
		 " WHEN 'decimal' THEN 38 "
		 " WHEN 'real' THEN 24 "
		 " WHEN 'float4' THEN 53 "
		 " WHEN 'double precision' THEN 53 "
		 " WHEN 'float8' THEN 53 "
		 " WHEN 'float' THEN 53 "
		 " WHEN 'numeric' THEN NUMERIC_PRECISION "
		 " WHEN 'geometry' THEN NULL "
		 " WHEN 'super' THEN 4194304 "
		 " WHEN 'varbyte' THEN NULL "
		 " WHEN 'geography' THEN NULL "
		 " WHEN 'intervaly2m' THEN 32 "
		 " WHEN 'intervald2s' THEN 64 "
		 " ELSE 2147483647 "
		 " END AS COLUMN_SIZE, "
		 " CASE LENGTH "
		 " WHEN 'text' THEN COLUMN_BYTES "
		 " WHEN 'varchar' THEN COLUMN_BYTES "
		 " WHEN 'character varying' THEN COLUMN_BYTES "
		 " WHEN '\"char\"' THEN COLUMN_BYTES "
		 " WHEN 'character' THEN COLUMN_BYTES "
		 " WHEN 'nchar' THEN COLUMN_BYTES "
		 " WHEN 'bpchar' THEN COLUMN_BYTES "
		 " WHEN 'nvarchar' THEN COLUMN_BYTES "
		 " WHEN 'date' THEN 6 "
		 " WHEN 'time' THEN 6 "
		 " WHEN 'time without time zone' THEN 6 "
		 " WHEN 'timetz' THEN 6 "
		 " WHEN 'time with time zone' THEN 6 "
		 " WHEN 'timestamp' THEN 6 "
		 " WHEN 'timestamp without time zone' THEN 6 "
		 " WHEN 'timestamptz' THEN 6 "
		 " WHEN 'timestamp with time zone' THEN 6 "
		 " WHEN 'smallint' THEN 2 "
		 " WHEN 'int2' THEN 2 "
		 " WHEN 'integer' THEN 4 "
		 " WHEN 'int' THEN 4 "
		 " WHEN 'int4' THEN 4 "
		 " WHEN 'bigint' THEN 20 "
		 " WHEN 'int8' THEN 20 "
		 " WHEN 'decimal' THEN 8 "
		 " WHEN 'real' THEN 4 "
		 " WHEN 'float4' THEN 8 "
		 " WHEN 'double precision' THEN 8 "
		 " WHEN 'float8' THEN 8 "
		 " WHEN 'float' THEN  8 "
		 " WHEN 'geometry' THEN NULL "
		 " WHEN 'super' THEN 4194304 "
		 " WHEN 'varbyte' THEN NULL "
		 " WHEN 'geography' THEN NULL "
		 " WHEN 'intervaly2m' THEN 32 "
		 " WHEN 'intervald2s' THEN 64 "
		 " END AS LENGTH, "
		 " CAST(CASE DECIMAL_DIGITS "
		 " WHEN 'smallint' THEN 0 "
		 " WHEN 'int2' THEN 0 "
		 " WHEN 'integer' THEN 0 "
		 " WHEN 'int' THEN 0 "
		 " WHEN 'int4' THEN 0 "
		 " WHEN 'bigint' THEN 0 "
		 " WHEN 'int8' THEN 0 "
		 " WHEN 'decimal' THEN 0 "
		 " WHEN 'real' THEN 8 "
		 " WHEN 'float4' THEN 8 "
		 " WHEN 'double precision' THEN 17 "
		 " WHEN 'float' THEN 17 "
		 " WHEN 'float8' THEN 17 "
		 " WHEN 'numeric' THEN NUMERIC_SCALE "
		 " WHEN 'time' THEN 6 "
		 " WHEN 'time without time zone' THEN 6 "
		 " WHEN 'timetz' THEN 6 "
		 " WHEN 'time with time zone' THEN 6 "
		 " WHEN 'timestamp' THEN 6 "
		 " WHEN 'timestamp without time zone' THEN 6 "
		 " WHEN 'timestamptz' THEN 6 "
		 " WHEN 'timestamp with time zone' THEN 6 "
		 " WHEN 'intervaly2m' THEN 0 "
		 " WHEN 'intervald2s' THEN 6 "
		 " ELSE NULL END AS SMALLINT) AS DECIMAL_DIGITS, "
		 " 10 AS NUM_PREC_RADIX, "
		 " CAST(2 AS SMALLINT) AS NULLABLE, "
		 " CAST(''AS VARCHAR(256)) AS REMARKS, "
		 " NULL AS COLUMN_DEF,"
		 " CAST( CASE SQL_DATA_TYPE"
		 " WHEN 'text' THEN 12 "
		 " WHEN 'bit' THEN  -7 "
		 " WHEN 'bool' THEN  -7 "
		 " WHEN 'boolean' THEN  -7 "
		 " WHEN 'varchar' THEN 12 "
		 " WHEN 'character varying' THEN  12 "
		 " WHEN '\"char\"' THEN  1 "
		 " WHEN 'char' THEN  1 "
		 " WHEN 'character' THEN  1 "
		 " WHEN 'nchar' THEN 1 "
		 " WHEN 'bpchar' THEN 1 "
		 " WHEN 'nvarchar' THEN 12 "
		 " WHEN 'date' THEN 91 "
		 " WHEN 'time' THEN 92 "
		 " WHEN 'time without time zone' THEN 92 "
		 " WHEN 'timetz' THEN 2013 "
		 " WHEN 'time with time zone' THEN 2013 "
		 " WHEN 'timestamp' THEN 93 "
		 " WHEN 'timestamp without time zone' THEN 93 "
		 " WHEN 'timestamptz' THEN 2014 "
		 " WHEN 'timestamp with time zone' THEN 2014 "
		 " WHEN 'smallint' THEN 5 "
		 " WHEN 'int2' THEN 5 "
		 " WHEN 'integer' THEN 4 "
		 " WHEN 'int' THEN 4 "
		 " WHEN 'int4' THEN 4 "
		 " WHEN 'bigint' THEN -5 "
		 " WHEN 'int8' THEN -5 "
		 " WHEN 'real' THEN 7 "
		 " WHEN 'float4' THEN 7 "
		 " WHEN 'double precision' THEN 6 "
		 " WHEN 'float8' THEN 6 "
		 " WHEN 'float' THEN 6 "
		 " WHEN 'decimal' THEN 3 "
		 " WHEN 'numeric' THEN 2 "
		 " WHEN 'bytea' THEN -2 "
		 " WHEN 'oid' THEN -5 "
		 " WHEN 'name' THEN 12 "
		 " WHEN 'ARRAY' THEN 2003 "
		 " WHEN 'geometry' THEN -4 "
		 " WHEN 'super' THEN -1 "
		 " WHEN 'varbyte' THEN -4 "
		 " WHEN 'geography' THEN -4 "
		 " WHEN 'intervaly2m' THEN 107 "
		 " WHEN 'intervald2s' THEN 110 "
		 " END AS SMALLINT) AS SQL_DATA_TYPE, "
		 " CAST(NULL AS SMALLINT) AS SQL_DATETIME_SUB, "
		 " CAST(NULL AS SMALLINT) AS CHAR_OCTET_LENGTH, "
		 " PROARGINDEX AS ORDINAL_POSITION, "
		 " CAST(''AS VARCHAR(256)) AS IS_NULLABLE, "
		 " PROOID, PROARGINDEX "
		 " FROM ( "
		 " SELECT current_database() AS PROCEDURE_CAT,"
		 " n.nspname AS PROCEDURE_SCHEM, "
		 " proname AS PROCEDURE_NAME, "
		 " CASE WHEN (proallargtypes is NULL) THEN proargnames[pos+1] "
		 " ELSE proargnames[pos] END AS COLUMN_NAME,"
		 " CASE WHEN proargmodes is NULL THEN 105 "
		 " ELSE CAST(proargmodes[pos] AS INT) END AS COLUMN_TYPE, "
		 " CASE WHEN proallargtypes is NULL THEN pg_catalog.format_type(proargtypes[pos], NULL)"
		 " ELSE pg_catalog.format_type(proallargtypes[pos], NULL) END AS DATA_TYPE,"
		 " CASE WHEN proallargtypes is NULL THEN pg_catalog.format_type(proargtypes[pos], NULL) "
		 " ELSE pg_catalog.format_type(proallargtypes[pos], NULL) END AS TYPE_NAME,"
		 " CASE WHEN proallargtypes is NULL THEN pg_catalog.format_type(proargtypes[pos], NULL)"
		 " ELSE pg_catalog.format_type(proallargtypes[pos], NULL) END AS COLUMN_SIZE,"
		 " CASE  WHEN (proallargtypes IS NOT NULL) and prokind='p' AND proallargtypes[pos] IN (1042, 1700, 1043) "
		 "				THEN (string_to_array(textin(byteaout(substring(probin from 1 for length(probin)-3))),','))[pos]::integer "
		 " 			WHEN (proallargtypes IS NULL) AND prokind='p' AND proargtypes[pos] IN (1042,1700,1043) "
		 "				THEN (string_to_array(textin(byteaout(substring(probin FROM 1 FOR length(probin)-3))), ',')) [pos+1]::integer "
		 " END AS PROBIN_BYTES, "
		 " CASE "
		 "   WHEN (PROBIN_BYTES IS NOT NULL) "
		 " 				AND (proallargtypes[pos] IN (1042, 1043) or proargtypes[pos] in (1042,1043)) "
		 "		THEN PROBIN_BYTES-4 "
		 " END AS COLUMN_BYTES, "
		 " CASE WHEN proallargtypes is NULL THEN pg_catalog.format_type(proargtypes[pos], NULL)"
		 " ELSE pg_catalog.format_type(proallargtypes[pos], NULL) END AS LENGTH,"
		 " CASE WHEN proallargtypes is NULL THEN pg_catalog.format_type(proargtypes[pos], NULL)"
		 " ELSE pg_catalog.format_type(proallargtypes[pos], NULL) END AS DECIMAL_DIGITS,"
		 " CASE WHEN proallargtypes is NULL THEN pg_catalog.format_type(proargtypes[pos], NULL)"
		 " ELSE pg_catalog.format_type(proallargtypes[pos], NULL) END AS RADIX,"
		 " CAST(2 AS SMALLINT) AS NULLABLE,"
		 " CAST(''AS VARCHAR(256)) AS REMARKS,"
		 " CAST(NULL AS SMALLINT) AS COLUMN_DEF,"
		 " pg_catalog.format_type(proargtypes[pos], NULL) AS SQL_DATA_TYPE,"
		 " CAST(NULL AS SMALLINT) AS SQL_DATETIME_SUB,"
		 " pg_catalog.format_type(proargtypes[pos], NULL) AS CHAR_OCTET_LENGTH,"
		 " CASE WHEN (proallargtypes is NULL) THEN pos+1"
		 " WHEN pos = array_upper(proallargtypes, 1) THEN 0"
		 " ELSE pos END AS ORDINAL_POSITION,"
		 " CAST('' AS VARCHAR(256)) AS IS_NULLABLE,"
		 " p.prooid AS PROOID,"
		 " CASE WHEN (proallargtypes is NULL) THEN pos+1"
		 " WHEN prokind = 'f' AND pos = array_upper(proallargtypes, 1) THEN 0"
		 " ELSE pos END AS PROARGINDEX, "
		 " CASE WHEN (proallargtypes IS NULL AND proargtypes[pos] = 1700 AND prokind='p') OR (proallargtypes IS NOT NULL AND proallargtypes[pos] = 1700 AND prokind='p' AND proallargtypes[pos] = 1700) THEN (PROBIN_BYTES-4)/65536 END as NUMERIC_PRECISION, "
		 " CASE WHEN (proallargtypes IS NULL AND proargtypes[pos] = 1700 AND prokind='p') OR (proallargtypes IS NOT NULL AND proallargtypes[pos] = 1700 AND prokind='p' AND proallargtypes[pos] = 1700) THEN (((PROBIN_BYTES::numeric-4)/65536 - (PROBIN_BYTES-4)/65536) *  65536)::INT END as NUMERIC_SCALE, "
		 " p.proname || '_' || p.prooid AS SPECIFIC_NAME "
		 " FROM (pg_catalog.pg_proc_info p LEFT JOIN pg_namespace n"
		 " ON n.oid = p.pronamespace)"
		 " LEFT JOIN (SELECT "
		 " CASE WHEN (proallargtypes IS NULL) "
		 " THEN generate_series(array_lower(proargnames, 1), array_upper(proargnames, 1))-1"
		 " ELSE generate_series(array_lower(proargnames, 1), array_upper(proargnames, 1)+1)-1 "
		 " END AS pos"
		 " FROM pg_catalog.pg_proc_info p ) AS s ON (pos >= 0)");

	procedureColQuery.append(" WHERE true ");

	procedureColQuery.append(catalogFilter);

	filterClause[0] = '\0';
	addLikeOrEqualFilterCondition(pStmt, filterClause, "n.nspname", (char *)pSchemaName, cbSchemaName);
	addLikeOrEqualFilterCondition(pStmt, filterClause, "proname", (char *)pProcName, cbProcName);
	addLikeOrEqualFilterCondition(pStmt, filterClause, "COLUMN_NAME", (char *)pColumnName, cbColumnName);
	procedureColQuery.append(filterClause);

	procedureColQuery.append(" ) AS INPUT_PARAM_TABLE"
		 " WHERE ORDINAL_POSITION IS NOT NULL"
		 " ) AS RESULT_SET WHERE (DATA_TYPE != 0 OR (TYPE_NAME IS NOT NULL AND TYPE_NAME != '-'))"
		 " ORDER BY PROCEDURE_CAT ,PROCEDURE_SCHEM,"
		 " PROCEDURE_NAME, PROARGINDEX, COLUMN_TYPE DESC"); // PROOID


	rs_strncpy(szCatalogQuery, procedureColQuery.c_str(), sizeof(szCatalogQuery));

    setCatalogQueryBuf(pStmt, szCatalogQuery);
    rc = RsExecute::RS_SQLExecDirect(phstmt, (SQLCHAR *)szCatalogQuery, SQL_NTS, TRUE, FALSE, FALSE, TRUE);
    resetCatalogQueryFlag(pStmt);

error:

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Unicoed version of SQLProcedureColumns.
//
SQLRETURN SQL_API SQLProcedureColumnsW(SQLHSTMT     phstmt,
                                        SQLWCHAR*    pwCatalogName,
                                        SQLSMALLINT  cchCatalogName,
                                        SQLWCHAR*    pwSchemaName,
                                        SQLSMALLINT  cchSchemaName,
                                        SQLWCHAR*    pwProcName,
                                        SQLSMALLINT  cchProcName,
                                        SQLWCHAR*    pwColumnName,
                                        SQLSMALLINT  cchColumnName)
{
    SQLRETURN rc;
    char szCatalogName[MAX_IDEN_LEN];
    char szSchemaName[MAX_IDEN_LEN];
    char szProcName[MAX_IDEN_LEN]; 
    char szColumnName[MAX_IDEN_LEN]; 
    
    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLProcedureColumnsW(FUNC_CALL, 0, phstmt, pwCatalogName, cchCatalogName, pwSchemaName, cchSchemaName, pwProcName, cchProcName, pwColumnName, cchColumnName);

    wchar_to_utf8(pwCatalogName, cchCatalogName, szCatalogName, MAX_IDEN_LEN);
    wchar_to_utf8(pwSchemaName, cchSchemaName, szSchemaName, MAX_IDEN_LEN);
    wchar_to_utf8(pwProcName, cchProcName, szProcName, MAX_IDEN_LEN);
    wchar_to_utf8(pwColumnName, cchColumnName, szColumnName, MAX_IDEN_LEN);

    rc = RsCatalog::RS_SQLProcedureColumns(phstmt, (SQLCHAR *)((pwCatalogName) ? szCatalogName : NULL), SQL_NTS,
                                        (SQLCHAR *)((pwSchemaName) ? szSchemaName : NULL), SQL_NTS,
                                        (SQLCHAR *)((pwProcName) ? szProcName : NULL), SQL_NTS,
                                        (SQLCHAR *)((pwColumnName) ? szColumnName : NULL), SQL_NTS);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLProcedureColumnsW(FUNC_RETURN, rc, phstmt, pwCatalogName, cchCatalogName, pwSchemaName, cchSchemaName, pwProcName, cchProcName, pwColumnName, cchColumnName);

    return rc;
}
/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLProcedures returns the list of procedure names stored in a specific data source.
//
SQLRETURN SQL_API SQLProcedures(SQLHSTMT           phstmt,
                                SQLCHAR           *pCatalogName,
                                SQLSMALLINT        cbCatalogName,
                                SQLCHAR           *pSchemaName,
                                SQLSMALLINT        cbSchemaName,
                                SQLCHAR           *pProcName,
                                SQLSMALLINT        cbProcName)
{
    SQLRETURN rc;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLProcedures(FUNC_CALL, 0, phstmt, pCatalogName, cbCatalogName, pSchemaName, cbSchemaName, pProcName, cbProcName);

    rc = RsCatalog::RS_SQLProcedures(phstmt, pCatalogName, cbCatalogName, pSchemaName, cbSchemaName, pProcName, cbProcName);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLProcedures(FUNC_RETURN, rc, phstmt, pCatalogName, cbCatalogName, pSchemaName, cbSchemaName, pProcName, cbProcName);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLProcedures and SQLProceduresW.
//
SQLRETURN SQL_API RsCatalog::RS_SQLProcedures(SQLHSTMT           phstmt,
                                SQLCHAR           *pCatalogName,
                                SQLSMALLINT        cbCatalogName,
                                SQLCHAR           *pSchemaName,
                                SQLSMALLINT        cbSchemaName,
                                SQLCHAR           *pProcName,
                                SQLSMALLINT        cbProcName)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    char szCatalogQuery[MAX_CATALOG_QUERY_LEN];
	char catalogFilter[MAX_LARGE_TEMP_BUF_LEN];
	char filterClause[MAX_CATALOG_QUERY_FILTER_LEN];
	int len;


    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pStmt->pErrorList = clearErrorList(pStmt->pErrorList);

    if(!checkForValidCatalogName(pStmt, pCatalogName))
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HYC00", "Optional feature not implemented::RS_SQLProcedures", 0, NULL);
        goto error; 
    }

	rs_strncpy(szCatalogQuery, SQLPROCEDURES_BASE_QUERY, sizeof(szCatalogQuery));

	getCatalogFilterCondition(catalogFilter, MAX_LARGE_TEMP_BUF_LEN, pStmt, (char *)pCatalogName, cbCatalogName,
								TRUE, NULL);

	filterClause[0] = '\0';
    addLikeOrEqualFilterCondition(pStmt, filterClause, "n.nspname" , (char *)pSchemaName, cbSchemaName);
	if (filterClause[0] == '\0')
	{
		/* limit to current schema if no schema given */
		rs_strncpy(filterClause, "and pg_function_is_visible(p.prooid)", sizeof(filterClause));
	}

    addLikeOrEqualFilterCondition(pStmt, filterClause, "p.proname" , (char *)pProcName, cbProcName);
	len = strlen(szCatalogQuery);
	snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, "%s", catalogFilter);
	len = strlen(szCatalogQuery);
	snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, "%s", filterClause);

    addOrderByClause(szCatalogQuery,"PROCEDURE_SCHEM,PROCEDURE_NAME"); 

    setCatalogQueryBuf(pStmt, szCatalogQuery);
    rc = RsExecute::RS_SQLExecDirect(phstmt, (SQLCHAR *)szCatalogQuery, SQL_NTS, TRUE, FALSE, FALSE, TRUE);
    resetCatalogQueryFlag(pStmt);

error:

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Unicode version of SQLProcedures.
//
SQLRETURN SQL_API SQLProceduresW(SQLHSTMT    phstmt,
                                SQLWCHAR*    pwCatalogName,
                                SQLSMALLINT  cchCatalogName,
                                SQLWCHAR*    pwSchemaName,
                                SQLSMALLINT  cchSchemaName,
                                SQLWCHAR*    pwProcName,
                                SQLSMALLINT  cchProcName)
{
    SQLRETURN rc;
    char szCatalogName[MAX_IDEN_LEN];
    char szSchemaName[MAX_IDEN_LEN];
    char szProcName[MAX_IDEN_LEN]; 
    
    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLProceduresW(FUNC_CALL, 0, phstmt, pwCatalogName, cchCatalogName, pwSchemaName, cchSchemaName, pwProcName, cchProcName);

    wchar_to_utf8(pwCatalogName, cchCatalogName, szCatalogName, MAX_IDEN_LEN);
    wchar_to_utf8(pwSchemaName, cchSchemaName, szSchemaName, MAX_IDEN_LEN);
    wchar_to_utf8(pwProcName, cchProcName, szProcName, MAX_IDEN_LEN);

    rc = RsCatalog::RS_SQLProcedures(phstmt, (SQLCHAR *)((pwCatalogName) ? szCatalogName : NULL), SQL_NTS,
                                  (SQLCHAR *)((pwSchemaName) ? szSchemaName : NULL), SQL_NTS,
                                  (SQLCHAR *)((pwProcName) ? szProcName : NULL), SQL_NTS);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLProceduresW(FUNC_RETURN, rc, phstmt, pwCatalogName, cchCatalogName, pwSchemaName, cchSchemaName, pwProcName, cchProcName);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLForeignKeys can return:
//  A list of foreign keys in the specified table (columns in the specified table that refer to primary keys 
//   in other tables).
//  A list of foreign keys in other tables that refer to the primary key in the specified table.
// The driver returns each list as a result set on the specified statement. 
//
SQLRETURN SQL_API SQLForeignKeys(SQLHSTMT               phstmt,
                                    SQLCHAR           *pPkCatalogName,
                                    SQLSMALLINT        cbPkCatalogName,
                                    SQLCHAR           *pPkSchemaName,
                                    SQLSMALLINT        cbPkSchemaName,
                                    SQLCHAR           *pPkTableName,
                                    SQLSMALLINT        cbPkTableName,
                                    SQLCHAR           *pFkCatalogName,
                                    SQLSMALLINT        cbFkCatalogName,
                                    SQLCHAR           *pFkSchemaName,
                                    SQLSMALLINT        cbFkSchemaName,
                                    SQLCHAR           *pFkTableName,
                                    SQLSMALLINT        cbFkTableName)
{
    SQLRETURN rc;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLForeignKeys(FUNC_CALL, 0, phstmt, pPkCatalogName, cbPkCatalogName, pPkSchemaName, cbPkSchemaName, pPkTableName, cbPkTableName,
                                pFkCatalogName, cbFkCatalogName, pFkSchemaName, cbFkSchemaName, pFkTableName, cbFkTableName);

    rc = RsCatalog::RS_SQLForeignKeys(phstmt, pPkCatalogName, cbPkCatalogName, pPkSchemaName, cbPkSchemaName, pPkTableName, cbPkTableName,
                            pFkCatalogName, cbFkCatalogName, pFkSchemaName, cbFkSchemaName, pFkTableName, cbFkTableName);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLForeignKeys(FUNC_RETURN, rc, phstmt, pPkCatalogName, cbPkCatalogName, pPkSchemaName, cbPkSchemaName, pPkTableName, cbPkTableName,
                                pFkCatalogName, cbFkCatalogName, pFkSchemaName, cbFkSchemaName, pFkTableName, cbFkTableName);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLForeignKeys and SQLForeignKeysW.
//
SQLRETURN SQL_API RsCatalog::RS_SQLForeignKeys(SQLHSTMT               phstmt,
                                    SQLCHAR           *pPkCatalogName,
                                    SQLSMALLINT        cbPkCatalogName,
                                    SQLCHAR           *pPkSchemaName,
                                    SQLSMALLINT        cbPkSchemaName,
                                    SQLCHAR           *pPkTableName,
                                    SQLSMALLINT        cbPkTableName,
                                    SQLCHAR           *pFkCatalogName,
                                    SQLSMALLINT        cbFkCatalogName,
                                    SQLCHAR           *pFkSchemaName,
                                    SQLSMALLINT        cbFkSchemaName,
                                    SQLCHAR           *pFkTableName,
                                    SQLSMALLINT        cbFkTableName)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    char szCatalogQuery[MAX_CATALOG_QUERY_LEN];
	char catalogFilter[MAX_LARGE_TEMP_BUF_LEN];
	char filterClause[MAX_CATALOG_QUERY_FILTER_LEN];
	int len;

    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pStmt->pErrorList = clearErrorList(pStmt->pErrorList);

    if(!checkForValidCatalogName(pStmt, pPkCatalogName)
        || !checkForValidCatalogName(pStmt, pFkCatalogName)
        )
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HYC00", "Optional feature not implemented::RS_SQLForeignKeys", 0, NULL);
        goto error; 
    }

	rs_strncpy(szCatalogQuery, SQLFOREIGN_KEYS_BASE_QUERY, sizeof(szCatalogQuery));

	getCatalogFilterCondition(catalogFilter, MAX_LARGE_TEMP_BUF_LEN, pStmt, (char *)pPkCatalogName, cbPkCatalogName,
								TRUE, NULL);

	filterClause[0] = '\0';
    addEqualFilterCondition(pStmt, filterClause, MAX_CATALOG_QUERY_FILTER_LEN, "PKTABLE_SCHEM" , (char *)pPkSchemaName, cbPkSchemaName);
    addEqualFilterCondition(pStmt, filterClause, MAX_CATALOG_QUERY_FILTER_LEN, "PKTABLE_NAME" , (char *)pPkTableName, cbPkTableName);
    addEqualFilterCondition(pStmt, filterClause, MAX_CATALOG_QUERY_FILTER_LEN, "FKTABLE_SCHEM" , (char *)pFkSchemaName, cbFkSchemaName);
    addEqualFilterCondition(pStmt, filterClause, MAX_CATALOG_QUERY_FILTER_LEN, "FKTABLE_NAME" , (char *)pFkTableName, cbFkTableName);

	len = strlen(szCatalogQuery);
	snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, "%s", catalogFilter);
	len = strlen(szCatalogQuery);
	snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, "%s", filterClause);

    addOrderByClause(szCatalogQuery,"FKTABLE_SCHEM,FKTABLE_NAME,KEY_SEQ"); 

    setCatalogQueryBuf(pStmt, szCatalogQuery);
    rc = RsExecute::RS_SQLExecDirect(phstmt, (SQLCHAR *)szCatalogQuery, SQL_NTS, TRUE, FALSE, FALSE, TRUE);
    resetCatalogQueryFlag(pStmt);

error:

    return rc;
}


/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Unicode version of SQLForeignKeys.
//
SQLRETURN SQL_API SQLForeignKeysW(SQLHSTMT         phstmt,
                                    SQLWCHAR*    pwPkCatalogName,
                                    SQLSMALLINT  cchPkCatalogName,
                                    SQLWCHAR*    pwPkSchemaName,
                                    SQLSMALLINT  cchPkSchemaName,
                                    SQLWCHAR*    pwPkTableName,
                                    SQLSMALLINT  cchPkTableName,
                                    SQLWCHAR*    pwFkCatalogName,
                                    SQLSMALLINT  cchFkCatalogName,
                                    SQLWCHAR*    pwFkSchemaName,
                                    SQLSMALLINT  cchFkSchemaName,
                                    SQLWCHAR*    pwFkTableName,
                                    SQLSMALLINT  cchFkTableName)
{
    SQLRETURN rc;
    char szPkCatalogName[MAX_IDEN_LEN];
    char szPkSchemaName[MAX_IDEN_LEN];
    char szPkTableName[MAX_IDEN_LEN]; 
    char szFkCatalogName[MAX_IDEN_LEN];
    char szFkSchemaName[MAX_IDEN_LEN];
    char szFkTableName[MAX_IDEN_LEN]; 

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLForeignKeysW(FUNC_CALL, 0, phstmt, pwPkCatalogName, cchPkCatalogName, pwPkSchemaName, cchPkSchemaName, pwPkTableName, cchPkTableName,
                                pwFkCatalogName, cchFkCatalogName, pwFkSchemaName, cchFkSchemaName, pwFkTableName, cchFkTableName);

    wchar_to_utf8(pwPkCatalogName, cchPkCatalogName, szPkCatalogName, MAX_IDEN_LEN);
    wchar_to_utf8(pwPkSchemaName, cchPkSchemaName, szPkSchemaName, MAX_IDEN_LEN);
    wchar_to_utf8(pwPkTableName, cchPkTableName, szPkTableName, MAX_IDEN_LEN);

    wchar_to_utf8(pwFkCatalogName, cchFkCatalogName, szFkCatalogName, MAX_IDEN_LEN);
    wchar_to_utf8(pwFkSchemaName, cchFkSchemaName, szFkSchemaName, MAX_IDEN_LEN);
    wchar_to_utf8(pwFkTableName, cchFkTableName, szFkTableName, MAX_IDEN_LEN);

    rc = RsCatalog::RS_SQLForeignKeys(phstmt, (SQLCHAR *)((pwPkCatalogName) ? szPkCatalogName : NULL), SQL_NTS,
                                   (SQLCHAR *)((pwPkSchemaName)  ? szPkSchemaName  : NULL), SQL_NTS,
                                   (SQLCHAR *)((pwPkTableName)   ? szPkTableName   : NULL), SQL_NTS,
                                   (SQLCHAR *)((pwFkCatalogName) ? szFkCatalogName : NULL), SQL_NTS,
                                   (SQLCHAR *)((pwFkSchemaName)  ? szFkSchemaName  : NULL), SQL_NTS,
                                   (SQLCHAR *)((pwFkTableName)   ? szFkTableName   : NULL), SQL_NTS);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLForeignKeysW(FUNC_RETURN, rc, phstmt, pwPkCatalogName, cchPkCatalogName, pwPkSchemaName, cchPkSchemaName, pwPkTableName, cchPkTableName,
                                pwFkCatalogName, cchFkCatalogName, pwFkSchemaName, cchFkSchemaName, pwFkTableName, cchFkTableName);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLPrimaryKeys returns the column names that make up the primary key for a table. 
// The driver returns the information as a result set. 
// This function does not support returning primary keys from multiple tables in a single call.
//
SQLRETURN SQL_API SQLPrimaryKeys(SQLHSTMT           phstmt,
                                    SQLCHAR         *pCatalogName,
                                    SQLSMALLINT     cbCatalogName,
                                    SQLCHAR         *pSchemaName,
                                    SQLSMALLINT     cbSchemaName,
                                    SQLCHAR         *pTableName,
                                    SQLSMALLINT     cbTableName)

{
    SQLRETURN rc;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLPrimaryKeys(FUNC_CALL, 0, phstmt, pCatalogName, cbCatalogName, pSchemaName, cbSchemaName, pTableName, cbTableName);

    rc = RsCatalog::RS_SQLPrimaryKeys(phstmt, pCatalogName, cbCatalogName, pSchemaName, cbSchemaName, pTableName, cbTableName);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLPrimaryKeys(FUNC_RETURN, rc, phstmt, pCatalogName, cbCatalogName, pSchemaName, cbSchemaName, pTableName, cbTableName);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLPrimaryKeys and SQLPrimaryKeysW.
//
SQLRETURN SQL_API RsCatalog::RS_SQLPrimaryKeys(SQLHSTMT           phstmt,
                                    SQLCHAR         *pCatalogName,
                                    SQLSMALLINT     cbCatalogName,
                                    SQLCHAR         *pSchemaName,
                                    SQLSMALLINT     cbSchemaName,
                                    SQLCHAR         *pTableName,
                                    SQLSMALLINT     cbTableName)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    char szCatalogQuery[MAX_CATALOG_QUERY_LEN];
	char catalogFilter[MAX_LARGE_TEMP_BUF_LEN];
	char filterClause[MAX_CATALOG_QUERY_FILTER_LEN];
	int len;

    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pStmt->pErrorList = clearErrorList(pStmt->pErrorList);

    if(!checkForValidCatalogName(pStmt, pCatalogName))
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HYC00", "Optional feature not implemented::RS_SQLPrimaryKeys", 0, NULL);
        goto error; 
    }

    if(pTableName == NULL || cbTableName == SQL_NULL_DATA)
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HY009", "Invalid use of null pointer", 0, NULL);
        goto error; 
    }

	rs_strncpy(szCatalogQuery, SQLPRIMARY_KEYS_BASE_QUERY,sizeof(szCatalogQuery));

	getCatalogFilterCondition(catalogFilter, MAX_LARGE_TEMP_BUF_LEN, pStmt, (char *)pCatalogName, cbCatalogName,
								TRUE, NULL);

	filterClause[0] = '\0';
    addEqualFilterCondition(pStmt, filterClause, MAX_CATALOG_QUERY_FILTER_LEN, "n.nspname" , (char *)pSchemaName, cbSchemaName);
    addEqualFilterCondition(pStmt, filterClause, MAX_CATALOG_QUERY_FILTER_LEN, "ct.relname" , (char *)pTableName, cbTableName);

	len = strlen(szCatalogQuery);
	snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, "%s", catalogFilter);
	len = strlen(szCatalogQuery);
	snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, "%s", filterClause);

    addOrderByClause(szCatalogQuery,"TABLE_SCHEM,TABLE_NAME,KEY_SEQ"); // Difference than JDBC

    setCatalogQueryBuf(pStmt, szCatalogQuery);
    rc = RsExecute::RS_SQLExecDirect(phstmt, (SQLCHAR *)szCatalogQuery, SQL_NTS, TRUE, FALSE, FALSE, TRUE);
    resetCatalogQueryFlag(pStmt);

error:

    return rc;
}


/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Unicode version of SQLPrimaryKeys.
//
SQLRETURN SQL_API SQLPrimaryKeysW(SQLHSTMT     phstmt,
                                    SQLWCHAR*    pwCatalogName,
                                    SQLSMALLINT  cchCatalogName,
                                    SQLWCHAR*    pwSchemaName,
                                    SQLSMALLINT  cchSchemaName,
                                    SQLWCHAR*    pwTableName,
                                    SQLSMALLINT  cchTableName)
{
    SQLRETURN rc;
    char szCatalogName[MAX_IDEN_LEN];
    char szSchemaName[MAX_IDEN_LEN];
    char szTableName[MAX_IDEN_LEN]; 

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLPrimaryKeysW(FUNC_CALL, 0, phstmt, pwCatalogName, cchCatalogName, pwSchemaName, cchSchemaName, pwTableName, cchTableName);

    wchar_to_utf8(pwCatalogName, cchCatalogName, szCatalogName, MAX_IDEN_LEN);
    wchar_to_utf8(pwSchemaName, cchSchemaName, szSchemaName, MAX_IDEN_LEN);
    wchar_to_utf8(pwTableName, cchTableName, szTableName, MAX_IDEN_LEN);

    rc = RsCatalog::RS_SQLPrimaryKeys(phstmt, (SQLCHAR *)((pwCatalogName) ? szCatalogName : NULL), SQL_NTS,
                                    (SQLCHAR *)((pwSchemaName) ? szSchemaName : NULL), SQL_NTS,
                                    (SQLCHAR *)((pwTableName) ? szTableName : NULL), SQL_NTS);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLPrimaryKeysW(FUNC_RETURN, rc, phstmt, pwCatalogName, cchCatalogName, pwSchemaName, cchSchemaName, pwTableName, cchTableName);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLGetTypeInfo returns information about data types supported by the data source. 
// The driver returns the information in the form of an SQL result set. 
// The data types are intended for use in Data Definition Language (DDL) statements.
//
SQLRETURN  SQL_API SQLGetTypeInfo(SQLHSTMT phstmt,
                                   SQLSMALLINT hType)
{
    SQLRETURN rc;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLGetTypeInfo(FUNC_CALL, 0, phstmt, hType);

    rc = RsCatalog::RS_SQLGetTypeInfo(phstmt, hType);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLGetTypeInfo(FUNC_RETURN, rc, phstmt, hType);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLGetTypeInfo and SQLGetTypeInfoW.
//
SQLRETURN  SQL_API RsCatalog::RS_SQLGetTypeInfo(SQLHSTMT phstmt,
                                        SQLSMALLINT hType)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    char szCatalogQuery[MAX_CATALOG_QUERY_LEN];
    int i; 
    RS_TYPE_INFO typesInfo[MAX_TYPES] = 
    {
        { "boolean", SQL_BIT, 1, "", "", "", SQL_NULLABLE , SQL_FALSE,  SQL_PRED_BASIC , SQL_NULL_DATA , SQL_FALSE, SQL_NULL_DATA , "boolean", 
           0, 0, SQL_BIT, SQL_NULL_DATA, 10, SQL_NULL_DATA
        },
        { "bigint", SQL_BIGINT, 19, "", "", "", SQL_NULLABLE, SQL_FALSE, SQL_SEARCHABLE , SQL_FALSE, SQL_FALSE, SQL_FALSE, "bigint", 
           0, 0, SQL_BIGINT, SQL_NULL_DATA, 10, SQL_NULL_DATA
        },
        {
          "character", SQL_CHAR, 65535, "\\'", "\\'", "length", SQL_NULLABLE, SQL_TRUE, SQL_SEARCHABLE, SQL_NULL_DATA, SQL_FALSE, SQL_NULL_DATA, "char", 
          SQL_NULL_DATA, SQL_NULL_DATA, SQL_CHAR, SQL_NULL_DATA, SQL_NULL_DATA, SQL_NULL_DATA
        },
        {
          "numeric", SQL_NUMERIC, 19, "", "", "precision,scale", SQL_NULLABLE, SQL_FALSE, SQL_SEARCHABLE, SQL_FALSE, SQL_FALSE, SQL_FALSE, "numeric", 
          0, 18, SQL_NUMERIC, SQL_NULL_DATA, 10, SQL_NULL_DATA
        },
        {
          "integer", SQL_INTEGER, 10, "", "", "", SQL_NULLABLE, SQL_FALSE, SQL_SEARCHABLE, SQL_FALSE, SQL_FALSE, SQL_FALSE, "integer", 
          0, 0, SQL_INTEGER, SQL_NULL_DATA, 10, SQL_NULL_DATA
        },
        {
          "smallint", SQL_SMALLINT, 5, "", "", "", SQL_NULLABLE, SQL_FALSE, SQL_SEARCHABLE, SQL_FALSE, SQL_FALSE, SQL_FALSE, "smallint", 
          0, 0, SQL_SMALLINT, SQL_NULL_DATA, 10, SQL_NULL_DATA
        },
		{
          "real", SQL_REAL, 24, "", "", "", SQL_NULLABLE, SQL_FALSE, SQL_SEARCHABLE, SQL_FALSE, SQL_FALSE, SQL_FALSE, "real", 
          SQL_NULL_DATA, SQL_NULL_DATA, SQL_REAL, SQL_NULL_DATA, 2, SQL_NULL_DATA
        },
        {
          "double precision", SQL_DOUBLE, 53, "", "", "", SQL_NULLABLE, SQL_FALSE, SQL_SEARCHABLE, SQL_FALSE, SQL_FALSE, SQL_FALSE, "double precision", 
          SQL_NULL_DATA, SQL_NULL_DATA, SQL_DOUBLE, SQL_NULL_DATA, 2, SQL_NULL_DATA
        },
        {
          "character varying", SQL_VARCHAR , 65535, "\\'", "\\'", "max length", SQL_NULLABLE, SQL_TRUE, SQL_SEARCHABLE, SQL_NULL_DATA, SQL_FALSE, SQL_NULL_DATA, "varchar",
          SQL_NULL_DATA, SQL_NULL_DATA, SQL_VARCHAR, SQL_NULL_DATA, SQL_NULL_DATA, SQL_NULL_DATA
        },
        {
          "date", SQL_TYPE_DATE, 10, "{d \\'", "\\'}", "", SQL_NULLABLE, SQL_FALSE, SQL_SEARCHABLE, SQL_NULL_DATA, SQL_FALSE, SQL_NULL_DATA, "date", 
          SQL_NULL_DATA, SQL_NULL_DATA, SQL_DATE, SQL_CODE_DATE, SQL_NULL_DATA, SQL_NULL_DATA
        },
        {
          "timestamp", SQL_TYPE_TIMESTAMP, 26, "{ts \\'", "\\'}", "", SQL_NULLABLE, SQL_FALSE, SQL_SEARCHABLE, SQL_NULL_DATA, SQL_FALSE, SQL_NULL_DATA, "timestamp", 
          0, 6, SQL_DATE, SQL_CODE_TIMESTAMP, SQL_NULL_DATA, SQL_NULL_DATA
        },
        {
          "time", SQL_TYPE_TIME, 15, "{t \\'", "\\'}", "", SQL_NULLABLE, SQL_FALSE, SQL_SEARCHABLE, SQL_NULL_DATA, SQL_FALSE, SQL_NULL_DATA, "time", 
          0, 6, SQL_DATE, SQL_CODE_TIME, SQL_NULL_DATA, SQL_NULL_DATA
        },
		{
			"timetz", SQL_TYPE_TIME, 21,  "\\'", "\\'", "", SQL_NULLABLE, SQL_FALSE, SQL_SEARCHABLE, SQL_NULL_DATA, SQL_FALSE, SQL_NULL_DATA, "timetz",
			0, 6, SQL_DATE, SQL_CODE_TIME, SQL_NULL_DATA, SQL_NULL_DATA
		},
		{
			"timestamptz", SQL_TYPE_TIMESTAMP, 35,  "\\'", "\\'", "", SQL_NULLABLE, SQL_FALSE, SQL_SEARCHABLE, SQL_NULL_DATA, SQL_FALSE, SQL_NULL_DATA, "timestamptz",
			0, 6, SQL_DATE, SQL_CODE_TIMESTAMP, SQL_NULL_DATA, SQL_NULL_DATA
		},
		{
			"bpchar", SQL_CHAR, 256, "\\'", "\\'", "length", SQL_NULLABLE, SQL_TRUE, SQL_SEARCHABLE, SQL_NULL_DATA, SQL_FALSE, SQL_NULL_DATA, "bpchar",
			SQL_NULL_DATA, SQL_NULL_DATA, SQL_CHAR, SQL_NULL_DATA, SQL_NULL_DATA, SQL_NULL_DATA
		},
		{
			"geometry", SQL_LONGVARBINARY , 1000000, "\\'", "\\'", "max length", SQL_NULLABLE, SQL_TRUE, SQL_SEARCHABLE, SQL_NULL_DATA, SQL_FALSE, SQL_NULL_DATA, "geometry",
			SQL_NULL_DATA, SQL_NULL_DATA, SQL_LONGVARBINARY, SQL_NULL_DATA, SQL_NULL_DATA, SQL_NULL_DATA
		},
		{
			"super", SQL_LONGVARCHAR , 4194304, "\\'", "\\'", "max length", SQL_NULLABLE, SQL_TRUE, SQL_SEARCHABLE, SQL_NULL_DATA, SQL_FALSE, SQL_NULL_DATA, "super",
			SQL_NULL_DATA, SQL_NULL_DATA, SQL_LONGVARCHAR, SQL_NULL_DATA, SQL_NULL_DATA, SQL_NULL_DATA
		},
		{
			"varbyte", SQL_LONGVARBINARY , 1000000, "\\'", "\\'", "max length", SQL_NULLABLE, SQL_TRUE, SQL_SEARCHABLE, SQL_NULL_DATA, SQL_FALSE, SQL_NULL_DATA, "varbyte",
			SQL_NULL_DATA, SQL_NULL_DATA, SQL_LONGVARBINARY, SQL_NULL_DATA, SQL_NULL_DATA, SQL_NULL_DATA
		},
		{
			"geography", SQL_LONGVARBINARY , 1000000, "\\'", "\\'", "max length", SQL_NULLABLE, SQL_FALSE, SQL_SEARCHABLE, SQL_NULL_DATA, SQL_FALSE, SQL_NULL_DATA, "geography",
			SQL_NULL_DATA, SQL_NULL_DATA, SQL_LONGVARBINARY, SQL_NULL_DATA, SQL_NULL_DATA, SQL_NULL_DATA
		},
		{
			"intervaly2m", SQL_INTERVAL_YEAR_TO_MONTH, 32, "\\'", "\\'", "", SQL_NULLABLE, SQL_FALSE, SQL_SEARCHABLE, SQL_NULL_DATA, SQL_FALSE, SQL_NULL_DATA, "intervaly2m",
			0, 0, SQL_CODE_YEAR_TO_MONTH, SQL_NULL_DATA, SQL_NULL_DATA, SQL_NULL_DATA
		},
		{
			"intervald2s", SQL_INTERVAL_DAY_TO_SECOND, 64, "\\'", "\\'", "", SQL_NULLABLE, SQL_FALSE, SQL_SEARCHABLE, SQL_NULL_DATA, SQL_FALSE, SQL_NULL_DATA, "intervald2s",
			0, 0, SQL_CODE_DAY_TO_SECOND, SQL_NULL_DATA, SQL_NULL_DATA, SQL_NULL_DATA
		}
    };

    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pStmt->pErrorList = clearErrorList(pStmt->pErrorList);

    rs_strncpy(szCatalogQuery," SELECT ", sizeof(szCatalogQuery));
    if(hType == SQL_ALL_TYPES)
    {
        for(i = 0; i < MAX_TYPES;i++)
        {
            if(i != 0)
                rs_strncat(szCatalogQuery," UNION SELECT ", MAX_CATALOG_QUERY_LEN - strlen(szCatalogQuery));
            addTypeRow(szCatalogQuery, &(typesInfo[i]));
        }
    }
    else
    {
        int found = FALSE;

        // Convert 2.0 date/time types to 3.0 date/time types
        if(hType == SQL_DATE)
            hType = SQL_TYPE_DATE;
        else
        if(hType == SQL_TIMESTAMP)
            hType = SQL_TYPE_TIMESTAMP;
        else
        if(hType == SQL_TIME)
            hType = SQL_TYPE_TIME;
		else
		if (hType == SQL_TINYINT)
			hType = SQL_BIT;


        for(i = 0; i < MAX_TYPES;i++)
        {
            if(typesInfo[i].hType == hType)
            {
                addTypeRow(szCatalogQuery, &(typesInfo[i]));
                found = TRUE;
                break;
            }
        }

        if(!found)
        {
            rc = SQL_ERROR;
            addError(&pStmt->pErrorList,"HY004", "Invalid SQL data type", 0, NULL);
            goto error; 
        }
    }

    addOrderByClause(szCatalogQuery,"DATA_TYPE"); 

    setCatalogQueryBuf(pStmt, szCatalogQuery);
    rc = RsExecute::RS_SQLExecDirect(phstmt, (SQLCHAR *)szCatalogQuery, SQL_NTS, TRUE, FALSE, FALSE, TRUE);
    resetCatalogQueryFlag(pStmt);

error:

    return rc;
}


/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Unicode version of SQLGetTypeInfo.
//
SQLRETURN SQL_API   SQLGetTypeInfoW(SQLHSTMT     phstmt,
                                    SQLSMALLINT   hType)
{
    SQLRETURN rc;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLGetTypeInfoW(FUNC_CALL, 0, phstmt, hType);

    rc = RsCatalog::RS_SQLGetTypeInfo(phstmt, hType);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLGetTypeInfoW(FUNC_RETURN, rc, phstmt, hType);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLColumnPrivileges returns a list of columns and associated privileges for the specified table. 
// The driver returns the information as a result set on the specified StatementHandle.
//
SQLRETURN SQL_API SQLColumnPrivileges(SQLHSTMT           phstmt,
                                        SQLCHAR          *pCatalogName,
                                        SQLSMALLINT      cbCatalogName,
                                        SQLCHAR          *pSchemaName,
                                        SQLSMALLINT      cbSchemaName,
                                        SQLCHAR          *pTableName,
                                        SQLSMALLINT      cbTableName,
                                        SQLCHAR          *pColumnName,
                                        SQLSMALLINT      cbColumnName)
{
    SQLRETURN rc;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLColumnPrivileges(FUNC_CALL, 0, phstmt, pCatalogName, cbCatalogName, pSchemaName, cbSchemaName, pTableName, cbTableName, pColumnName, cbColumnName);

    rc = RsCatalog::RS_SQLColumnPrivileges(phstmt, pCatalogName, cbCatalogName, pSchemaName, cbSchemaName, pTableName, cbTableName, pColumnName, cbColumnName);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLColumnPrivileges(FUNC_RETURN, rc, phstmt, pCatalogName, cbCatalogName, pSchemaName, cbSchemaName, pTableName, cbTableName, pColumnName, cbColumnName);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLColumnPrivileges and SQLColumnPrivilegesW.
//
SQLRETURN SQL_API RsCatalog::RS_SQLColumnPrivileges(SQLHSTMT           phstmt,
                                        SQLCHAR          *pCatalogName,
                                        SQLSMALLINT      cbCatalogName,
                                        SQLCHAR          *pSchemaName,
                                        SQLSMALLINT      cbSchemaName,
                                        SQLCHAR          *pTableName,
                                        SQLSMALLINT      cbTableName,
                                        SQLCHAR          *pColumnName,
                                        SQLSMALLINT      cbColumnName)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    char szCatalogQuery[MAX_CATALOG_QUERY_LEN];
	char catalogFilter[MAX_LARGE_TEMP_BUF_LEN];
	char filterClause[MAX_CATALOG_QUERY_FILTER_LEN];
	int len;

    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pStmt->pErrorList = clearErrorList(pStmt->pErrorList);

    if(!checkForValidCatalogName(pStmt, pCatalogName))
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HYC00", "Optional feature not implemented::RS_SQLColumnPrivileges", 0, NULL);
        goto error; 
    }

    if(pTableName == NULL || cbTableName == SQL_NULL_DATA)
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HY009", "Invalid use of null pointer", 0, NULL);
        goto error; 
    }

	rs_strncpy(szCatalogQuery, SQLCOLUMN_PRIVILEGES_BASE_QUERY, sizeof(szCatalogQuery));

	getCatalogFilterCondition(catalogFilter, MAX_LARGE_TEMP_BUF_LEN, pStmt, (char *)pCatalogName, cbCatalogName,
								TRUE, NULL);

	filterClause[0] = '\0';
    addEqualFilterCondition(pStmt, filterClause, MAX_CATALOG_QUERY_FILTER_LEN, "TABLE_SCHEM" , (char *)pSchemaName, cbSchemaName);
    addEqualFilterCondition(pStmt, filterClause, MAX_CATALOG_QUERY_FILTER_LEN, "TABLE_NAME" , (char *)pTableName, cbTableName);
    addLikeOrEqualFilterCondition(pStmt, filterClause, "COLUMN_NAME" , (char *)pColumnName, cbColumnName);

	len = strlen(szCatalogQuery);
	snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, "%s", catalogFilter);
	len = strlen(szCatalogQuery);
	snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, "%s", filterClause);

    addOrderByClause(szCatalogQuery,"TABLE_SCHEM,TABLE_NAME,COLUMN_NAME,PRIVILEGE"); 

    setCatalogQueryBuf(pStmt, szCatalogQuery);
    rc = RsExecute::RS_SQLExecDirect(phstmt, (SQLCHAR *)szCatalogQuery, SQL_NTS, TRUE, FALSE, FALSE, TRUE);
    resetCatalogQueryFlag(pStmt);

error:

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Unicode version of SQLColumnPrivileges.
//
SQLRETURN SQL_API SQLColumnPrivilegesW(SQLHSTMT      phstmt,
                                       SQLWCHAR*     pwCatalogName,
                                        SQLSMALLINT  cchCatalogName,
                                        SQLWCHAR*    pwSchemaName,
                                        SQLSMALLINT  cchSchemaName,
                                        SQLWCHAR*    pwTableName,
                                        SQLSMALLINT  cchTableName,
                                        SQLWCHAR*    pwColumnName,
                                        SQLSMALLINT  cchColumnName)
{
    SQLRETURN rc;
    char szCatalogName[MAX_IDEN_LEN];
    char szSchemaName[MAX_IDEN_LEN];
    char szTableName[MAX_IDEN_LEN]; 
    char szColumnName[MAX_IDEN_LEN]; 
    
    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLColumnPrivilegesW(FUNC_CALL, 0, phstmt, pwCatalogName, cchCatalogName, pwSchemaName, cchSchemaName, pwTableName, cchTableName, pwColumnName, cchColumnName);

    wchar_to_utf8(pwCatalogName, cchCatalogName, szCatalogName, MAX_IDEN_LEN);
    wchar_to_utf8(pwSchemaName, cchSchemaName, szSchemaName, MAX_IDEN_LEN);
    wchar_to_utf8(pwTableName, cchTableName, szTableName, MAX_IDEN_LEN);
    wchar_to_utf8(pwColumnName, cchColumnName, szColumnName, MAX_IDEN_LEN);


    rc = RsCatalog::RS_SQLColumnPrivileges(phstmt, (SQLCHAR *)((pwCatalogName) ? szCatalogName : NULL), SQL_NTS,
                                        (SQLCHAR *)((pwSchemaName) ? szSchemaName : NULL), SQL_NTS,
                                        (SQLCHAR *)((pwTableName) ? szTableName : NULL), SQL_NTS,
                                        (SQLCHAR *)((pwColumnName) ? szColumnName : NULL), SQL_NTS);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLColumnPrivilegesW(FUNC_RETURN, rc, phstmt, pwCatalogName, cchCatalogName, pwSchemaName, cchSchemaName, pwTableName, cchTableName, pwColumnName, cchColumnName);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLTablePrivileges returns a list of tables and the privileges associated with each table. 
// The driver returns the information as a result set on the specified statement.
//
SQLRETURN SQL_API SQLTablePrivileges(SQLHSTMT           phstmt,
                                        SQLCHAR         *pCatalogName,
                                        SQLSMALLINT     cbCatalogName,
                                        SQLCHAR         *pSchemaName,
                                        SQLSMALLINT     cbSchemaName,
                                        SQLCHAR         *pTableName,
                                        SQLSMALLINT      cbTableName)
{
    SQLRETURN rc;

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLTablePrivileges(FUNC_CALL, 0, phstmt, pCatalogName, cbCatalogName, pSchemaName, cbSchemaName, pTableName, cbTableName);

    rc = RsCatalog::RS_SQLTablePrivileges(phstmt, pCatalogName, cbCatalogName, pSchemaName, cbSchemaName, pTableName, cbTableName);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLTablePrivileges(FUNC_RETURN, rc, phstmt, pCatalogName, cbCatalogName, pSchemaName, cbSchemaName, pTableName, cbTableName);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Common function for SQLTablePrivileges and SQLTablePrivilegesW.
//
SQLRETURN SQL_API RsCatalog::RS_SQLTablePrivileges(SQLHSTMT           phstmt,
                                        SQLCHAR         *pCatalogName,
                                        SQLSMALLINT     cbCatalogName,
                                        SQLCHAR         *pSchemaName,
                                        SQLSMALLINT     cbSchemaName,
                                        SQLCHAR         *pTableName,
                                        SQLSMALLINT      cbTableName)
{
    SQLRETURN rc = SQL_SUCCESS;
    RS_STMT_INFO *pStmt = (RS_STMT_INFO *)phstmt;
    char szCatalogQuery[MAX_CATALOG_QUERY_LEN];
	char catalogFilter[MAX_LARGE_TEMP_BUF_LEN];
	char filterClause[MAX_CATALOG_QUERY_FILTER_LEN];
	int len;


    if(!VALID_HSTMT(phstmt))
    {
        rc = SQL_INVALID_HANDLE;
        goto error;
    }

    // Clear error list
    pStmt->pErrorList = clearErrorList(pStmt->pErrorList);

    if(!checkForValidCatalogName(pStmt, pCatalogName))
    {
        rc = SQL_ERROR;
        addError(&pStmt->pErrorList,"HYC00", "Optional feature not implemented::RS_SQLTablePrivileges", 0, NULL);
        goto error; 
    }

	rs_strncpy(szCatalogQuery, SQLTABLE_PRIVILEGES_BASE_QUERY, sizeof(szCatalogQuery));

	getCatalogFilterCondition(catalogFilter, MAX_LARGE_TEMP_BUF_LEN, pStmt, (char *)pCatalogName, cbCatalogName,
								TRUE, NULL);

	filterClause[0] = '\0';
	addLikeOrEqualFilterCondition(pStmt, filterClause, "TABLE_SCHEM" , (char *)pSchemaName, cbSchemaName);
    addLikeOrEqualFilterCondition(pStmt, filterClause, "TABLE_NAME" , (char *)pTableName, cbTableName);

	len = strlen(szCatalogQuery);
	snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, "%s", catalogFilter);
	len = strlen(szCatalogQuery);
	snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, "%s", filterClause);

    addOrderByClause(szCatalogQuery,"TABLE_SCHEM,TABLE_NAME,PRIVILEGE"); 

    setCatalogQueryBuf(pStmt, szCatalogQuery);
    rc = RsExecute::RS_SQLExecDirect(phstmt, (SQLCHAR *)szCatalogQuery, SQL_NTS, TRUE, FALSE, FALSE, TRUE);
    resetCatalogQueryFlag(pStmt);

error:

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Unicode version of SQLTablePrivileges.
//
SQLRETURN SQL_API SQLTablePrivilegesW(SQLHSTMT       phstmt,
                                        SQLWCHAR*    pwCatalogName,
                                        SQLSMALLINT  cchCatalogName,
                                        SQLWCHAR*    pwSchemaName,
                                        SQLSMALLINT  cchSchemaName,
                                        SQLWCHAR*    pwTableName,
                                        SQLSMALLINT  cchTableName)
{
    SQLRETURN rc;
    char szCatalogName[MAX_IDEN_LEN];
    char szSchemaName[MAX_IDEN_LEN];
    char szTableName[MAX_IDEN_LEN]; 

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLTablePrivilegesW(FUNC_CALL, 0, phstmt, pwCatalogName, cchCatalogName, pwSchemaName, cchSchemaName, pwTableName, cchTableName);

    wchar_to_utf8(pwCatalogName, cchCatalogName, szCatalogName, MAX_IDEN_LEN);
    wchar_to_utf8(pwSchemaName, cchSchemaName, szSchemaName, MAX_IDEN_LEN);
    wchar_to_utf8(pwTableName, cchTableName, szTableName, MAX_IDEN_LEN);


    rc = RsCatalog::RS_SQLTablePrivileges(phstmt, (SQLCHAR *)((pwCatalogName) ? szCatalogName : NULL), SQL_NTS,
                                       (SQLCHAR *)((pwSchemaName) ? szSchemaName : NULL), SQL_NTS,
                                       (SQLCHAR *)((pwTableName) ? szTableName : NULL), SQL_NTS);

    if(IS_TRACE_LEVEL_API_CALL())
        TraceSQLTablePrivilegesW(FUNC_RETURN, rc, phstmt, pwCatalogName, cchCatalogName, pwSchemaName, cchSchemaName, pwTableName, cchTableName);

    return rc;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Add = or LIKE WHERE clause for catalog query.
//
static void addLikeOrEqualFilterCondition(RS_STMT_INFO *pStmt, char *filterClause, const char *pFilterColumn, const char *pVal, SQLSMALLINT cbLen)
{
    if(pVal && *pVal != '\0')
    {
		char *pEscapedName = NULL;
		short cbEscapedName = 0;
		int len;

        if(cbLen == SQL_NTS)
            cbLen = (SQLSMALLINT) strlen(pVal);

		pEscapedName = escapedFilterCondition(pVal, cbLen);
		cbEscapedName = (short)strlen(pEscapedName);

		len = strlen(filterClause);
        snprintf(filterClause + len, MAX_CATALOG_QUERY_FILTER_LEN - len, " AND %s %s '%.*s' ",
					pFilterColumn, (pStmt->pStmtAttr->iMetaDataId ? EQUAL_OP : LIKE_OP),
					cbEscapedName, pEscapedName);
		pEscapedName = (char *)rs_free(pEscapedName);
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Add = clause for catalog query.
//
static void addEqualFilterCondition(RS_STMT_INFO *pStmt, char *szCatalogQuery, int bufLen, char *pFilterColumn, char *pVal, SQLSMALLINT cbLen)
{
    if(pVal && *pVal != '\0')
    {
		char *pEscapedName = NULL;
		short cbEscapedName = 0;
		int len;

        if(cbLen == SQL_NTS)
            cbLen = (SQLSMALLINT) strlen(pVal);

		pEscapedName = escapedFilterCondition(pVal, cbLen);
		cbEscapedName = (short)strlen(pEscapedName);

		len = strlen(szCatalogQuery);
        snprintf(szCatalogQuery + len, bufLen - len, " AND %s %s '%.*s' ", pFilterColumn, EQUAL_OP, cbEscapedName, pEscapedName);

		pEscapedName = (char *)rs_free(pEscapedName);
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Add table type WHERE clause for catalog query.
//
static void addTableTypeFilterCondition(char *filterClause, char *pVal, SQLSMALLINT cbLen, int schemaPatternType)
{
    if(pVal && *pVal != '\0')
    {
        char tempBuf[MAX_TEMP_BUF_LEN];
        int len = (int)((cbLen == SQL_NTS) ? strlen(pVal) : cbLen);
        char *pToken;

        strncpy(tempBuf, pVal, len);
        tempBuf[len] = '\0';

        pToken = strtok(tempBuf, ",");

		if (schemaPatternType == LOCAL_SCHEMA_QUERY)
		{
			rs_strncat(filterClause, " AND (false ", MAX_CATALOG_QUERY_FILTER_LEN - strlen(filterClause));

			while (pToken)
			{
				pToken = trim_whitespaces(pToken);

				if (_stricmp(pToken, "TABLE") == 0
					|| _stricmp(pToken, "'TABLE'") == 0)
				{
					rs_strncat(filterClause, " OR ( c.relkind = 'r' AND n.nspname !~ '^pg_' AND n.nspname <> 'information_schema' ) ", MAX_CATALOG_QUERY_FILTER_LEN - strlen(filterClause));
				}
				else
				if (_stricmp(pToken, "VIEW") == 0
					|| _stricmp(pToken, "'VIEW'") == 0)
				{
					rs_strncat(filterClause, " OR ( c.relkind = 'v' AND n.nspname <> 'pg_catalog' AND n.nspname <> 'information_schema' ) ", MAX_CATALOG_QUERY_FILTER_LEN - strlen(filterClause));
				}
				else
				if (_stricmp(pToken, "SYSTEM TABLE") == 0
					|| _stricmp(pToken, "'SYSTEM TABLE'") == 0)
				{
					rs_strncat(filterClause, " OR ( c.relkind = 'r' AND (n.nspname = 'pg_catalog' OR n.nspname = 'information_schema') ) ", MAX_CATALOG_QUERY_FILTER_LEN - strlen(filterClause));
				}
				else
				if (_stricmp(pToken, "SYSTEM VIEW") == 0
					|| _stricmp(pToken, "'SYSTEM VIEW'") == 0)
				{
					rs_strncat(filterClause, " OR ( c.relkind = 'v' AND (n.nspname = 'pg_catalog' OR n.nspname = 'information_schema') ) ", MAX_CATALOG_QUERY_FILTER_LEN - strlen(filterClause));
				}
				else
				if (_stricmp(pToken, "SYSTEM TOAST TABLE") == 0
					|| _stricmp(pToken, "'SYSTEM TOAST TABLE'") == 0)
				{
					rs_strncat(filterClause, " OR ( c.relkind = 'r' AND n.nspname = 'pg_toast' ) ", MAX_CATALOG_QUERY_FILTER_LEN - strlen(filterClause));
				}
				else
				if (_stricmp(pToken, "TEMPORARY TABLE") == 0
					|| _stricmp(pToken, "'TEMPORARY TABLE'") == 0)
				{
					rs_strncat(filterClause, " OR ( c.relkind = 'r' AND n.nspname ~ '^pg_temp_' ) ", MAX_CATALOG_QUERY_FILTER_LEN - strlen(filterClause));
				}
				else
				if (_stricmp(pToken, "TEMPORARY VIEW") == 0
					|| _stricmp(pToken, "'TEMPORARY VIEW'") == 0)
				{
					rs_strncat(filterClause, " OR ( c.relkind = 'v' AND n.nspname ~ '^pg_temp_' ) ", MAX_CATALOG_QUERY_FILTER_LEN - strlen(filterClause));
				}

				pToken = strtok(NULL, ",");
			} // Loop

			rs_strncat(filterClause, " ) ", MAX_CATALOG_QUERY_FILTER_LEN - strlen(filterClause));
		} // LOCAL_SCHEMA_QUERY
		else
		if (schemaPatternType == NO_SCHEMA_UNIVERSAL_QUERY
			|| schemaPatternType == EXTERNAL_SCHEMA_QUERY) 
		{
			bool first = TRUE;
			rs_strncat(filterClause, " AND TABLE_TYPE IN ( ", MAX_CATALOG_QUERY_FILTER_LEN - strlen(filterClause));

			while (pToken)
			{
				char *pEscapedName = NULL;
				short cbEscapedName = 0;
				int len1;

				if (!first)
				{
					rs_strncat(filterClause, ", ", MAX_CATALOG_QUERY_FILTER_LEN - strlen(filterClause));
				}
				pToken = trim_whitespaces(pToken);
				if (*pToken == '\'')
				{
					pToken++;
					pToken[strlen(pToken) - 1] = '\0'; // overwrite last '\'' with terminator
				}

				pEscapedName = escapedFilterCondition(pToken, strlen(pToken));

				len1 = strlen(filterClause);
				snprintf(filterClause + len1, MAX_CATALOG_QUERY_FILTER_LEN - len1, "'%s'", pEscapedName);
				pEscapedName = (char *)rs_free(pEscapedName);
				first = FALSE;
				pToken = strtok(NULL, ",");
			} // Loop

			rs_strncat(filterClause, " ) ", MAX_CATALOG_QUERY_FILTER_LEN - strlen(filterClause));
		}
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Add ORDER BY clause for catalog query.
//
static void addOrderByClause(char *szCatalogQuery, char *pOrderByCols)
{
	int len = strlen(szCatalogQuery);
    snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, " ORDER BY %s ",pOrderByCols);
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Add type info for catalog query.
//
static void addTypeRow(char *szCatalogQuery, RS_TYPE_INFO *pTypeInfo)
{
	int len;

	len = strlen(szCatalogQuery);
    snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, " '%s' AS TYPE_NAME, ", pTypeInfo->szTypeName);
	len = strlen(szCatalogQuery);
    snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, " %hd AS DATA_TYPE, ", pTypeInfo->hType);
	len = strlen(szCatalogQuery);
    snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, " %d AS COLUMN_SIZE, ", pTypeInfo->iColumnSize);

	len = strlen(szCatalogQuery);
    if(pTypeInfo->szLiteralPrefix[0] != '\0') {
        snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, " '%s' AS LITERAL_PREFIX, ", pTypeInfo->szLiteralPrefix);
	}
    else {
        snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, " NULL AS LITERAL_PREFIX, ");
	}

	len = strlen(szCatalogQuery);
    if(pTypeInfo->szLiteralSuffix[0] != '\0') {
        snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, " '%s' AS LITERAL_SUFFIX, ", pTypeInfo->szLiteralSuffix);
	}
    else {
        snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, " NULL AS LITERAL_SUFFIX, ");
	}

	len = strlen(szCatalogQuery);
    if(pTypeInfo->szCreateParams[0] != '\0') {
        snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, " '%s' AS CREATE_PARAMS, ", pTypeInfo->szCreateParams);
	}
    else {
        snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, " NULL AS CREATE_PARAMS, ");
	}

	len = strlen(szCatalogQuery);
    if(pTypeInfo->hNullable != SQL_NULL_DATA) {
        snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, " %hd AS NULLABLE, ", pTypeInfo->hNullable);
	}
    else {
        snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, " NULL AS NULLABLE, ");
	}

	len = strlen(szCatalogQuery);
    if(pTypeInfo->iCaseSensitive != SQL_NULL_DATA) {
        snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, " %d AS CASE_SENSITIVE, ", pTypeInfo->iCaseSensitive);
	}
    else {
        snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, " NULL AS CASE_SENSITIVE, ");
	}

	len = strlen(szCatalogQuery);
    if(pTypeInfo->iSearchable != SQL_NULL_DATA) {
        snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, " %d AS SEARCHABLE, ", pTypeInfo->iSearchable);
	}
    else {
        snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, " NULL AS SEARCHABLE, ");
	}

	len = strlen(szCatalogQuery);
    if(pTypeInfo->iUnsigned != SQL_NULL_DATA) {
        snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, " %d AS UNSIGNED_ATTRIBUTE, ", pTypeInfo->iUnsigned);
	}
    else {
        snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, " NULL AS UNSIGNED_ATTRIBUTE, ");
	}

	len = strlen(szCatalogQuery);
    if(pTypeInfo->iFixedPrecScale != SQL_NULL_DATA) {
        snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, " %d AS FIXED_PREC_SCALE, ", pTypeInfo->iFixedPrecScale);
	}
    else {
        snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, " NULL AS FIXED_PREC_SCALE, ");
	}

	len = strlen(szCatalogQuery);
    if(pTypeInfo->iAutoInc != SQL_NULL_DATA) {
        snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, " %d AS AUTO_UNIQUE, ", pTypeInfo->iAutoInc);
	}
    else {
        snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, " NULL AS AUTO_UNIQUE, ");
	}

	len = strlen(szCatalogQuery);
    snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, " '%s' AS LOCAL_TYPE_NAME, ", pTypeInfo->szLocalTypeName);

	len = strlen(szCatalogQuery);
    if(pTypeInfo->hMinScale != SQL_NULL_DATA) {
        snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, " %hd AS MINIMUM_SCALE, ", pTypeInfo->hMinScale);
	}
    else {
        snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, " NULL AS MINIMUM_SCALE, ");
	}

	len = strlen(szCatalogQuery);
    if(pTypeInfo->hMaxScale != SQL_NULL_DATA) {
        snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, " %hd AS MAXIMUM_SCALE, ", pTypeInfo->hMaxScale);
	}
    else {
        snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, " NULL AS MAXIMUM_SCALE, ");
	}

	len = strlen(szCatalogQuery);
    snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, " %hd AS SQL_DATA_TYPE, ", pTypeInfo->hSqlDataType);

	len = strlen(szCatalogQuery);
    if(pTypeInfo->hSqlDateTimeSub != SQL_NULL_DATA) {
        snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, " %hd AS SQL_DATETIME_SUB, ", pTypeInfo->hSqlDateTimeSub);
	}
    else {
        snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, " NULL AS SQL_DATETIME_SUB, ");
	}

	len = strlen(szCatalogQuery);
    if(pTypeInfo->iNumPrexRadix != SQL_NULL_DATA) {
        snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, " %d AS NUM_PREC_RADIX, ", pTypeInfo->iNumPrexRadix);
	}
    else {
        snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, " NULL AS NUM_PREC_RADIX, ");
	}

	len = strlen(szCatalogQuery);
    if(pTypeInfo->iIntervalPrecision != SQL_NULL_DATA) {
        snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, " %d AS INTERVAL_PRECISION ", pTypeInfo->iIntervalPrecision);
	}
    else {
        snprintf(szCatalogQuery + len, MAX_CATALOG_QUERY_LEN - len, " NULL AS INTERVAL_PRECISION ");
	}
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Check whether catalog name is valid or not for catalog functions.
// Return TRUE when it's valid otherwise FALSE.
// Deprecated: Returns TRUE all the time and will be removed in future releases
static int checkForValidCatalogName(RS_STMT_INFO *pStmt, SQLCHAR *pCatalogName)
{
    RS_LOG_DEBUG("RSCAT", "Discarding deprecated catalog name validity check.");
    return TRUE;
    int iValid;

    if(pCatalogName && *pCatalogName != '\0')
    {
        if(pStmt)
        {
            RS_CONN_INFO *pConn = pStmt->phdbc;

            if(pConn->pConnectProps && _stricmp(pConn->pConnectProps->szDatabase, (char *)pCatalogName) == 0)
                iValid = TRUE;
            else
                iValid = FALSE;
        }
        else
            iValid = FALSE;
    }
    else
        iValid = TRUE;

    return iValid;
}

/*====================================================================================================================================================*/

static bool isSingleDatabaseMetaData(RS_STMT_INFO *pStmt) {
    bool isDatabaseMetadaCurrentOnly_ = isDatabaseMetadaCurrentOnly(pStmt);
    bool isDsEnabled = getLibpqParameterStatus(pStmt, "datashare_enabled");
    bool isExtDbEnabled = getLibpqParameterStatus(pStmt, "external_database");
    
    if (isExtDbEnabled)
      return false;
    else
      return (isDatabaseMetadaCurrentOnly_ || !isDsEnabled);
}

/*====================================================================================================================================================*/

/**
* Helper method to determine if there is a possible external schema pattern match.
*
* @throws SQLException   If an error occurs.
*/
static int getExtSchemaPatternMatch(RS_STMT_INFO *pStmt, SQLCHAR *pSchemaName, SQLSMALLINT cbSchemaName)
{
	if (NULL != pSchemaName && *pSchemaName != '\0')
	{
		if (isSingleDatabaseMetaData(pStmt)) 
		{
			SQLRETURN rc;
			char szSql[MAX_LARGE_TEMP_BUF_LEN];
			RS_CONN_INFO *pConn = pStmt->phdbc;
			char pVal[MAX_NUMBER_BUF_LEN];

			pVal[0] = '\0';

			if (cbSchemaName == SQL_NTS)
				cbSchemaName = (short)strlen((char *)pSchemaName);
			
			snprintf(szSql, sizeof(szSql), "select 1 from svv_external_schemas where schemaname like '%.*s'", cbSchemaName, pSchemaName);

			rc = getOneQueryVal(pConn, szSql, pVal, MAX_NUMBER_BUF_LEN);
			if(pVal[0] != '\0')
				return EXTERNAL_SCHEMA_QUERY; // Optimized query
			else
				return LOCAL_SCHEMA_QUERY; // Only local schema
		}
		else 
		{
			// Datashare or cross-db support always go through
			// svv_all* view.
			return NO_SCHEMA_UNIVERSAL_QUERY; // Query both external and local schema
		}
	}
	else
	{
		// If the schema filter is null or empty, treat it as if there was a
		// matching schema found.
		return NO_SCHEMA_UNIVERSAL_QUERY; // Query both external and local schema
	}
}

/*====================================================================================================================================================*/

void buildLocalSchemaTablesQuery(char *pszCatalogQuery,
								RS_STMT_INFO *pStmt,
								SQLCHAR *pCatalogName,
								SQLSMALLINT cbCatalogName,
								SQLCHAR *pSchemaName,
								SQLSMALLINT cbSchemaName,
								SQLCHAR *pTableName,
								SQLSMALLINT cbTableName,
								SQLCHAR *pTableType,
								SQLSMALLINT cbTableType)
{
	char filterClause[MAX_CATALOG_QUERY_FILTER_LEN];

	getTableFilterClause(filterClause, pStmt, pCatalogName, cbCatalogName, pSchemaName, cbSchemaName,
						pTableName, cbTableName, pTableType, cbTableType, LOCAL_SCHEMA_QUERY, TRUE, NULL);

	rs_strncpy(pszCatalogQuery, SQLTABLES_BASE_QUERY, MAX_CATALOG_QUERY_LEN);
	rs_strncat(pszCatalogQuery, filterClause, MAX_CATALOG_QUERY_LEN - strlen(pszCatalogQuery));
	addOrderByClause(pszCatalogQuery, "TABLE_TYPE,TABLE_SCHEM,TABLE_NAME");
}

/*====================================================================================================================================================*/

static void getTableFilterClause(
	char *filterClause,
	RS_STMT_INFO *pStmt,
	SQLCHAR *pCatalogName,
	SQLSMALLINT cbCatalogName,
	SQLCHAR *pSchemaName,
	SQLSMALLINT cbSchemaName,
	SQLCHAR *pTableName,
	SQLSMALLINT cbTableName,
	SQLCHAR *pTableType,
	SQLSMALLINT cbTableType,
	int schemaPatternType,
	bool apiSupportedOnlyForConnectedDatabase,
	char * databaseColName) 
{
//	char *useSchemas = "SCHEMAS";
	char catalogFilter[MAX_LARGE_TEMP_BUF_LEN];
	int len;

	filterClause[0] = '\0';

	getCatalogFilterCondition(catalogFilter, MAX_LARGE_TEMP_BUF_LEN, pStmt, (char *)pCatalogName, cbCatalogName,
												apiSupportedOnlyForConnectedDatabase, databaseColName);

	len = strlen(filterClause);
	snprintf(filterClause + len, MAX_CATALOG_QUERY_FILTER_LEN - len, "%s", catalogFilter);

	addLikeOrEqualFilterCondition(pStmt, filterClause, "TABLE_SCHEM", (char *)pSchemaName, cbSchemaName);
	addLikeOrEqualFilterCondition(pStmt, filterClause, "TABLE_NAME", (char *)pTableName, cbTableName);
	addTableTypeFilterCondition(filterClause, (char *)pTableType, cbTableType, schemaPatternType);


//	if (schemaPatternType == LOCAL_SCHEMA_QUERY) {
//		if (connection.getHideUnprivilegedObjects()) {
//		snprintf(filterClause + strlen(filterClause), "%s", " AND has_table_privilege(c.oid, " \
//											" 'SELECT, INSERT, UPDATE, DELETE')");
//		}
//	}
}

/*====================================================================================================================================================*/

static void getCatalogFilterCondition(char *catalogFilter, 
									  int bufLen,
									 RS_STMT_INFO *pStmt,
									 char *pCatalogName,
									 short cbCatalogName,
									 bool apiSupportedOnlyForConnectedDatabase, 
									 char * databaseColName) 
{
	catalogFilter[0] = '\0';

	if (pCatalogName != NULL && *pCatalogName != '\0') 
	{
		char *pEscapedName = NULL;
		short cbEscapedName = 0;

		if (cbCatalogName == SQL_NTS)
			cbCatalogName = (short)strlen(pCatalogName);

		pEscapedName = escapedFilterCondition(pCatalogName, cbCatalogName);
		cbEscapedName = (short)strlen(pEscapedName);

		if (isSingleDatabaseMetaData(pStmt)
			|| apiSupportedOnlyForConnectedDatabase) 
		{
			snprintf(catalogFilter, bufLen, " AND current_database() LIKE  '%.*s' ", cbEscapedName, pEscapedName);
		}
		else {
			if (databaseColName == NULL)
				databaseColName = "database_name";

			snprintf(catalogFilter, bufLen, " AND %s = '%.*s' ", databaseColName, cbEscapedName, pEscapedName);
		}

		pEscapedName = (char *)rs_free(pEscapedName);
	}
}

/*====================================================================================================================================================*/

static void buildUniversalSchemaTablesQuery(char *pszCatalogQuery,
											RS_STMT_INFO *pStmt,
											SQLCHAR *pCatalogName,
											SQLSMALLINT cbCatalogName,
											SQLCHAR *pSchemaName,
											SQLSMALLINT cbSchemaName,
											SQLCHAR *pTableName,
											SQLSMALLINT cbTableName,
											SQLCHAR *pTableType,
											SQLSMALLINT cbTableType) 
{
	// Basic query, without the join operation and subquery name appended to the end
	char filterClause[MAX_CATALOG_QUERY_FILTER_LEN];

	rs_strncpy(pszCatalogQuery,"SELECT * FROM (SELECT CAST(current_database() AS VARCHAR(124)) AS TABLE_CAT," \
		 " table_schema AS TABLE_SCHEM," \
		 " table_name AS TABLE_NAME," \
		 " CAST(" \
		 " CASE table_type" \
		 " WHEN 'BASE TABLE' THEN CASE" \
		 " WHEN table_schema = 'pg_catalog' OR table_schema = 'information_schema' THEN 'SYSTEM TABLE'" \
		 " WHEN table_schema = 'pg_toast' THEN 'SYSTEM TOAST TABLE'" \
		 " WHEN table_schema ~ '^pg_' AND table_schema != 'pg_toast' THEN 'TEMPORARY TABLE'" \
		 " ELSE 'TABLE'" \
		 " END" \
		 " WHEN 'VIEW' THEN CASE" \
		 " WHEN table_schema = 'pg_catalog' OR table_schema = 'information_schema' THEN 'SYSTEM VIEW'" \
		 " WHEN table_schema = 'pg_toast' THEN NULL" \
		 " WHEN table_schema ~ '^pg_' AND table_schema != 'pg_toast' THEN 'TEMPORARY VIEW'" \
		 " ELSE 'VIEW'" \
		 " END" \
		 " WHEN 'EXTERNAL TABLE' THEN 'TABLE'" \
		 " END" \
		 " AS VARCHAR(124)) AS TABLE_TYPE," \
		 " REMARKS" \
		 " FROM svv_tables)",
		MAX_CATALOG_QUERY_LEN);

	rs_strncat(pszCatalogQuery, " WHERE true ", MAX_CATALOG_QUERY_LEN - strlen(pszCatalogQuery));

	getTableFilterClause(filterClause, pStmt, pCatalogName, cbCatalogName, pSchemaName, cbSchemaName,
		pTableName, cbTableName, pTableType, cbTableType, NO_SCHEMA_UNIVERSAL_QUERY, TRUE, NULL);

	rs_strncat(pszCatalogQuery, filterClause, MAX_CATALOG_QUERY_LEN - strlen(pszCatalogQuery));
	addOrderByClause(pszCatalogQuery, "TABLE_TYPE,TABLE_SCHEM,TABLE_NAME");
}

/*====================================================================================================================================================*/

// Datashare/Cross-db support svv_all_tables view
static void buildUniversalAllSchemaTablesQuery(char *pszCatalogQuery,
	RS_STMT_INFO *pStmt,
	SQLCHAR *pCatalogName,
	SQLSMALLINT cbCatalogName,
	SQLCHAR *pSchemaName,
	SQLSMALLINT cbSchemaName,
	SQLCHAR *pTableName,
	SQLSMALLINT cbTableName,
	SQLCHAR *pTableType,
	SQLSMALLINT cbTableType)
{
	char filterClause[MAX_CATALOG_QUERY_FILTER_LEN];

	rs_strncpy(pszCatalogQuery, "SELECT * FROM (SELECT CAST(DATABASE_NAME AS VARCHAR(124)) AS TABLE_CAT," 
		 " SCHEMA_NAME AS TABLE_SCHEM," 
		 " TABLE_NAME  AS TABLE_NAME,"
		 " CAST("
		 " CASE "
		 " WHEN SCHEMA_NAME='information_schema' "
		 "    AND TABLE_TYPE='TABLE' THEN 'SYSTEM TABLE' "
		 " WHEN SCHEMA_NAME='information_schema' "
		 "    AND TABLE_TYPE='VIEW' THEN 'SYSTEM VIEW' "
		 " ELSE TABLE_TYPE "
		 " END "
		 " AS VARCHAR(124)) AS TABLE_TYPE,"
		 " REMARKS"
		 " FROM PG_CATALOG.SVV_ALL_TABLES)", MAX_CATALOG_QUERY_LEN);

	rs_strncat(pszCatalogQuery, " WHERE true ", MAX_CATALOG_QUERY_LEN - strlen(pszCatalogQuery));

	getTableFilterClause(filterClause, pStmt, pCatalogName, cbCatalogName, pSchemaName, cbSchemaName,
		pTableName, cbTableName, pTableType, cbTableType, NO_SCHEMA_UNIVERSAL_QUERY, FALSE, "TABLE_CAT");

	rs_strncat(pszCatalogQuery, filterClause, MAX_CATALOG_QUERY_LEN - strlen(pszCatalogQuery));
	addOrderByClause(pszCatalogQuery, "TABLE_TYPE,TABLE_SCHEM,TABLE_NAME");
}

/*====================================================================================================================================================*/

static void buildExternalSchemaTablesQuery(char *pszCatalogQuery,
	RS_STMT_INFO *pStmt,
	SQLCHAR *pCatalogName,
	SQLSMALLINT cbCatalogName,
	SQLCHAR *pSchemaName,
	SQLSMALLINT cbSchemaName,
	SQLCHAR *pTableName,
	SQLSMALLINT cbTableName,
	SQLCHAR *pTableType,
	SQLSMALLINT cbTableType)
{
	char filterClause[MAX_CATALOG_QUERY_FILTER_LEN];

	// Basic query, without the join operation and subquery name appended to the end
	rs_strncpy(pszCatalogQuery, "SELECT * FROM (SELECT CAST(current_database() AS VARCHAR(124)) AS TABLE_CAT,"
			 " schemaname AS table_schem,"
			 " tablename AS TABLE_NAME,"
			 " 'TABLE' AS TABLE_TYPE,"
			 " NULL AS REMARKS"
			 " FROM svv_external_tables)",
				MAX_CATALOG_QUERY_LEN);

	rs_strncat(pszCatalogQuery, " WHERE true ", MAX_CATALOG_QUERY_LEN - strlen(pszCatalogQuery));

	getTableFilterClause(filterClause, pStmt, pCatalogName, cbCatalogName, pSchemaName, cbSchemaName,
		pTableName, cbTableName, pTableType, cbTableType, EXTERNAL_SCHEMA_QUERY, TRUE, NULL);

	rs_strncat(pszCatalogQuery, filterClause, MAX_CATALOG_QUERY_LEN - strlen(pszCatalogQuery));
	addOrderByClause(pszCatalogQuery, "TABLE_TYPE,TABLE_SCHEM,TABLE_NAME");
}

/*====================================================================================================================================================*/

static void buildLocalSchemaColumnsQuery(char *pszCatalogQuery,
	RS_STMT_INFO *pStmt,
	SQLCHAR *pCatalogName,
	SQLSMALLINT cbCatalogName,
	SQLCHAR *pSchemaName,
	SQLSMALLINT cbSchemaName,
	SQLCHAR *pTableName,
	SQLSMALLINT cbTableName,
	SQLCHAR *pColumnName,
	SQLSMALLINT cbColumnName)
{
	std::string result = "";
	char catalogFilter[MAX_LARGE_TEMP_BUF_LEN];
	char filterClause[MAX_CATALOG_QUERY_FILTER_LEN];


	getCatalogFilterCondition(catalogFilter, MAX_LARGE_TEMP_BUF_LEN, pStmt, (char *)pCatalogName, cbCatalogName,
								TRUE, NULL);


	result.append("SELECT * FROM ( ");
	result.append("SELECT current_database() AS TABLE_CAT, ");
	result.append("n.nspname AS TABLE_SCHEM, ");
	result.append("c.relname as TABLE_NAME , ");
	result.append("a.attname as COLUMN_NAME, ");
	result.append("CAST(case typname ");
	result.append("when 'text' THEN 12 ");
	result.append("when 'bit' THEN -7 ");
	result.append("when 'bool' THEN -7 ");
	result.append("when 'boolean' THEN -7 ");
	result.append("when 'varchar' THEN 12 ");
	result.append("when 'character varying' THEN 12 ");
	result.append("when 'char' THEN 1 ");
	result.append("when '\"char\"' THEN 1 ");
	result.append("when 'character' THEN 1 ");
	result.append("when 'nchar' THEN 12 ");
	result.append("when 'bpchar' THEN 1 ");
	result.append("when 'nvarchar' THEN 12 ");
	result.append("when 'date' THEN 91 ");
	result.append("when 'time' THEN 92 ");
	result.append("when 'time without time zone' THEN 92 ");
	result.append("when 'timetz' THEN 2013 ");
	result.append("when 'time with time zone' THEN 2013 ");
	result.append("when 'timestamp' THEN 93 ");
	result.append("when 'timestamp without time zone' THEN 93 ");
	result.append("when 'timestamptz' THEN 2014 ");
	result.append("when 'timestamp with time zone' THEN 2014 ");
	result.append("when 'smallint' THEN 5 ");
	result.append("when 'int2' THEN 5 ");
	result.append("when 'integer' THEN 4 ");
	result.append("when 'int' THEN 4 ");
	result.append("when 'int4' THEN 4 ");
	result.append("when 'bigint' THEN -5 ");
	result.append("when 'int8' THEN -5 ");
	result.append("when 'decimal' THEN 3 ");
	result.append("when 'real' THEN 7 ");
	result.append("when 'float4' THEN 7 ");
	result.append("when 'double precision' THEN 8 ");
	result.append("when 'float8' THEN 8 ");
	result.append("when 'float' THEN 6 ");
	result.append("when 'numeric' THEN 2 ");
	result.append("when '_float4' THEN 2003 ");
	result.append("when '_aclitem' THEN 2003 ");
	result.append("when '_text' THEN 2003 ");
	result.append("when 'bytea' THEN -2 ");
	result.append("when 'oid' THEN -5 ");
	result.append("when 'name' THEN 12 ");
	result.append("when '_int4' THEN 2003 ");
	result.append("when '_int2' THEN 2003 ");
	result.append("when 'ARRAY' THEN 2003 ");
	result.append("when 'geometry' THEN -4 ");
	result.append("when 'super' THEN -1 ");
	result.append("when 'varbyte' THEN -4 ");
	result.append("when 'geography' THEN -4 ");
	result.append("when 'intervaly2m' THEN 107 ");
	result.append("when 'intervald2s' THEN 110 ");
	result.append("else 0 END as SMALLINT) AS DATA_TYPE, ");
	result.append("t.typname as TYPE_NAME, ");
	result.append("case typname ");
	result.append("when 'int4' THEN 10 ");
	result.append("when 'bit' THEN 1 ");
	result.append("when 'bool' THEN 1 ");
	result.append("when 'varchar' THEN atttypmod -4 ");
	result.append("when 'character varying' THEN atttypmod -4 ");
	result.append("when 'char' THEN atttypmod -4 ");
	result.append("when 'character' THEN atttypmod -4 ");
	result.append("when 'nchar' THEN atttypmod -4 ");
	result.append("when 'bpchar' THEN atttypmod -4 ");
	result.append("when 'nvarchar' THEN atttypmod -4 ");
	result.append("when 'date' THEN 13 ");
	result.append("when 'time' THEN 15 ");
	result.append("when 'time without time zone' THEN 15 ");
	result.append("when 'timetz' THEN 21 ");
	result.append("when 'time with time zone' THEN 21 ");
	result.append("when 'timestamp' THEN 29 ");
	result.append("when 'timestamp without time zone' THEN 29 ");
	result.append("when 'timestamptz' THEN 35 ");
	result.append("when 'timestamp with time zone' THEN 35 ");
	result.append("when 'smallint' THEN 5 ");
	result.append("when 'int2' THEN 5 ");
	result.append("when 'integer' THEN 10 ");
	result.append("when 'int' THEN 10 ");
	result.append("when 'int4' THEN 10 ");
	result.append("when 'bigint' THEN 19 ");
	result.append("when 'int8' THEN 19 ");
	result.append("when 'decimal' then (atttypmod - 4) >> 16 ");
	result.append("when 'real' THEN 8 ");
	result.append("when 'float4' THEN 8 ");
	result.append("when 'double precision' THEN 17 ");
	result.append("when 'float8' THEN 17 ");
	result.append("when 'float' THEN 17 ");
	result.append("when 'numeric' THEN (atttypmod - 4) >> 16 ");
	result.append("when '_float4' THEN 8 ");
	result.append("when 'oid' THEN 10 ");
	result.append("when '_int4' THEN 10 ");
	result.append("when '_int2' THEN 5 ");
	result.append("when 'geometry' THEN NULL ");
	result.append("when 'super' THEN NULL ");
	result.append("when 'varbyte' THEN NULL ");
	result.append("when 'geography' THEN NULL ");
	result.append("when 'intervaly2m' THEN 32 ");
	result.append("when 'intervald2s' THEN 64 ");
	//      if (connSettings.m_unknownLength == null)
	{
		result.append("else 2147483647 end as COLUMN_SIZE , ");
	}
	/*      else
	{
	result.append("else ");
	result.append(connSettings.m_unknownLength);
	result.append(" end as COLUMN_SIZE , ");
	} */
	result.append("null as BUFFER_LENGTH , ");
	result.append("case typname ");
	result.append("when 'float4' then 8 ");
	result.append("when 'float8' then 17 ");
	result.append("when 'numeric' then (atttypmod - 4) & 65535 ");
	result.append("when 'time without time zone' then 6 ");
	result.append("when 'timetz' then 6 ");
	result.append("when 'time with time zone' then 6 ");
	result.append("when 'timestamp without time zone' then 6 ");
	result.append("when 'timestamp' then 6 ");
	result.append("when 'geometry' then NULL ");
	result.append("when 'super' then NULL ");
	result.append("when 'varbyte' then NULL ");
	result.append("when 'geography' then NULL ");
	result.append("when 'intervaly2m' THEN 0 ");
	result.append("when 'intervald2s' THEN 6 ");
	result.append("else 0 end as DECIMAL_DIGITS, ");
	result.append("case typname ");
	result.append("when 'varbyte' then 2 ");
	result.append("when 'geography' then 2 ");
	result.append("when 'varchar' THEN 0 ");
	result.append("when 'character varying' THEN 0 ");
	result.append("when 'char' THEN 0 ");
	result.append("when 'character' THEN 0 ");
	result.append("when 'nchar' THEN 0 ");
	result.append("when 'bpchar' THEN 0 ");
	result.append("when 'nvarchar' THEN 0 ");
	result.append("else 10 end as NUM_PREC_RADIX, ");
	result.append("case a.attnotnull OR (t.typtype = 'd' AND t.typnotnull) ");
	result.append("when 'false' then 1 ");
	result.append("when NULL then 2 ");
	result.append("else 0 end AS NULLABLE , ");
	result.append("dsc.description as REMARKS , ");
	result.append("pg_catalog.pg_get_expr(def.adbin, def.adrelid) AS COLUMN_DEF, ");
	result.append("CAST(case typname ");
	result.append("when 'text' THEN 12 ");
	result.append("when 'bit' THEN -7 ");
	result.append("when 'bool' THEN -7 ");
	result.append("when 'boolean' THEN -7 ");
	result.append("when 'varchar' THEN 12 ");
	result.append("when 'character varying' THEN 12 ");
	result.append("when '\"char\"' THEN 1 ");
	result.append("when 'char' THEN 1 ");
	result.append("when 'character' THEN 1 ");
	result.append("when 'nchar' THEN 1 ");
	result.append("when 'bpchar' THEN 1 ");
	result.append("when 'nvarchar' THEN 12 ");
	result.append("when 'date' THEN 91 ");
	result.append("when 'time' THEN 92 ");
	result.append("when 'time without time zone' THEN 92 ");
	result.append("when 'timetz' THEN 2013 ");
	result.append("when 'time with time zone' THEN 2013 ");
	result.append("when 'timestamp with time zone' THEN 2014 ");
	result.append("when 'timestamp' THEN 93 ");
	result.append("when 'timestamp without time zone' THEN 93 ");
	result.append("when 'smallint' THEN 5 ");
	result.append("when 'int2' THEN 5 ");
	result.append("when 'integer' THEN 4 ");
	result.append("when 'int' THEN 4 ");
	result.append("when 'int4' THEN 4 ");
	result.append("when 'bigint' THEN -5 ");
	result.append("when 'int8' THEN -5 ");
	result.append("when 'decimal' THEN 3 ");
	result.append("when 'real' THEN 7 ");
	result.append("when 'float4' THEN 7 ");
	result.append("when 'double precision' THEN 8 ");
	result.append("when 'float8' THEN 8 ");
	result.append("when 'float' THEN 6 ");
	result.append("when 'numeric' THEN 2 ");
	result.append("when '_float4' THEN 2003 ");
	result.append("when 'timestamptz' THEN 2014 ");
	result.append("when 'timestamp with time zone' THEN 2014 ");
	result.append("when '_aclitem' THEN 2003 ");
	result.append("when '_text' THEN 2003 ");
	result.append("when 'bytea' THEN -2 ");
	result.append("when 'oid' THEN -5 ");
	result.append("when 'name' THEN 12 ");
	result.append("when '_int4' THEN 2003 ");
	result.append("when '_int2' THEN 2003 ");
	result.append("when 'ARRAY' THEN 2003 ");
	result.append("when 'geometry' THEN -4 ");
	result.append("when 'super' THEN -1 ");
	result.append("when 'varbyte' THEN -4 ");
	result.append("when 'geography' THEN -4 ");
	result.append("when 'intervaly2m' THEN 107 ");
	result.append("when 'intervald2s' THEN 110 ");
	result.append("else 0 END as SMALLINT) AS SQL_DATA_TYPE, ");
	result.append("CAST(NULL AS SMALLINT) as SQL_DATETIME_SUB , ");
	result.append("case typname ");
	result.append("when 'int4' THEN 10 ");
	result.append("when 'bit' THEN 1 ");
	result.append("when 'bool' THEN 1 ");
	result.append("when 'varchar' THEN atttypmod -4 ");
	result.append("when 'character varying' THEN atttypmod -4 ");
	result.append("when 'char' THEN atttypmod -4 ");
	result.append("when 'character' THEN atttypmod -4 ");
	result.append("when 'nchar' THEN atttypmod -4 ");
	result.append("when 'bpchar' THEN atttypmod -4 ");
	result.append("when 'nvarchar' THEN atttypmod -4 ");
	result.append("when 'date' THEN 13 ");
	result.append("when 'time' THEN 15 ");
	result.append("when 'time without time zone' THEN 15 ");
	result.append("when 'timetz' THEN 21 ");
	result.append("when 'time with time zone' THEN 21 ");
	result.append("when 'timestamp' THEN 29 ");
	result.append("when 'timestamp without time zone' THEN 29 ");
	result.append("when 'timestamptz' THEN 35 ");
	result.append("when 'timestamp with time zone' THEN 35 ");
	result.append("when 'smallint' THEN 5 ");
	result.append("when 'int2' THEN 5 ");
	result.append("when 'integer' THEN 10 ");
	result.append("when 'int' THEN 10 ");
	result.append("when 'int4' THEN 10 ");
	result.append("when 'bigint' THEN 19 ");
	result.append("when 'int8' THEN 19 ");
	result.append("when 'decimal' then ((atttypmod - 4) >> 16) & 65535 ");
	result.append("when 'real' THEN 8 ");
	result.append("when 'float4' THEN 8 ");
	result.append("when 'double precision' THEN 17 ");
	result.append("when 'float8' THEN 17 ");
	result.append("when 'float' THEN 17 ");
	result.append("when 'numeric' THEN ((atttypmod - 4) >> 16) & 65535 ");
	result.append("when '_float4' THEN 8 ");
	result.append("when 'oid' THEN 10 ");
	result.append("when '_int4' THEN 10 ");
	result.append("when '_int2' THEN 5 ");
	result.append("when 'geometry' THEN NULL ");
	result.append("when 'super' THEN NULL ");
	result.append("when 'varbyte' THEN NULL ");
	result.append("when 'geography' THEN NULL ");
	result.append("when 'intervaly2m' THEN 32 ");
	result.append("when 'intervald2s' THEN 64 ");
	//      if (connSettings.m_unknownLength == null)
//	{
		result.append("else 2147483647 end as CHAR_OCTET_LENGTH , ");
//	}
	/*      else
	{
	result.append("else ");
	result.append(connSettings.m_unknownLength);
	result.append(" end as CHAR_OCTET_LENGTH , ");
	} */
	result.append("a.attnum AS ORDINAL_POSITION, ");
	result.append("case a.attnotnull OR (t.typtype = 'd' AND t.typnotnull) ");
	result.append("when 'false' then 'YES' ");
	result.append("when NULL then '' ");
	result.append("else 'NO' end AS IS_NULLABLE ");
/*	result.append("null as SCOPE_CATALOG  ");
	result.append("null as SCOPE_SCHEMA , ");
	result.append("null as SCOPE_TABLE, ");
	result.append("t.typbasetype AS SOURCE_DATA_TYPE , ");
	result.append("CASE WHEN left(pg_catalog.pg_get_expr(def.adbin, def.adrelid), 16) = 'default_identity' THEN 'YES' ");
	result.append("ELSE 'NO' END AS IS_AUTOINCREMENT, ");
	result.append("IS_AUTOINCREMENT AS IS_GENERATEDCOLUMN ");
*/
	result.append("FROM pg_catalog.pg_namespace n  JOIN pg_catalog.pg_class c ON (c.relnamespace = n.oid) ");
	result.append("JOIN pg_catalog.pg_attribute a ON (a.attrelid=c.oid) ");
	result.append("JOIN pg_catalog.pg_type t ON (a.atttypid = t.oid) ");
	result.append("LEFT JOIN pg_catalog.pg_attrdef def ON (a.attrelid=def.adrelid AND a.attnum = def.adnum) ");
	result.append("LEFT JOIN pg_catalog.pg_description dsc ON (c.oid=dsc.objoid AND a.attnum = dsc.objsubid) ");
	result.append("LEFT JOIN pg_catalog.pg_class dc ON (dc.oid=dsc.classoid AND dc.relname='pg_class') ");
	result.append("LEFT JOIN pg_catalog.pg_namespace dn ON (dc.relnamespace=dn.oid AND dn.nspname='pg_catalog') ");
	result.append("WHERE a.attnum > 0 AND NOT a.attisdropped    ");

	result.append(catalogFilter);

	filterClause[0] = '\0';
	addLikeOrEqualFilterCondition(pStmt, filterClause, "n.nspname", (char *)pSchemaName, cbSchemaName);
	addLikeOrEqualFilterCondition(pStmt, filterClause, "c.relname", (char *)pTableName, cbTableName);
	addLikeOrEqualFilterCondition(pStmt, filterClause, "attname", (char *)pColumnName, cbColumnName);

	result.append(filterClause);

	result.append(" ORDER BY TABLE_SCHEM,c.relname,attnum ) ");


	// This part uses redshift method PG_GET_LATE_BINDING_VIEW_COLS() to
	// get the column list for late binding view.
	result.append(" UNION ALL ");
	result.append("SELECT current_database()::VARCHAR(128) AS TABLE_CAT, ");
	result.append("schemaname::varchar(128) AS table_schem, ");
	result.append("tablename::varchar(128) AS table_name, ");
	result.append("columnname::varchar(128) AS column_name, ");
	result.append("CAST(CASE columntype_rep ");
	result.append("WHEN 'text' THEN 12 ");
	result.append("WHEN 'bit' THEN -7 ");
	result.append("WHEN 'bool' THEN -7 ");
	result.append("WHEN 'boolean' THEN -7 ");
	result.append("WHEN 'varchar' THEN 12 ");
	result.append("WHEN 'character varying' THEN 12 ");
	result.append("WHEN 'char' THEN 1 ");
	result.append("WHEN 'character' THEN 1 ");
	result.append("WHEN 'nchar' THEN 1 ");
	result.append("WHEN 'bpchar' THEN 1 ");
	result.append("WHEN 'nvarchar' THEN 12 ");
	result.append("WHEN '\"char\"' THEN 1 ");
	result.append("WHEN 'date' THEN 91 ");
	result.append("when 'time' THEN 92 ");
	result.append("when 'time without time zone' THEN 92 ");
	result.append("when 'timetz' THEN 2013 ");
	result.append("when 'time with time zone' THEN 2013 ");
	result.append("WHEN 'timestamp' THEN 93 ");
	result.append("WHEN 'timestamp without time zone' THEN 93 ");
	result.append("when 'timestamptz' THEN 2014 ");
	result.append("WHEN 'timestamp with time zone' THEN 2014 ");
	result.append("WHEN 'smallint' THEN 5 ");
	result.append("WHEN 'int2' THEN 5 ");
	result.append("WHEN 'integer' THEN 4 ");
	result.append("WHEN 'int' THEN 4 ");
	result.append("WHEN 'int4' THEN 4 ");
	result.append("WHEN 'bigint' THEN -5 ");
	result.append("WHEN 'int8' THEN -5 ");
	result.append("WHEN 'decimal' THEN 3 ");
	result.append("WHEN 'real' THEN 7 ");
	result.append("WHEN 'float4' THEN 7 ");
	result.append("WHEN 'double precision' THEN 8 ");
	result.append("WHEN 'float8' THEN 8 ");
	result.append("WHEN 'float' THEN 6 ");
	result.append("WHEN 'numeric' THEN 2 ");
	result.append("WHEN 'timestamptz' THEN 2014 ");
	result.append("WHEN 'bytea' THEN -2 ");
	result.append("WHEN 'oid' THEN -5 ");
	result.append("WHEN 'name' THEN 12 ");
	result.append("WHEN 'ARRAY' THEN 2003 ");
	result.append("WHEN 'geometry' THEN -4 ");
	result.append("WHEN 'super' THEN -1 ");
	result.append("WHEN 'varbyte' THEN -4 ");
	result.append("WHEN 'geography' THEN -4 ");
	result.append("when 'intervaly2m' THEN 107 ");
	result.append("when 'intervald2s' THEN 110 ");
	result.append("ELSE 0 END AS SMALLINT) AS DATA_TYPE, ");
	result.append("COALESCE(NULL,CASE columntype WHEN 'boolean' THEN 'bool' ");
	result.append("WHEN 'character varying' THEN 'varchar' ");
	result.append("WHEN '\"char\"' THEN 'char' ");
	result.append("WHEN 'smallint' THEN 'int2' ");
	result.append("WHEN 'integer' THEN 'int4'");
	result.append("WHEN 'bigint' THEN 'int8' ");
	result.append("WHEN 'real' THEN 'float4' ");
	result.append("WHEN 'double precision' THEN 'float8' ");
	result.append("WHEN 'time without time zone' THEN 'time' ");
	result.append("WHEN 'time with time zone' THEN 'timetz' ");
	result.append("WHEN 'timestamp without time zone' THEN 'timestamp' ");
	result.append("WHEN 'timestamp with time zone' THEN 'timestamptz' ");
	result.append("ELSE columntype END) AS TYPE_NAME,  ");
	result.append("CASE columntype_rep ");
	result.append("WHEN 'int4' THEN 10  ");
	result.append("WHEN 'bit' THEN 1    ");
	result.append("WHEN 'bool' THEN 1");
	result.append("WHEN 'boolean' THEN 1");
	result.append("WHEN 'varchar' THEN isnull(nullif(regexp_substr (columntype,'[0-9]+',7),''),'0')::INTEGER ");
	result.append("WHEN 'character varying' THEN isnull(nullif(regexp_substr (columntype,'[0-9]+',7),''),'0')::INTEGER ");
	result.append("WHEN 'char' THEN isnull(nullif(regexp_substr (columntype,'[0-9]+',4),''),'0')::INTEGER ");
	result.append("WHEN 'character' THEN isnull(nullif(regexp_substr (columntype,'[0-9]+',4),''),'0')::INTEGER ");
	result.append("WHEN 'nchar' THEN isnull(nullif(regexp_substr (columntype,'[0-9]+',7),''),'0')::INTEGER ");
	result.append("WHEN 'bpchar' THEN isnull(nullif(regexp_substr (columntype,'[0-9]+',7),''),'0')::INTEGER ");
	result.append("WHEN 'nvarchar' THEN isnull(nullif(regexp_substr (columntype,'[0-9]+',7),''),'0')::INTEGER ");
	result.append("WHEN 'date' THEN 13 ");
	result.append("WHEN 'time' THEN 15 ");
	result.append("WHEN 'time without time zone' THEN 15 ");
	result.append("WHEN 'timetz' THEN 21 ");
	result.append("WHEN 'timestamp' THEN 29 ");
	result.append("WHEN 'timestamp without time zone' THEN 29 ");
	result.append("WHEN 'time with time zone' THEN 21 ");
	result.append("WHEN 'timestamptz' THEN 35 ");
	result.append("WHEN 'timestamp with time zone' THEN 35 ");
	result.append("WHEN 'smallint' THEN 5 ");
	result.append("WHEN 'int2' THEN 5 ");
	result.append("WHEN 'integer' THEN 10 ");
	result.append("WHEN 'int' THEN 10 ");
	result.append("WHEN 'int4' THEN 10 ");
	result.append("WHEN 'bigint' THEN 19 ");
	result.append("WHEN 'int8' THEN 19 ");
	result.append("WHEN 'decimal' THEN isnull(nullif(regexp_substr (columntype,'[0-9]+',7),''),'0')::INTEGER ");
	result.append("WHEN 'real' THEN 8 ");
	result.append("WHEN 'float4' THEN 8 ");
	result.append("WHEN 'double precision' THEN 17 ");
	result.append("WHEN 'float8' THEN 17 ");
	result.append("WHEN 'float' THEN 17");
	result.append("WHEN 'numeric' THEN isnull(nullif(regexp_substr (columntype,'[0-9]+',7),''),'0')::INTEGER ");
	result.append("WHEN '_float4' THEN 8 ");
	result.append("WHEN 'oid' THEN 10 ");
	result.append("WHEN '_int4' THEN 10 ");
	result.append("WHEN '_int2' THEN 5 ");
	result.append("WHEN 'geometry' THEN NULL ");
	result.append("WHEN 'super' THEN NULL ");
	result.append("WHEN 'varbyte' THEN NULL ");
	result.append("WHEN 'geography' THEN NULL ");
	result.append("WHEN 'intervaly2m' THEN 32 ");
	result.append("WHEN 'intervald2s' THEN 64 ");
	result.append("ELSE 2147483647 END AS COLUMN_SIZE, ");
	result.append("NULL AS BUFFER_LENGTH, ");
	result.append("CASE REGEXP_REPLACE(columntype,'[()0-9,]') ");
	result.append("WHEN 'real' THEN 8 ");
	result.append("WHEN 'float4' THEN 8 ");
	result.append("WHEN 'double precision' THEN 17 ");
	result.append("WHEN 'float8' THEN 17 ");
	result.append("WHEN 'timestamp' THEN 6 ");
	result.append("WHEN 'timestamp without time zone' THEN 6 ");
	result.append("WHEN 'geometry' THEN NULL ");
	result.append("WHEN 'super' THEN NULL ");
	result.append("WHEN 'numeric' THEN regexp_substr (columntype,'[0-9]+',charindex (',',columntype))::INTEGER ");
	result.append("WHEN 'varbyte' THEN NULL ");
	result.append("WHEN 'geography' THEN NULL ");
	result.append("WHEN 'intervaly2m' THEN 0 ");
	result.append("WHEN 'intervald2s' THEN 6 ");
	result.append("ELSE 0 END AS DECIMAL_DIGITS, ");
	result.append("CASE columntype ");
	result.append("WHEN 'varbyte' THEN 2 ");
	result.append("WHEN 'geography' THEN 2 ");
	result.append("when 'varchar' THEN 0 ");
	result.append("when 'character varying' THEN 0 ");
	result.append("when 'char' THEN 0 ");
	result.append("when 'character' THEN 0 ");
	result.append("when 'nchar' THEN 0 ");
	result.append("when 'bpchar' THEN 0 ");
	result.append("when 'nvarchar' THEN 0 ");
	result.append("ELSE 10 END AS NUM_PREC_RADIX, ");
	result.append("NULL AS NULLABLE,  NULL AS REMARKS,   NULL AS COLUMN_DEF, ");
	result.append("CAST(CASE columntype_rep ");
	result.append("WHEN 'text' THEN 12 ");
	result.append("WHEN 'bit' THEN -7 ");
	result.append("WHEN 'bool' THEN -7 ");
	result.append("WHEN 'boolean' THEN -7 ");
	result.append("WHEN 'varchar' THEN 12 ");
	result.append("WHEN 'character varying' THEN 12 ");
	result.append("WHEN 'char' THEN 1 ");
	result.append("WHEN 'character' THEN 1 ");
	result.append("WHEN 'nchar' THEN 12 ");
	result.append("WHEN 'bpchar' THEN 1 ");
	result.append("WHEN 'nvarchar' THEN 12 ");
	result.append("WHEN '\"char\"' THEN 1 ");
	result.append("WHEN 'date' THEN 91 ");
	result.append("WHEN 'time' THEN 92 ");
	result.append("WHEN 'time without time zone' THEN 92 ");
	result.append("WHEN 'timetz' THEN 2013 ");
	result.append("WHEN 'time with time zone' THEN 2013 ");
	result.append("WHEN 'timestamp' THEN 93 ");
	result.append("WHEN 'timestamp without time zone' THEN 93 ");
	result.append("WHEN 'timestamptz' THEN 2014 ");
	result.append("WHEN 'timestamp with time zone' THEN 2014 ");
	result.append("WHEN 'smallint' THEN 5 ");
	result.append("WHEN 'int2' THEN 5 ");
	result.append("WHEN 'integer' THEN 4 ");
	result.append("WHEN 'int' THEN 4 ");
	result.append("WHEN 'int4' THEN 4 ");
	result.append("WHEN 'bigint' THEN -5 ");
	result.append("WHEN 'int8' THEN -5 ");
	result.append("WHEN 'decimal' THEN 3 ");
	result.append("WHEN 'real' THEN 7 ");
	result.append("WHEN 'float4' THEN 7 ");
	result.append("WHEN 'double precision' THEN 8 ");
	result.append("WHEN 'float8' THEN 8 ");
	result.append("WHEN 'float' THEN 6 ");
	result.append("WHEN 'numeric' THEN 2 ");
	result.append("WHEN 'bytea' THEN -2 ");
	result.append("WHEN 'oid' THEN -5 ");
	result.append("WHEN 'name' THEN 12 ");
	result.append("WHEN 'ARRAY' THEN 2003 ");
	result.append("WHEN 'geometry' THEN -4 ");
	result.append("WHEN 'super' THEN -1 ");
	result.append("WHEN 'varbyte' THEN -4 ");
	result.append("WHEN 'geography' THEN -4 ");
	result.append("WHEN 'intervaly2m' THEN 107 ");
	result.append("WHEN 'intervald2s' THEN 110 ");
	result.append("ELSE 0 END AS SMALLINT) AS SQL_DATA_TYPE, ");
	result.append("CAST(NULL AS SMALLINT) AS SQL_DATETIME_SUB, CASE ");
	result.append("WHEN LEFT (columntype,7) = 'varchar' THEN isnull(nullif(regexp_substr (columntype,'[0-9]+',7),''),'0')::INTEGER ");
	result.append("WHEN LEFT (columntype,4) = 'char' THEN isnull(nullif(regexp_substr (columntype,'[0-9]+',4),''),'0')::INTEGER ");
	result.append("WHEN columntype = 'string' THEN 16383  ELSE NULL ");
	result.append("END AS CHAR_OCTET_LENGTH, columnnum AS ORDINAL_POSITION, ");
	result.append("NULL AS IS_NULLABLE ");
	/*	result.append("NULL AS IS_NULLABLE,  NULL AS SCOPE_CATALOG,  NULL AS SCOPE_SCHEMA, ");
		result.append("NULL AS SCOPE_TABLE, NULL AS SOURCE_DATA_TYPE, 'NO' AS IS_AUTOINCREMENT, ");
		result.append("'NO' as IS_GENERATEDCOLUMN "); 
	*/
	result.append("FROM (select lbv_cols.schemaname, ");
	result.append("lbv_cols.tablename, lbv_cols.columnname,");
	result.append("REGEXP_REPLACE(REGEXP_REPLACE(lbv_cols.columntype,'\\\\(.*\\\\)'),'^_.+','ARRAY') as columntype_rep,");
	result.append("columntype, ");
	result.append("lbv_cols.columnnum ");
	result.append("from pg_get_late_binding_view_cols() lbv_cols( ");
	result.append("schemaname name, tablename name, columnname name, ");
	result.append("columntype text, columnnum int)) lbv_columns  ");
	result.append(" WHERE true ");

	// Apply the filters to the column list for late binding view.
	result.append(catalogFilter);

	filterClause[0] = '\0';
	addLikeOrEqualFilterCondition(pStmt, filterClause, "schemaname", (char *)pSchemaName, cbSchemaName);
	addLikeOrEqualFilterCondition(pStmt, filterClause, "tablename", (char *)pTableName, cbTableName);
	addLikeOrEqualFilterCondition(pStmt, filterClause, "columnname", (char *)pColumnName, cbColumnName);

	result.append(filterClause);

	rs_strncpy(pszCatalogQuery, result.c_str(), MAX_CATALOG_QUERY_LEN);
}

/*====================================================================================================================================================*/

static void buildUniversalSchemaColumnsQuery(char *pszCatalogQuery,
	RS_STMT_INFO *pStmt,
	SQLCHAR *pCatalogName,
	SQLSMALLINT cbCatalogName,
	SQLCHAR *pSchemaName,
	SQLSMALLINT cbSchemaName,
	SQLCHAR *pTableName,
	SQLSMALLINT cbTableName,
	SQLCHAR *pColumnName,
	SQLSMALLINT cbColumnName)
{
	std::string result = "";
	char catalogFilter[MAX_LARGE_TEMP_BUF_LEN];
	char filterClause[MAX_CATALOG_QUERY_FILTER_LEN];
//	std::string unknownColumnSize = "2147483647";

	getCatalogFilterCondition(catalogFilter, MAX_LARGE_TEMP_BUF_LEN, pStmt, (char *)pCatalogName, cbCatalogName,
								TRUE, NULL);


	// NOTE: Explicit cast on current_database() prevents bug where data returned from server
	// has incorrect length and displays random characters. [JDBC-529]
	result.append("SELECT current_database()::varchar(128) AS TABLE_CAT,"
		 " table_schema AS TABLE_SCHEM,"
		 " table_name,"
		 " COLUMN_NAME,"
		 " CAST(CASE regexp_replace(data_type, '^_.+', 'ARRAY')"
		 " WHEN 'text' THEN 12"
		 " WHEN 'bit' THEN -7"
		 " WHEN 'bool' THEN -7"
		 " WHEN 'boolean' THEN -7"
		 " WHEN 'varchar' THEN 12"
		 " WHEN 'character varying' THEN 12"
		 " WHEN 'char' THEN 1"
		 " WHEN 'character' THEN 1"
		 " WHEN 'nchar' THEN 1"
		 " WHEN 'bpchar' THEN 1"
		 " WHEN 'nvarchar' THEN 12"
		 " WHEN '\"char\"' THEN 1"
		 " WHEN 'date' THEN 91"
		 " WHEN 'time' THEN 92 "
		 " WHEN 'time without time zone' THEN 92 "
		 " WHEN 'timetz' THEN 2013 "
		 " WHEN 'time with time zone' THEN 2013 "
		 " WHEN 'timestamp' THEN 93"
		 " WHEN 'timestamp without time zone' THEN 93"
		 " WHEN 'timestamptz' THEN 2014"
		 " WHEN 'timestamp with time zone' THEN 2014"
		 " WHEN 'smallint' THEN 5"
		 " WHEN 'int2' THEN 5"
		 " WHEN 'integer' THEN 4"
		 " WHEN 'int' THEN 4"
		 " WHEN 'int4' THEN 4"
		 " WHEN 'bigint' THEN -5"
		 " WHEN 'int8' THEN -5"
		 " WHEN 'decimal' THEN 3"
		 " WHEN 'real' THEN 7"
		 " WHEN 'float4' THEN 7"
		 " WHEN 'double precision' THEN 8"
		 " WHEN 'float8' THEN 8"
		 " WHEN 'float' THEN 6"
		 " WHEN 'numeric' THEN 2"
		 " WHEN 'bytea' THEN -2"
		 " WHEN 'oid' THEN -5"
		 " WHEN 'name' THEN 12"
		 " WHEN 'ARRAY' THEN 2003"
		 " WHEN 'geometry' THEN -4 "
		 " WHEN 'super' THEN -1 "
		 " WHEN 'varbyte' THEN -4 "
		 " WHEN 'geography' THEN -4 "
		 " WHEN 'intervaly2m' THEN 107 "
		 " WHEN 'intervald2s' THEN 110 "
		 " ELSE 0 END AS SMALLINT) AS DATA_TYPE,"
		 " COALESCE("
		 " domain_name,"
		 " CASE data_type"
		 " WHEN 'boolean' THEN 'bool'"
		 " WHEN 'character varying' THEN 'varchar'"
		 " WHEN '\"char\"' THEN 'char'"
		 " WHEN 'smallint' THEN 'int2'"
		 " WHEN 'integer' THEN 'int4'"
		 " WHEN 'bigint' THEN 'int8'"
		 " WHEN 'real' THEN 'float4'"
		 " WHEN 'double precision' THEN 'float8'"
		 " WHEN 'time without time zone' THEN 'time'"
		 " WHEN 'time with time zone' THEN 'timetz'"
		 " WHEN 'timestamp without time zone' THEN 'timestamp'"
		 " WHEN 'timestamp with time zone' THEN 'timestamptz'"
		 " ELSE data_type"
		 " END) AS TYPE_NAME,"
		 " CASE data_type"
		 " WHEN 'int4' THEN 10"
		 " WHEN 'bit' THEN 1"
		 " WHEN 'bool' THEN 1"
		 " WHEN 'boolean' THEN 1"
		 " WHEN 'varchar' THEN character_maximum_length"
		 " WHEN 'character varying' THEN character_maximum_length"
		 " WHEN 'char' THEN character_maximum_length"
		 " WHEN 'character' THEN character_maximum_length"
		 " WHEN 'nchar' THEN character_maximum_length"
		 " WHEN 'bpchar' THEN character_maximum_length"
		 " WHEN 'nvarchar' THEN character_maximum_length"
		 " WHEN 'date' THEN 13"
		 " WHEN 'time' THEN 15 "
		 " WHEN 'time without time zone' THEN 15 "
		 " WHEN 'timetz' THEN 21 "
		 " WHEN 'time with time zone' THEN 21 "
		 " WHEN 'timestamp' THEN 29"
		 " WHEN 'timestamp without time zone' THEN 29"
		 " WHEN 'timestamptz' THEN 35"
		 " WHEN 'timestamp with time zone' THEN 35"
		 " WHEN 'smallint' THEN 5"
		 " WHEN 'int2' THEN 5"
		 " WHEN 'integer' THEN 10"
		 " WHEN 'int' THEN 10"
		 " WHEN 'int4' THEN 10"
		 " WHEN 'bigint' THEN 19"
		 " WHEN 'int8' THEN 19"
		 " WHEN 'decimal' THEN numeric_precision"
		 " WHEN 'real' THEN 8"
		 " WHEN 'float4' THEN 8"
		 " WHEN 'double precision' THEN 17"
		 " WHEN 'float8' THEN 17"
		 " WHEN 'float' THEN 17"
		 " WHEN 'numeric' THEN numeric_precision"
		 " WHEN '_float4' THEN 8"
		 " WHEN 'oid' THEN 10"
		 " WHEN '_int4' THEN 10"
		 " WHEN '_int2' THEN 5"
		 " WHEN 'geometry' THEN NULL"
		 " WHEN 'super' THEN NULL"
		 " WHEN 'varbyte' THEN NULL"
		 " WHEN 'geography' THEN NULL"
		 " WHEN 'intervaly2m' THEN 32"
		 " WHEN 'intervald2s' THEN 64"
		 " ELSE 2147483647" 
		 " END AS COLUMN_SIZE,"
		 " NULL AS BUFFER_LENGTH,"
		 " CASE data_type"
		 " WHEN 'real' THEN 8"
		 " WHEN 'float4' THEN 8"
		 " WHEN 'double precision' THEN 17"
		 " WHEN 'float8' THEN 17"
		 " WHEN 'numeric' THEN numeric_scale"
		 " WHEN 'time' THEN 6"
		 " WHEN 'time without time zone' THEN 6"
		 " WHEN 'timetz' THEN 6"
		 " WHEN 'time with time zone' THEN 6"
		 " WHEN 'timestamp' THEN 6"
		 " WHEN 'timestamp without time zone' THEN 6"
		 " WHEN 'timestamptz' THEN 6"
		 " WHEN 'timestamp with time zone' THEN 6"
		 " WHEN 'geometry' THEN NULL"
		 " WHEN 'super' THEN NULL"
		 " WHEN 'varbyte' THEN NULL"
		 " WHEN 'geography' THEN NULL"
		 " WHEN 'intervaly2m' THEN 0"
		 " WHEN 'intervald2s' THEN 6"
		 " ELSE 0"
		 " END AS DECIMAL_DIGITS,"
		 " CASE data_type"
		 " WHEN 'varbyte' THEN 2"
		 " WHEN 'geography' THEN 2"
		 " when 'varchar' THEN 0 "
		 " when 'character varying' THEN 0 "
		 " when 'char' THEN 0 "
		 " when 'character' THEN 0 "
		 " when 'nchar' THEN 0 "
		 " when 'bpchar' THEN 0 "
		 " when 'nvarchar' THEN 0 "
		 " ELSE 10"
		 " END AS NUM_PREC_RADIX,"
		 " CASE is_nullable WHEN 'YES' THEN 1"
		 " WHEN 'NO' THEN 0"
		 " ELSE 2 end AS NULLABLE,"
		 " REMARKS,"
		 " column_default AS COLUMN_DEF,"
		 " CAST(CASE regexp_replace(data_type, '^_.+', 'ARRAY')"
		 " WHEN 'text' THEN 12"
		 " WHEN 'bit' THEN -7"
		 " WHEN 'bool' THEN -7"
		 " WHEN 'boolean' THEN -7"
		 " WHEN 'varchar' THEN 12"
		 " WHEN 'character varying' THEN 12"
		 " WHEN 'char' THEN 1"
		 " WHEN 'character' THEN 1"
		 " WHEN 'nchar' THEN 1"
		 " WHEN 'bpchar' THEN 1"
		 " WHEN 'nvarchar' THEN 12"
		 " WHEN '\"char\"' THEN 1"
		 " WHEN 'date' THEN 91"
		 " WHEN 'time' THEN 92 "
		 " WHEN 'time without time zone' THEN 92 "
		 " WHEN 'timetz' THEN 2013 "
		 " WHEN 'time with time zone' THEN 2013 "
		 " WHEN 'timestamp' THEN 93"
		 " WHEN 'timestamp without time zone' THEN 93"
		 " WHEN 'timestamptz' THEN 2014"
		 " WHEN 'timestamp with time zone' THEN 2014"
		 " WHEN 'smallint' THEN 5"
		 " WHEN 'int2' THEN 5"
		 " WHEN 'integer' THEN 4"
		 " WHEN 'int' THEN 4"
		 " WHEN 'int4' THEN 4"
		 " WHEN 'bigint' THEN -5"
		 " WHEN 'int8' THEN -5"
		 " WHEN 'decimal' THEN 3"
		 " WHEN 'real' THEN 7"
		 " WHEN 'float4' THEN 7"
		 " WHEN 'double precision' THEN 8"
		 " WHEN 'float8' THEN 8"
		 " WHEN 'float' THEN 6"
		 " WHEN 'numeric' THEN 2"
		 " WHEN 'bytea' THEN -2"
		 " WHEN 'oid' THEN -5"
		 " WHEN 'name' THEN 12"
		 " WHEN 'ARRAY' THEN 2003"
		 " WHEN 'geometry' THEN -4"
		 " WHEN 'super' THEN -1"
		 " WHEN 'varbyte' THEN -4"
		 " WHEN 'geography' THEN -4"
		 " WHEN 'intervaly2m' THEN 107 "
		 " WHEN 'intervald2s' THEN 110 "
		 " ELSE 0 END AS SMALLINT) AS SQL_DATA_TYPE,"
		 " CAST(NULL AS SMALLINT) AS SQL_DATETIME_SUB,"
		 " CASE data_type"
		 " WHEN 'int4' THEN 10"
		 " WHEN 'bit' THEN 1"
		 " WHEN 'bool' THEN 1"
		 " WHEN 'boolean' THEN 1"
		 " WHEN 'varchar' THEN character_maximum_length"
		 " WHEN 'character varying' THEN character_maximum_length"
		 " WHEN 'char' THEN character_maximum_length"
		 " WHEN 'character' THEN character_maximum_length"
		 " WHEN 'nchar' THEN character_maximum_length"
		 " WHEN 'bpchar' THEN character_maximum_length"
		 " WHEN 'nvarchar' THEN character_maximum_length"
		 " WHEN 'date' THEN 13"
		 " WHEN 'time' THEN 15"
		 " WHEN 'time without time zone' THEN 15"
		 " WHEN 'timetz' THEN 21"
		 " WHEN 'time with time zone' THEN 21"
		 " WHEN 'timestamp' THEN 29"
		 " WHEN 'timestamp without time zone' THEN 29"
		 " WHEN 'timestamptz' THEN 35"
		 " WHEN 'timestamp with time zone' THEN 35"
		 " WHEN 'smallint' THEN 5"
		 " WHEN 'int2' THEN 5"
		 " WHEN 'integer' THEN 10"
		 " WHEN 'int' THEN 10"
		 " WHEN 'int4' THEN 10"
		 " WHEN 'bigint' THEN 19"
		 " WHEN 'int8' THEN 19"
		 " WHEN 'decimal' THEN numeric_precision"
		 " WHEN 'real' THEN 8"
		 " WHEN 'float4' THEN 8"
		 " WHEN 'double precision' THEN 17"
		 " WHEN 'float8' THEN 17"
		 " WHEN 'float' THEN 17"
		 " WHEN 'numeric' THEN numeric_precision"
		 " WHEN '_float4' THEN 8"
		 " WHEN 'oid' THEN 10"
		 " WHEN '_int4' THEN 10"
		 " WHEN '_int2' THEN 5"
		 " WHEN 'geometry' THEN NULL"
		 " WHEN 'super' THEN NULL"
		 " WHEN 'varbyte' THEN NULL"
		 " WHEN 'geography' THEN NULL"
		 " WHEN 'intervaly2m' THEN 32"
		 " WHEN 'intervald2s' THEN 64"
		 " ELSE 2147483647 " 
		 " END AS CHAR_OCTET_LENGTH,"
		 " ordinal_position AS ORDINAL_POSITION,"
		 " is_nullable AS IS_NULLABLE "
		 " FROM svv_columns");

	result.append(" WHERE true ");

	result.append(catalogFilter);

	filterClause[0] = '\0';
	addLikeOrEqualFilterCondition(pStmt, filterClause, "table_schema", (char *)pSchemaName, cbSchemaName);
	addLikeOrEqualFilterCondition(pStmt, filterClause, "table_name", (char *)pTableName, cbTableName);
	addLikeOrEqualFilterCondition(pStmt, filterClause, "COLUMN_NAME", (char *)pColumnName, cbColumnName);

	result.append(filterClause);
	result.append(" ORDER BY table_schem,table_name,ORDINAL_POSITION ");

	rs_strncpy(pszCatalogQuery, result.c_str(), MAX_CATALOG_QUERY_LEN);
}

/*====================================================================================================================================================*/

static void buildUniversalAllSchemaColumnsQuery(char *pszCatalogQuery,
	RS_STMT_INFO *pStmt,
	SQLCHAR *pCatalogName,
	SQLSMALLINT cbCatalogName,
	SQLCHAR *pSchemaName,
	SQLSMALLINT cbSchemaName,
	SQLCHAR *pTableName,
	SQLSMALLINT cbTableName,
	SQLCHAR *pColumnName,
	SQLSMALLINT cbColumnName)
{
	std::string result = "";
	char catalogFilter[MAX_LARGE_TEMP_BUF_LEN];
	char filterClause[MAX_CATALOG_QUERY_FILTER_LEN];
	//	std::string unknownColumnSize = "2147483647";

	getCatalogFilterCondition(catalogFilter, MAX_LARGE_TEMP_BUF_LEN, pStmt, (char *)pCatalogName, cbCatalogName,
								FALSE, NULL);

	result.append("SELECT database_name AS TABLE_CAT, "
		 " schema_name AS TABLE_SCHEM, "
		 " table_name, "
		 " COLUMN_NAME, "
		 " CAST(CASE regexp_replace(data_type, '^_.', 'ARRAY') "
		 " WHEN 'text' THEN 12 "
		 " WHEN 'bit' THEN -7 "
		 " WHEN 'bool' THEN -7 "
		 " WHEN 'boolean' THEN -7 "
		 " WHEN 'varchar' THEN 12 "
		 " WHEN 'character varying' THEN 12 "
		 " WHEN 'char' THEN 1 "
		 " WHEN 'character' THEN 1 "
		 " WHEN 'nchar' THEN 1 "
		 " WHEN 'bpchar' THEN 1 "
		 " WHEN 'nvarchar' THEN 12 "
		 " WHEN '\"char\"' THEN 1 "
		 " WHEN 'date' THEN 91 "
		 " WHEN 'time' THEN 92 "
		 " WHEN 'time without time zone' THEN 92 "
		 " WHEN 'timetz' THEN 2013 "
		 " WHEN 'time with time zone' THEN 2013 "
		 " WHEN 'timestamp' THEN 93 "
		 " WHEN 'timestamp without time zone' THEN 93 "
		 " WHEN 'timestamptz' THEN 2014 "
		 " WHEN 'timestamp with time zone' THEN 2014 "
		 " WHEN 'smallint' THEN 5 "
		 " WHEN 'int2' THEN 5 "
		 " WHEN 'integer' THEN 4 "
		 " WHEN 'int' THEN 4 "
		 " WHEN 'int4' THEN 4 "
		 " WHEN 'bigint' THEN -5 "
		 " WHEN 'int8' THEN -5 "
		 " WHEN 'decimal' THEN 3 "
		 " WHEN 'real' THEN 7 "
		 " WHEN 'float4' THEN 7 "
		 " WHEN 'double precision' THEN 8 "
		 " WHEN 'float8' THEN 8 "
		 " WHEN 'float' THEN 6 "
		 " WHEN 'numeric' THEN 2 "
		 " WHEN 'bytea' THEN -2 "
		 " WHEN 'oid' THEN -5 "
		 " WHEN 'name' THEN 12 "
		 " WHEN 'ARRAY' THEN 2003 "
		 " WHEN 'geometry' THEN -4 "
		 " WHEN 'super' THEN -1 "
		 " WHEN 'varbyte' THEN -4 "
		 " WHEN 'geography' THEN -4 "
		 " WHEN 'intervaly2m' THEN 107 "
		 " WHEN 'intervald2s' THEN 110 "
		 " ELSE 0 END AS SMALLINT) AS DATA_TYPE, "
		 " CASE data_type "
		 " WHEN 'boolean' THEN 'bool' "
		 " WHEN 'character varying' THEN 'varchar' "
		 " WHEN '\"char\"' THEN 'char' "
		 " WHEN 'smallint' THEN 'int2' "
		 " WHEN 'integer' THEN 'int4' "
		 " WHEN 'bigint' THEN 'int8' "
		 " WHEN 'real' THEN 'float4' "
		 " WHEN 'double precision' THEN 'float8' "
		 " WHEN 'time without time zone' THEN 'time' "
		 " WHEN 'time with time zone' THEN 'timetz' "
		 " WHEN 'timestamp without time zone' THEN 'timestamp' "
		 " WHEN 'timestamp with time zone' THEN 'timestamptz' "
		 " ELSE data_type "
		 " END AS TYPE_NAME, "
		 " CASE data_type "
		 " WHEN 'int4' THEN 10 "
		 " WHEN 'bit' THEN 1 "
		 " WHEN 'bool' THEN 1 "
		 " WHEN 'boolean' THEN 1 "
		 " WHEN 'varchar' THEN character_maximum_length "
		 " WHEN 'character varying' THEN character_maximum_length "
		 " WHEN 'char' THEN character_maximum_length "
		 " WHEN 'character' THEN character_maximum_length "
		 " WHEN 'nchar' THEN character_maximum_length "
		 " WHEN 'bpchar' THEN character_maximum_length "
		 " WHEN 'nvarchar' THEN character_maximum_length "
		 " WHEN 'date' THEN 13 "
		 " WHEN 'time' THEN 15 "
		 " WHEN 'time without time zone' THEN 15 "
		 " WHEN 'timetz' THEN 21 "
		 " WHEN 'time with time zone' THEN 21 "
		 " WHEN 'timestamp' THEN 29 "
		 " WHEN 'timestamp without time zone' THEN 29 "
		 " WHEN 'timestamptz' THEN 35 "
		 " WHEN 'timestamp with time zone' THEN 35 "
		 " WHEN 'smallint' THEN 5 "
		 " WHEN 'int2' THEN 5 "
		 " WHEN 'integer' THEN 10 "
		 " WHEN 'int' THEN 10 "
		 " WHEN 'int4' THEN 10 "
		 " WHEN 'bigint' THEN 19 "
		 " WHEN 'int8' THEN 19 "
		 " WHEN 'decimal' THEN numeric_precision "
		 " WHEN 'real' THEN 8 "
		 " WHEN 'float4' THEN 8 "
		 " WHEN 'double precision' THEN 17 "
		 " WHEN 'float8' THEN 17 "
		 " WHEN 'float' THEN 17 "
		 " WHEN 'numeric' THEN numeric_precision "
		 " WHEN '_float4' THEN 8 "
		 " WHEN 'oid' THEN 10 "
		 " WHEN '_int4' THEN 10 "
		 " WHEN '_int2' THEN 5 "
		 " WHEN 'geometry' THEN NULL "
		 " WHEN 'super' THEN NULL "
		 " WHEN 'varbyte' THEN NULL "
		 " WHEN 'geography' THEN NULL "
		 " WHEN 'intervaly2m' THEN 32 "
		 " WHEN 'intervald2s' THEN 64 "
		 " ELSE   2147483647 "
		 " END AS COLUMN_SIZE, "
		 " NULL AS BUFFER_LENGTH, "
		 " CASE data_type "
		 " WHEN 'real' THEN 8 "
		 " WHEN 'float4' THEN 8 "
		 " WHEN 'double precision' THEN 17 "
		 " WHEN 'float8' THEN 17 "
		 " WHEN 'numeric' THEN numeric_scale "
		 " WHEN 'time' THEN 6 "
		 " WHEN 'time without time zone' THEN 6 "
		 " WHEN 'timetz' THEN 6 "
		 " WHEN 'time with time zone' THEN 6 "
		 " WHEN 'timestamp' THEN 6 "
		 " WHEN 'timestamp without time zone' THEN 6 "
		 " WHEN 'timestamptz' THEN 6 "
		 " WHEN 'timestamp with time zone' THEN 6 "
		 " WHEN 'geometry' THEN NULL "
		 " WHEN 'super' THEN NULL "
		 " WHEN 'varbyte' THEN NULL "
		 " WHEN 'geography' THEN NULL "
		 " WHEN 'intervaly2m' THEN 0 "
		 " WHEN 'intervald2s' THEN 6 "
		 " ELSE 0 "
		 " END AS DECIMAL_DIGITS, "
		 " CASE data_type "
		 " WHEN 'varbyte' THEN 2 "
		 " WHEN 'geography' THEN 2 "
		 " when 'varchar' THEN 0 "
		 " when 'character varying' THEN 0 "
		 " when 'char' THEN 0 "
		 " when 'character' THEN 0 "
		 " when 'nchar' THEN 0 "
		 " when 'bpchar' THEN 0 "
		 " when 'nvarchar' THEN 0 "
		 " ELSE 10 "
		 " END AS NUM_PREC_RADIX, "
		 " CASE is_nullable WHEN 'YES' THEN 1 "
		 " WHEN 'NO' THEN 0 "
		 " ELSE 2 end AS NULLABLE, "
		 " REMARKS, "
		 " column_default AS COLUMN_DEF, "
		 " CAST(CASE regexp_replace(data_type, '^_.', 'ARRAY') "
		 " WHEN 'text' THEN 12 "
		 " WHEN 'bit' THEN -7 "
		 " WHEN 'bool' THEN -7 "
		 " WHEN 'boolean' THEN -7 "
		 " WHEN 'varchar' THEN 12 "
		 " WHEN 'character varying' THEN 12 "
		 " WHEN 'char' THEN 1 "
		 " WHEN 'character' THEN 1 "
		 " WHEN 'nchar' THEN 1 "
		 " WHEN 'bpchar' THEN 1 "
		 " WHEN 'nvarchar' THEN 12 "
		 " WHEN '\"char\"' THEN 1 "
		 " WHEN 'date' THEN 91 "
		 " WHEN 'time' THEN 92 "
		 " WHEN 'time without time zone' THEN 92 "
		 " WHEN 'timetz' THEN 2013 "
		 " WHEN 'time with time zone' THEN 2013 "
		 " WHEN 'timestamp' THEN 93 "
		 " WHEN 'timestamp without time zone' THEN 93 "
		 " WHEN 'timestamptz' THEN 2014 "
		 " WHEN 'timestamp with time zone' THEN 2014 "
		 " WHEN 'smallint' THEN 5 "
		 " WHEN 'int2' THEN 5 "
		 " WHEN 'integer' THEN 4 "
		 " WHEN 'int' THEN 4 "
		 " WHEN 'int4' THEN 4 "
		 " WHEN 'bigint' THEN -5 "
		 " WHEN 'int8' THEN -5 "
		 " WHEN 'decimal' THEN 3 "
		 " WHEN 'real' THEN 7 "
		 " WHEN 'float4' THEN 7 "
		 " WHEN 'double precision' THEN 8 "
		 " WHEN 'float8' THEN 8 "
		 " WHEN 'float' THEN 6 "
		 " WHEN 'numeric' THEN 2 "
		 " WHEN 'bytea' THEN -2 "
		 " WHEN 'oid' THEN -5 "
		 " WHEN 'name' THEN 12 "
		 " WHEN 'ARRAY' THEN 2003 "
		 " WHEN 'geometry' THEN -4 "
		 " WHEN 'super' THEN -1 "
		 " WHEN 'varbyte' THEN -4 "
		 " WHEN 'geography' THEN -4 "
		 " WHEN 'intervaly2m' THEN 107 "
		 " WHEN 'intervald2s' THEN 110 "
		 " ELSE 0 END AS SMALLINT) AS SQL_DATA_TYPE, "
		 " CAST(NULL AS SMALLINT) AS SQL_DATETIME_SUB, "
		 " CASE data_type "
		 " WHEN 'int4' THEN 10 "
		 " WHEN 'bit' THEN 1 "
		 " WHEN 'bool' THEN 1 "
		 " WHEN 'boolean' THEN 1 "
		 " WHEN 'varchar' THEN character_maximum_length "
		 " WHEN 'character varying' THEN character_maximum_length "
		 " WHEN 'char' THEN character_maximum_length "
		 " WHEN 'character' THEN character_maximum_length "
		 " WHEN 'nchar' THEN character_maximum_length "
		 " WHEN 'bpchar' THEN character_maximum_length "
		 " WHEN 'nvarchar' THEN character_maximum_length "
		 " WHEN 'date' THEN 13 "
		 " WHEN 'time' THEN 15 "
		 " WHEN 'time without time zone' THEN 15 "
		 " WHEN 'timetz' THEN 21 "
		 " WHEN 'time with time zone' THEN 21 "
		 " WHEN 'timestamp' THEN 29 "
		 " WHEN 'timestamp without time zone' THEN 29 "
		 " WHEN 'timestamptz' THEN 35 "
		 " WHEN 'timestamp with time zone' THEN 35 "
		 " WHEN 'smallint' THEN 5 "
		 " WHEN 'int2' THEN 5 "
		 " WHEN 'integer' THEN 10 "
		 " WHEN 'int' THEN 10 "
		 " WHEN 'int4' THEN 10 "
		 " WHEN 'bigint' THEN 19 "
		 " WHEN 'int8' THEN 19 "
		 " WHEN 'decimal' THEN numeric_precision "
		 " WHEN 'real' THEN 8 "
		 " WHEN 'float4' THEN 8 "
		 " WHEN 'double precision' THEN 17 "
		 " WHEN 'float8' THEN 17 "
		 " WHEN 'float' THEN 17 "
		 " WHEN 'numeric' THEN numeric_precision "
		 " WHEN '_float4' THEN 8 "
		 " WHEN 'oid' THEN 10 "
		 " WHEN '_int4' THEN 10 "
		 " WHEN '_int2' THEN 5 "
		 " WHEN 'geometry' THEN NULL "
		 " WHEN 'super' THEN NULL "
		 " WHEN 'varbyte' THEN NULL "
		 " WHEN 'geography' THEN NULL "
		 " WHEN 'intervaly2m' THEN 32 "
		 " WHEN 'intervald2s' THEN 64 "
		 " ELSE   2147483647 "
		 " END AS CHAR_OCTET_LENGTH, "
		 " ordinal_position AS ORDINAL_POSITION, "
		 " is_nullable AS IS_NULLABLE "
		 " FROM PG_CATALOG.svv_all_columns ");

	result.append(" WHERE true ");

	result.append(catalogFilter);

	filterClause[0] = '\0';
	addLikeOrEqualFilterCondition(pStmt, filterClause, "schema_name", (char *)pSchemaName, cbSchemaName);
	addLikeOrEqualFilterCondition(pStmt, filterClause, "table_name", (char *)pTableName, cbTableName);
	addLikeOrEqualFilterCondition(pStmt, filterClause, "COLUMN_NAME", (char *)pColumnName, cbColumnName);

	result.append(filterClause);
	result.append(" ORDER BY TABLE_CAT, TABLE_SCHEM, TABLE_NAME, ORDINAL_POSITION ");

	rs_strncpy(pszCatalogQuery, result.c_str(), MAX_CATALOG_QUERY_LEN);
}

/*====================================================================================================================================================*/

static void buildExternalSchemaColumnsQuery(char *pszCatalogQuery,
	RS_STMT_INFO *pStmt,
	SQLCHAR *pCatalogName,
	SQLSMALLINT cbCatalogName,
	SQLCHAR *pSchemaName,
	SQLSMALLINT cbSchemaName,
	SQLCHAR *pTableName,
	SQLSMALLINT cbTableName,
	SQLCHAR *pColumnName,
	SQLSMALLINT cbColumnName)
{
	std::string result = "";
	char catalogFilter[MAX_LARGE_TEMP_BUF_LEN];
	char filterClause[MAX_CATALOG_QUERY_FILTER_LEN];
	//	std::string unknownColumnSize = "2147483647";

	getCatalogFilterCondition(catalogFilter, MAX_LARGE_TEMP_BUF_LEN, pStmt, (char *)pCatalogName, cbCatalogName,
								TRUE, NULL);

	// NOTE: Explicit cast on current_database() prevents bug where data returned from server
	// has incorrect length and displays random characters. [JDBC-529]
	result.append("SELECT current_database()::varchar(128) AS TABLE_CAT,"
		 " schemaname AS TABLE_SCHEM,"
		 " tablename AS TABLE_NAME,"
		 " columnname AS COLUMN_NAME,"
		 " CAST(CASE WHEN external_type = 'text' THEN 12"
		 " WHEN external_type = 'bit' THEN -7"
		 " WHEN external_type = 'bool' THEN -7"
		 " WHEN external_type = 'boolean' THEN -7"
		 " WHEN left(external_type, 7) = 'varchar' THEN 12"
		 " WHEN left(external_type, 17) = 'character varying' THEN 12"
		 " WHEN left(external_type, 4) = 'char' THEN 1"
		 " WHEN left(external_type, 9) = 'character' THEN 1"
		 " WHEN left(external_type, 5) = 'nchar' THEN 1"
		 " WHEN left(external_type, 6) = 'bpchar' THEN 1"
		 " WHEN left(external_type, 8) = 'nvarchar' THEN 12"
		 " WHEN external_type = '\"char\"' THEN 1"
		 " WHEN external_type = 'date' THEN 91"
		 " WHEN external_type = 'time' THEN 92 "
		 " WHEN external_type = 'time without time zone' THEN 92 "
		 " WHEN external_type = 'timetz' THEN 2013 "
		 " WHEN external_type = 'time with time zone' THEN 2013 "
		 " WHEN external_type = 'timestamp' THEN 93"
		 " WHEN external_type = 'timestamp without time zone' THEN 93"
		 " WHEN external_type = 'timestamptz' THEN 2014"
		 " WHEN external_type = 'timestamp with time zone' THEN 2014"
		 " WHEN external_type = 'smallint' THEN 5"
		 " WHEN external_type = 'int2' THEN 5"
		 " WHEN external_type = '_int2' THEN 5"
		 " WHEN external_type = 'integer' THEN 4"
		 " WHEN external_type = 'int' THEN 4"
		 " WHEN external_type = 'int4' THEN 4"
		 " WHEN external_type = '_int4' THEN 4"
		 " WHEN external_type = 'bigint' THEN -5"
		 " WHEN external_type = 'int8' THEN -5"
		 " WHEN left(external_type, 7) = 'decimal' THEN 2"
		 " WHEN external_type = 'real' THEN 7"
		 " WHEN external_type = 'float4' THEN 7"
		 " WHEN external_type = '_float4' THEN 7"
		 " WHEN external_type = 'double' THEN 8"
		 " WHEN external_type = 'double precision' THEN 8"
		 " WHEN external_type = 'float8' THEN 8"
		 " WHEN external_type = '_float8' THEN 8"
		 " WHEN external_type = 'float' THEN 6"
		 " WHEN left(external_type, 7) = 'numeric' THEN 2"
		 " WHEN external_type = 'bytea' THEN -2"
		 " WHEN external_type = 'oid' THEN -5"
		 " WHEN external_type = 'name' THEN 12"
		 " WHEN external_type = 'ARRAY' THEN 2003"
		 " WHEN external_type = 'geometry' THEN -4"
		 " WHEN external_type = 'super' THEN -1"
		 " WHEN external_type = 'varbyte' THEN -4"
		 " WHEN external_type = 'geography' THEN -4"
		 " WHEN external_type = 'intervaly2m' THEN 107"
		 " WHEN external_type = 'intervald2s' THEN 110"
		 " ELSE 0 END AS SMALLINT) AS DATA_TYPE,"
		 " CASE WHEN left(external_type, 17) = 'character varying' THEN 'varchar'"
		 " WHEN left(external_type, 7) = 'varchar' THEN 'varchar'"
		 " WHEN left(external_type, 4) = 'char' THEN 'char'"
		 " WHEN left(external_type, 7) = 'decimal' THEN 'numeric'"
		 " WHEN left(external_type, 7) = 'numeric' THEN 'numeric'"
		 " WHEN external_type = 'double' THEN 'double precision'"
		 " WHEN external_type = 'time without time zone' THEN 'time'"
		 " WHEN external_type = 'time with time zone' THEN 'timetz'"
		 " WHEN external_type = 'timestamp without time zone' THEN 'timestamp'"
		 " WHEN external_type = 'timestamp with time zone' THEN 'timestamptz'"
		 " ELSE external_type END AS TYPE_NAME,"
		 " CASE WHEN external_type = 'int4' THEN 10"
		 " WHEN external_type = 'bit' THEN 1"
		 " WHEN external_type = 'bool' THEN 1"
		 " WHEN external_type = 'boolean' THEN 1"
		 " WHEN left(external_type, 7) = 'varchar' "
		 "  THEN CASE "
		 "   WHEN regexp_instr(external_type, '\\\\(', 7) = 0 THEN '0' "
		 "   ELSE regexp_substr(external_type, '[0-9]+', 7) "
		 "  END::integer "
		 " WHEN left(external_type, 17) = 'character varying' "
		 "  THEN CASE "
		 "   WHEN regexp_instr(external_type, '\\\\(', 17) = 0 THEN '0' "
		 "   ELSE regexp_substr(external_type, '[0-9]+', 17) "
		 "  END::integer "
		 " WHEN left(external_type, 4) = 'char' "
		 "  THEN CASE "
		 "   WHEN regexp_instr(external_type, '\\\\(', 4) = 0 THEN '0' "
		 "   ELSE regexp_substr(external_type, '[0-9]+', 4) "
		 "  END::integer "
		 " WHEN left(external_type, 9) = 'character' "
		 "	 THEN CASE "
		 "   WHEN regexp_instr(external_type, '\\\\(', 9) = 0 THEN '0' "
		 "   ELSE regexp_substr(external_type, '[0-9]+', 9) "
		 "  END::integer "
		 " WHEN left(external_type, 5) = 'nchar' "
		 "  THEN CASE "
		 "   WHEN regexp_instr(external_type, '\\\\(', 5) = 0 THEN '0' "
		 "   ELSE regexp_substr(external_type, '[0-9]+', 5) "
		 "  END::integer "
		 " WHEN left(external_type, 6) = 'bpchar' "
		 "	 THEN CASE "
		 "   WHEN regexp_instr(external_type, '\\\\(', 6) = 0 THEN '0' "
		 "   ELSE regexp_substr(external_type, '[0-9]+', 6) "
		 "  END::integer "
		 " WHEN left(external_type, 8) = 'nvarchar' "
		 "  THEN CASE "
		 "    WHEN regexp_instr(external_type, '\\\\(', 8) = 0 THEN '0' "
		 "    ELSE regexp_substr(external_type, '[0-9]+', 8) "
		 "  END::integer "
		 " WHEN external_type = 'date' THEN 13 "
		 " WHEN external_type = 'time' THEN 15 "
		 " WHEN external_type = 'time without time zone' THEN 15 "
		 " WHEN external_type = 'timetz' THEN 21 "
		 " WHEN external_type = 'time with time zone' THEN 21 "
		 " WHEN external_type = 'timestamp' THEN 29 "
		 " WHEN external_type = 'timestamp without time zone' THEN 29"
		 " WHEN external_type = 'timestamptz' THEN 35"
		 " WHEN external_type = 'timestamp with time zone' THEN 35"
		 " WHEN external_type = 'smallint' THEN 5"
		 " WHEN external_type = 'int2' THEN 5"
		 " WHEN external_type = 'integer' THEN 10"
		 " WHEN external_type = 'int' THEN 10"
		 " WHEN external_type = 'int4' THEN 10"
		 " WHEN external_type = 'bigint' THEN 19"
		 " WHEN external_type = 'int8' THEN 19"
		 " WHEN left(external_type, 7) = 'decimal' THEN isnull(nullif(regexp_substr(external_type, '[0-9]+', 7),''),'0')::integer"
		 " WHEN external_type = 'real' THEN 8"
		 " WHEN external_type = 'float4' THEN 8"
		 " WHEN external_type = '_float4' THEN 8"
		 " WHEN external_type = 'double' THEN 17"
		 " WHEN external_type = 'double precision' THEN 17"
		 " WHEN external_type = 'float8' THEN 17"
		 " WHEN external_type = '_float8' THEN 17"
		 " WHEN external_type = 'float' THEN 17"
		 " WHEN left(external_type, 7) = 'numeric' THEN isnull(nullif(regexp_substr(external_type, '[0-9]+', 7),''),'0')::integer"
		 " WHEN external_type = '_float4' THEN 8"
		 " WHEN external_type = 'oid' THEN 10"
		 " WHEN external_type = '_int4' THEN 10"
		 " WHEN external_type = '_int2' THEN 5"
		 " WHEN external_type = 'geometry' THEN NULL"
		 " WHEN external_type = 'super' THEN NULL"
		 " WHEN external_type = 'varbyte' THEN NULL"
		 " WHEN external_type = 'geography' THEN NULL"
		 " WHEN external_type = 'intervaly2m' THEN 32"
		 " WHEN external_type = 'intervald2s' THEN 64"
		 " ELSE 2147483647 END AS COLUMN_SIZE,"
		 " NULL AS BUFFER_LENGTH,"
		 " CASE WHEN external_type = 'real'THEN 8"
		 " WHEN external_type = 'float4' THEN 8"
		 " WHEN external_type = 'double' THEN 17"
		 " WHEN external_type = 'double precision' THEN 17"
		 " WHEN external_type = 'float8' THEN 17"
		 " WHEN left(external_type, 7) = 'numeric' THEN isnull(nullif(regexp_substr(external_type, '[0-9]+', 10),''),'0')::integer"
		 " WHEN left(external_type, 7) = 'decimal' THEN isnull(nullif(regexp_substr(external_type, '[0-9]+', 10),''),'0')::integer"
		 " WHEN external_type = 'time' THEN 6 "
		 " WHEN external_type = 'time without time zone' THEN 6 "
		 " WHEN external_type = 'timetz' THEN 6 "
		 " WHEN external_type = 'time with time zone' THEN 6 "
		 " WHEN external_type = 'timestamp' THEN 6"
		 " WHEN external_type = 'timestamp without time zone' THEN 6"
		 " WHEN external_type = 'timestamptz' THEN 6"
		 " WHEN external_type = 'timestamp with time zone' THEN 6"
		 " WHEN external_type = 'geometry' THEN NULL"
		 " WHEN external_type = 'super' THEN NULL"
		 " WHEN external_type = 'varbyte' THEN NULL"
		 " WHEN external_type = 'geography' THEN NULL"
		 " WHEN external_type = 'intervaly2m' THEN 0"
		 " WHEN external_type = 'intervald2s' THEN 6"
		 " ELSE 0 END AS DECIMAL_DIGITS,"
		 " CASE WHEN external_type = 'varbyte' THEN 2"
		 " WHEN external_type = 'geography' THEN 2"
		 " when external_type = 'varchar' THEN 0 "
		 " when external_type = 'character varying' THEN 0 "
		 " when external_type = 'char' THEN 0 "
		 " when external_type = 'character' THEN 0 "
		 " when external_type = 'nchar' THEN 0 "
		 " when external_type = 'bpchar' THEN 0 "
		 " when external_type = 'nvarchar' THEN 0 "
		 " ELSE 10"
		 " END AS NUM_PREC_RADIX,"
		 " CAST(CASE is_nullable WHEN 'true' THEN 1 WHEN 'false' THEN 0 ELSE NULL END AS SMALLINT) AS NULLABLE,"
		 " NULL AS REMARKS,"
		 " NULL AS COLUMN_DEF,"
		 " CAST(CASE WHEN external_type = 'text' THEN 12"
		 " WHEN external_type = 'bit' THEN -7"
		 " WHEN external_type = 'bool' THEN -7"
		 " WHEN external_type = 'boolean' THEN -7"
		 " WHEN left(external_type, 7) = 'varchar' THEN 12"
		 " WHEN left(external_type, 17) = 'character varying' THEN 12"
		 " WHEN left(external_type, 4) = 'char' THEN 1"
		 " WHEN left(external_type, 9) = 'character' THEN 1"
		 " WHEN left(external_type, 5) = 'nchar' THEN 1"
		 " WHEN left(external_type, 6) = 'bpchar' THEN 1"
		 " WHEN left(external_type, 8) = 'nvarchar' THEN 12"
		 " WHEN external_type = '\"char\"' THEN 1"
		 " WHEN external_type = 'date' THEN 91"
		 " WHEN external_type = 'time' THEN 92 "
		 " WHEN external_type = 'time without time zone' THEN 92 "
		 " WHEN external_type = 'timetz' THEN 2013 "
		 " WHEN external_type = 'time with time zone' THEN 2013 "
		 " WHEN external_type = 'timestamp' THEN 93"
		 " WHEN external_type = 'timestamp without time zone' THEN 93"
		 " WHEN external_type = 'timestamptz' THEN 2014"
		 " WHEN external_type = 'timestamp with time zone' THEN 2014"
		 " WHEN external_type = 'smallint' THEN 5"
		 " WHEN external_type = 'int2' THEN 5"
		 " WHEN external_type = '_int2' THEN 5"
		 " WHEN external_type = 'integer' THEN 4"
		 " WHEN external_type = 'int' THEN 4"
		 " WHEN external_type = 'int4' THEN 4"
		 " WHEN external_type = '_int4' THEN 4"
		 " WHEN external_type = 'bigint' THEN -5"
		 " WHEN external_type = 'int8' THEN -5"
		 " WHEN left(external_type, 7) = 'decimal' THEN 3"
		 " WHEN external_type = 'real' THEN 7"
		 " WHEN external_type = 'float4' THEN 7"
		 " WHEN external_type = '_float4' THEN 7"
		 " WHEN external_type = 'double' THEN 8"
		 " WHEN external_type = 'double precision' THEN 8"
		 " WHEN external_type = 'float8' THEN 8"
		 " WHEN external_type = '_float8' THEN 8"
		 " WHEN external_type = 'float' THEN 6"
		 " WHEN left(external_type, 7) = 'numeric' THEN 2"
		 " WHEN external_type = 'bytea' THEN -2"
		 " WHEN external_type = 'oid' THEN -5"
		 " WHEN external_type = 'name' THEN 12"
		 " WHEN external_type = 'ARRAY' THEN 2003"
		 " WHEN external_type = 'geometry' THEN -4"
		 " WHEN external_type = 'super' THEN -1"
		 " WHEN external_type = 'varbyte' THEN -4"
		 " WHEN external_type = 'geography' THEN -4"
		 " WHEN external_type = 'intervaly2m' THEN 107"
		 " WHEN external_type = 'intervald2s' THEN 110"
		 " ELSE 0 END AS SMALLINT) AS SQL_DATA_TYPE,"
		 " CAST(NULL AS SMALLINT) AS SQL_DATETIME_SUB,"
		 " CASE WHEN left(external_type, 7) = 'varchar' "
		 "  THEN CASE "
		 "   WHEN regexp_instr(external_type, '\\\\(', 7) = 0 THEN '0' "
		 "   ELSE regexp_substr(external_type, '[0-9]+', 7) "
		 "  END::integer "
		 " WHEN left(external_type, 17) = 'character varying' "
		 "  THEN CASE "
		 "   WHEN regexp_instr(external_type, '\\\\(', 17) = 0 THEN '0' "
		 "   ELSE regexp_substr(external_type, '[0-9]+', 17) "
		 "  END::integer "
		 " WHEN left(external_type, 4) = 'char' "
		 "  THEN CASE "
		 "   WHEN regexp_instr(external_type, '\\\\(', 4) = 0 THEN '0' "
		 "   ELSE regexp_substr(external_type, '[0-9]+', 4) "
		 "  END::integer "
		 " WHEN left(external_type, 9) = 'character' "
		 "	 THEN CASE "
		 "   WHEN regexp_instr(external_type, '\\\\(', 9) = 0 THEN '0' "
		 "   ELSE regexp_substr(external_type, '[0-9]+', 9) "
		 "  END::integer "
		 " WHEN left(external_type, 5) = 'nchar' "
		 "  THEN CASE "
		 "   WHEN regexp_instr(external_type, '\\\\(', 5) = 0 THEN '0' "
		 "   ELSE regexp_substr(external_type, '[0-9]+', 5) "
		 "  END::integer "
		 " WHEN left(external_type, 6) = 'bpchar' "
		 "	 THEN CASE "
		 "   WHEN regexp_instr(external_type, '\\\\(', 6) = 0 THEN '0' "
		 "   ELSE regexp_substr(external_type, '[0-9]+', 6) "
		 "  END::integer "
		 " WHEN left(external_type, 8) = 'nvarchar' "
		 "  THEN CASE "
		 "    WHEN regexp_instr(external_type, '\\\\(', 8) = 0 THEN '0' "
		 "    ELSE regexp_substr(external_type, '[0-9]+', 8) "
		 "  END::integer "
		 " WHEN external_type = 'string' THEN 16383"
		 " ELSE NULL END AS CHAR_OCTET_LENGTH,"
		 " columnnum AS ORDINAL_POSITION,"
         " CASE IS_NULLABLE when 'true' THEN 'YES' when 'false' then 'NO' ELSE NULL END AS IS_NULLABLE"
		 " FROM svv_external_columns");

	result.append(" WHERE true ");

	result.append(catalogFilter);

	filterClause[0] = '\0';
	addLikeOrEqualFilterCondition(pStmt, filterClause, "schemaname", (char *)pSchemaName, cbSchemaName);
	addLikeOrEqualFilterCondition(pStmt, filterClause, "tablename", (char *)pTableName, cbTableName);
	addLikeOrEqualFilterCondition(pStmt, filterClause, "columnname", (char *)pColumnName, cbColumnName);

	result.append(filterClause);
	result.append(" ORDER BY table_schem,table_name,ORDINAL_POSITION ");

	rs_strncpy(pszCatalogQuery, result.c_str(), MAX_CATALOG_QUERY_LEN);
}

/*====================================================================================================================================================*/

char *escapedFilterCondition(const char *pName, short cbName)
{
	char *pEscapedName = (char *)rs_calloc(cbName * 2, sizeof(char));
	const char *pSrc = pName;
	char *pDest = pEscapedName;

	while (*pSrc)
	{
		if (*pSrc == '\'')
		{
			*pDest = '\'';
			pDest++;
		}

		*pDest = *pSrc;
		pSrc++;
		pDest++;
	}

	return pEscapedName;
}
