/*
 *      Copyright (C) 2000,2002 Nikos Mavroyanopoulos
 *
 * This file is part of GNUTLS.
 *
 * GNUTLS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * GNUTLS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

/* This file contains functions that manipulate a database
 * for resumed sessions. 
 */
#include "gnutls_int.h"
#include "gnutls_errors.h"
#include "gnutls_session.h"
#include <gnutls_db.h>
#include "debug.h"
#include <gnutls_session_pack.h>

#define GNUTLS_DBNAME state->gnutls_internals.db_name

#ifdef HAVE_LIBGDBM
# define GNUTLS_DBF state->gnutls_internals.db_reader
# define GNUTLS_REOPEN_DB() if (GNUTLS_DBF!=NULL) \
	gdbm_close( GNUTLS_DBF); \
	GNUTLS_DBF = gdbm_open(GNUTLS_DBNAME, 0, GDBM_READER, 0600, NULL);
#endif

/**
  * gnutls_db_set_retrieve_function - Sets the function that will be used to get data
  * @state: is a &GNUTLS_STATE structure.
  * @retr_func: is the function.
  *
  * Sets the function that will be used to retrieve data from the resumed
  * sessions database. This function must return a gnutls_datum containing the
  * data on success, or a gnutls_datum containing null and 0 on failure.
  * This function should only be used if you do
  * not plan to use the included gdbm backend.
  *
  * The first argument to store_func() will be null unless gnutls_db_set_ptr() 
  * has been called.
  *
  **/
void gnutls_db_set_retrieve_function( GNUTLS_STATE state, GNUTLS_DB_RETR_FUNC retr_func) {
	state->gnutls_internals.db_retrieve_func = retr_func;
}

/**
  * gnutls_db_set_remove_function - Sets the function that will be used to remove data
  * @state: is a &GNUTLS_STATE structure.
  * @rem_func: is the function.
  *
  * Sets the function that will be used to remove data from the resumed
  * sessions database. This function must return 0 on success.
  * This function should only be used if you do
  * not plan to use the included gdbm backend.
  *
  * The first argument to rem_func() will be null unless gnutls_db_set_ptr() 
  * has been called.
  *
  **/
void gnutls_db_set_remove_function( GNUTLS_STATE state, GNUTLS_DB_REMOVE_FUNC rem_func) {
	state->gnutls_internals.db_remove_func = rem_func;
}

/**
  * gnutls_db_set_store_function - Sets the function that will be used to put data
  * @state: is a &GNUTLS_STATE structure.
  * @store_func: is the function
  *
  * Sets the function that will be used to store data from the resumed
  * sessions database. This function must remove 0 on success. 
  * This function should only be used if you do
  * not plan to use the included gdbm backend.
  *
  * The first argument to store_func() will be null unless gnutls_db_set_ptr() 
  * has been called.
  *
  **/
void gnutls_db_set_store_function( GNUTLS_STATE state, GNUTLS_DB_STORE_FUNC store_func) {
	state->gnutls_internals.db_store_func = store_func;
}

/**
  * gnutls_db_set_ptr - Sets a pointer to be sent to db functions
  * @state: is a &GNUTLS_STATE structure.
  * @ptr: is the pointer
  *
  * Sets the pointer that will be sent to db store, retrieve and delete functions, as
  * the first argument. Should only be called if not using the gdbm backend.
  *
  **/
void gnutls_db_set_ptr( GNUTLS_STATE state, void* ptr) {
	state->gnutls_internals.db_ptr = ptr;
}

/**
  * gnutls_db_get_ptr - Returns the pointer which is sent to db functions
  * @state: is a &GNUTLS_STATE structure.
  *
  * Returns the pointer that will be sent to db store, retrieve and delete functions, as
  * the first argument. Should only be used if not using the default (gdbm) backend.
  *
  **/
void* gnutls_db_get_ptr( GNUTLS_STATE state) {
	return state->gnutls_internals.db_ptr;
}

/**
  * gnutls_db_set_cache_expiration - Sets the expiration time for resumed sessions.
  * @state: is a &GNUTLS_STATE structure.
  * @seconds: is the number of seconds.
  *
  * Sets the expiration time for resumed sessions. The default is 3600 (one hour)
  * at the time writing this.
  **/
void gnutls_db_set_cache_expiration( GNUTLS_STATE state, int seconds) {
	state->gnutls_internals.expire_time = seconds;
}

/**
  * gnutls_db_set_name - Sets the name of the database that holds TLS sessions.
  * @state: is a &GNUTLS_STATE structure.
  * @filename: is the filename for the database
  *
  * Sets the name of the (gdbm) database to be used to keep
  * the sessions to be resumed. This function also creates the database
  * - if it does not exist - and opens it for reading.
  * You should not call this function if using an other backend
  * than gdbm (ie. called function gnutls_db_set_store_func() etc.)
  *
  **/
int gnutls_db_set_name( GNUTLS_STATE state, const char* filename) {
#ifdef HAVE_LIBGDBM
GDBM_FILE dbf;

	if (filename==NULL) return 0;

	/* deallocate previous name */
	if (GNUTLS_DBNAME!=NULL)
		gnutls_free(GNUTLS_DBNAME);

	/* set name */
	GNUTLS_DBNAME = gnutls_strdup(filename);
	if (GNUTLS_DBNAME==NULL) return GNUTLS_E_MEMORY_ERROR;

	/* open for reader */
	GNUTLS_DBF = gdbm_open(GNUTLS_DBNAME, 0, GDBM_READER, 0600, NULL);
	if (GNUTLS_DBF==NULL) {
		/* maybe it does not exist - so try to
		 * create it.
		 */
		dbf = gdbm_open( (char*)filename, 0, GDBM_WRCREAT, 0600, NULL);
		if (dbf==NULL) return GNUTLS_E_DB_ERROR;
		gdbm_close(dbf);

		/* try to open again */
		GNUTLS_DBF = gdbm_open(GNUTLS_DBNAME, 0, GDBM_READER, 0600, NULL);
	}
	if (GNUTLS_DBF==NULL)
		return GNUTLS_E_DB_ERROR;

	return 0;
#else
	return GNUTLS_E_UNIMPLEMENTED_FEATURE;
#endif
}

/**
  * gnutls_db_check_entry - checks if the given db entry has expired
  * @state: is a &GNUTLS_STATE structure.
  * @session_entry: is the session data (not key)
  *
  * This function should only be used if not using the gdbm backend.
  * This function returns GNUTLS_E_EXPIRED, if the database entry
  * has expired or 0 otherwise. This function is to be used when
  * you want to clear unnesessary session which occupy space in your
  * backend.
  *
  **/
int gnutls_db_check_entry( GNUTLS_STATE state, gnutls_datum session_entry) {
time_t timestamp;

	timestamp = time(0);

	if (session_entry.data != NULL)
		if ( timestamp - ((SecurityParameters*)(session_entry.data))->timestamp <= state->gnutls_internals.expire_time || ((SecurityParameters*)(session_entry.data))->timestamp > timestamp|| ((SecurityParameters*)(session_entry.data))->timestamp == 0)
			return GNUTLS_E_EXPIRED;
	
	return 0;
}

/**
  * gnutls_db_clean - removes expired and invalid sessions from the database
  * @state: is a &GNUTLS_STATE structure.
  *
  * This function Deletes all expired records in the resumed sessions' database. 
  * This database may become huge if this function is not called.
  * This function is also quite expensive. This function should only
  * be called if using the gdbm backend.
  *
  **/
int gnutls_db_clean( GNUTLS_STATE state) {
#ifdef HAVE_LIBGDBM
GDBM_FILE dbf;
int ret;
datum key;
time_t timestamp;
gnutls_datum _key;

	if (GNUTLS_DBF==NULL) return GNUTLS_E_DB_ERROR;
	if (GNUTLS_DBNAME==NULL) return GNUTLS_E_DB_ERROR;

	dbf = gdbm_open(GNUTLS_DBNAME, 0, GDBM_WRITER, 0600, NULL);
	if (dbf==NULL) return GNUTLS_E_AGAIN;
	key = gdbm_firstkey(dbf);

	timestamp = time(0);

	_key.data = key.dptr;
	_key.size = key.dsize;
	while( _key.data != NULL) {

		if ( gnutls_db_check_entry( state, _key)==GNUTLS_E_EXPIRED) {
		    /* delete expired entry */
		    gdbm_delete( dbf, key);
		}
		
		free(key.dptr);
		key = gdbm_nextkey(dbf, key);
	}
	ret = gdbm_reorganize(dbf);
	
	gdbm_close(dbf);
	GNUTLS_REOPEN_DB();
	
	if (ret!=0) return GNUTLS_E_DB_ERROR;
		
	return 0;
#else
	return GNUTLS_E_UNIMPLEMENTED_FEATURE;
#endif

}

/* The format of storing data is:
 * (forget it). Check gnutls_session_pack.c
 */
int _gnutls_server_register_current_session( GNUTLS_STATE state)
{
gnutls_datum key = { state->security_parameters.session_id, state->security_parameters.session_id_size };
gnutls_datum content;
int ret = 0;

	if (state->gnutls_internals.resumable==RESUME_FALSE) {
		gnutls_assert();
		return GNUTLS_E_INVALID_SESSION;
	}
	
	if (state->security_parameters.session_id==NULL || state->security_parameters.session_id_size==0) {
		gnutls_assert();
		return GNUTLS_E_INVALID_SESSION;
	}
	
/* allocate space for data */
	content.size = _gnutls_session_size( state);
	if (content.size < 0) {
		gnutls_assert();
		return content.size;
	}

	content.data = gnutls_malloc( content.size);
	if (content.data==NULL) {
		gnutls_assert();
		return GNUTLS_E_MEMORY_ERROR;
	}
	
/* copy data */
	ret = _gnutls_session_pack( state, &content);
	if (ret < 0) {
		gnutls_free( content.data);
		gnutls_assert();
		return ret;
	}

	ret = _gnutls_store_session( state, key, content);

	gnutls_free( content.data);

	return ret;
}

int _gnutls_server_restore_session( GNUTLS_STATE state, uint8* session_id, int session_id_size)
{
gnutls_datum data;
gnutls_datum key = { session_id, session_id_size };
int ret;

	data = _gnutls_retrieve_session( state, key);

	if (data.data==NULL) {
		gnutls_assert();
		return GNUTLS_E_INVALID_SESSION;
	}

	/* expiration check is performed inside */
	ret = gnutls_session_set_data( state, data.data, data.size);
	if (ret < 0) {
		gnutls_assert();
	}
	
	/* Note: Data is not allocated with gnutls_malloc
	 */
	free(data.data);

	return 0;
}

int _gnutls_db_remove_session( GNUTLS_STATE state, uint8* session_id, int session_id_size)
{
gnutls_datum key = { session_id, session_id_size };

	return _gnutls_remove_session( state, key);
}


/* Checks if both db_store and db_retrieve functions have
 * been set up.
 */
static int _gnutls_db_func_is_ok( GNUTLS_STATE state) {
	if (state->gnutls_internals.db_store_func!=NULL &&
		state->gnutls_internals.db_retrieve_func!=NULL &&
		state->gnutls_internals.db_remove_func!=NULL) return 0;
	else return GNUTLS_E_DB_ERROR;
}



/* Stores session data to the db backend.
 */
int _gnutls_store_session( GNUTLS_STATE state, gnutls_datum session_id, gnutls_datum session_data)
{
#ifdef HAVE_LIBGDBM
GDBM_FILE dbf;
datum key = { session_id.data, session_id.size };
datum content = {session_data.data, session_data.size};
#endif
int ret = 0;

	if (state->gnutls_internals.resumable==RESUME_FALSE) {
		gnutls_assert();
		return GNUTLS_E_INVALID_SESSION;
	}
	
	if (GNUTLS_DBNAME==NULL && _gnutls_db_func_is_ok(state)!=0) {
		return GNUTLS_E_DB_ERROR;
	}
	
	if (session_id.data==NULL || session_id.size==0) {
		gnutls_assert();
		return GNUTLS_E_INVALID_SESSION;
	}
	
	if (session_data.data==NULL || session_data.size==0) {
		gnutls_assert();
		return GNUTLS_E_INVALID_SESSION;
	}
	/* if we can't read why bother writing? */

#ifdef HAVE_LIBGDBM
	if (GNUTLS_DBF!=NULL) { /* use gdbm */
		dbf = gdbm_open(GNUTLS_DBNAME, 0, GDBM_WRITER, 0600, NULL);
		if (dbf==NULL) {
			/* cannot open db for writing. This may happen if multiple
			 * instances try to write. 
			 */
			gnutls_assert();
			return GNUTLS_E_AGAIN;
		}
		ret = gdbm_store( dbf, key, content, GDBM_INSERT);
		if (ret<0) {
			gnutls_assert();
		}
		gdbm_close(dbf);

		return 0; /*GNUTLS_E_UNIMPLEMENTED_FEATURE;*/
	}
	else 
#endif
		if (state->gnutls_internals.db_store_func!=NULL)
			ret = state->gnutls_internals.db_store_func( state->gnutls_internals.db_ptr, session_id, session_data);


	return (ret == 0 ? ret : GNUTLS_E_DB_ERROR);

}

/* Retrieves session data from the db backend.
 */
gnutls_datum _gnutls_retrieve_session( GNUTLS_STATE state, gnutls_datum session_id)
{
#ifdef HAVE_LIBGDBM
datum key = { session_id.data, session_id.size };
datum content;
#endif
gnutls_datum ret = { NULL, 0 };

	if (GNUTLS_DBNAME==NULL && _gnutls_db_func_is_ok(state)!=0) {
		gnutls_assert();
		return ret;
	}
	
	if (session_id.data==NULL || session_id.size==0) {
		gnutls_assert();
		return ret;
	}
	
	/* if we can't read why bother writing? */
#ifdef HAVE_LIBGDBM
	if (GNUTLS_DBF!=NULL) { /* use gdbm */
		content = gdbm_fetch( GNUTLS_DBF, key);
		ret.data = content.dptr;
		ret.size = content.dsize;
	} else
#endif
		if (state->gnutls_internals.db_retrieve_func!=NULL)
			ret = state->gnutls_internals.db_retrieve_func( state->gnutls_internals.db_ptr, session_id);

	return ret;

}

/* Removes session data from the db backend.
 */
int _gnutls_remove_session( GNUTLS_STATE state, gnutls_datum session_id)
{
#ifdef HAVE_LIBGDBM
GDBM_FILE dbf;
datum key = { session_id.data, session_id.size };
#endif
int ret = 0;

	if (GNUTLS_DBNAME==NULL && _gnutls_db_func_is_ok(state)!=0) {
		return GNUTLS_E_DB_ERROR;
	}
	
	if (session_id.data==NULL || session_id.size==0)
		return GNUTLS_E_INVALID_SESSION;

	/* if we can't read why bother writing? */
#ifdef HAVE_LIBGDBM
	if (GNUTLS_DBF!=NULL) { /* use gdbm */

		dbf = gdbm_open(GNUTLS_DBNAME, 0, GDBM_WRITER, 0600, NULL);
		if (dbf==NULL) {
			/* cannot open db for writing. This may happen if multiple
			 * instances try to write. 
			 */
			return GNUTLS_E_AGAIN;
		}
		ret = gdbm_delete( dbf, key);

		gdbm_close(dbf);
	} else
#endif
		if (state->gnutls_internals.db_remove_func!=NULL)
			ret = state->gnutls_internals.db_remove_func( state->gnutls_internals.db_ptr, session_id);


	return (ret == 0 ? ret : GNUTLS_E_DB_ERROR);

}

/**
  * gnutls_db_remove_session - This function will remove the current session data from the db
  * @state: is a &GNUTLS_STATE structure.
  *
  * This function will remove the current session data from the session
  * database. This will prevent future handshakes reusing these session
  * data. This function should be called if a session was terminated
  * abnormaly.
  *
  **/
void gnutls_db_remove_session(GNUTLS_STATE state) {
	/* if the session has failed abnormally it has 
	 * to be removed from the db 
	 */
	_gnutls_db_remove_session( state, state->security_parameters.session_id, state->security_parameters.session_id_size);
}
